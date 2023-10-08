#include <vector>
#include <deque>
#include <map>
#include <fstream>
#include <ios>
#include <iterator>
#include <algorithm>
#include <filesystem>
#include <QDebug>
#include <QString>
#include <string>
#include "flacfile.h"


using std::vector; 
using std::string;
namespace fs = std::filesystem;

vector<byte> make_3be(size_t sz)
{
    vector<byte> ret;
    ret.push_back(static_cast<byte>(sz >> 16));
    ret.push_back((sz >> 8) & 255);
    ret.push_back(sz & 255);
    return ret;
}

vector<byte> make_4le(size_t sz)
{
    vector<byte> ret;
    ret.push_back(sz & 255);
    ret.push_back((sz >> 8) & 255);
    ret.push_back((sz >> 16) & 255);
    ret.push_back(static_cast<byte>(sz >> 24));
    
    return ret;
}

int get_4le_advance(std::vector<byte>::iterator& it)
{
    int ret =  (*it) | (*(it + 1)) << 8 | (*(it + 2)) << 16 | (*(it + 3)) << 24;
    it += 4;
    return ret;
}

std::map<byte, vector<byte>> FlacFile::make_blocks()
{
    std::ifstream mediafile(filename.toStdString(), std::ios_base::binary);
    noskipws(mediafile);
    std::istream_iterator<byte> infile(mediafile);
    header.reserve(42);
    copy_n(infile, 42, std::back_inserter(header));
    ++infile;
    
    std::map<byte, vector<byte>> metablocks;
    while (1)
    {
        byte blockinfo[4]{};
        copy_n(infile, 4, blockinfo);
        ++infile;
        bool lastblock = blockinfo[0] >> 7;  // set bit here indicates final block
        byte blockbyte = blockinfo[0] & 0b01111111; // reset first bit if set   
        int blocksize = blockinfo[1] << 16 | blockinfo[2] << 8 | blockinfo[3];
        vector<byte> block;
        block.reserve(blocksize);
        copy_n(infile, blocksize, std::back_inserter(block));
        ++infile;
        metablocks.insert({blockbyte, block});
        if (lastblock)
            break;
    }
    full_headersize = mediafile.tellg();
    auto fsize = fs::file_size(fs::path(filename.toStdString()));
    remaining_filesize = fsize - full_headersize;
    if (metablocks.count(4) == 0)
        throw std::runtime_error("No vorbis comments present in file!");
    return metablocks; 
    
}

std::map<QString, QString> FlacFile::make_vcomments()
{
    auto comment_block = metablocks.at(4);
    
    // extract vendor string
    int vendor_length = (comment_block[0] | comment_block[1] << 8 |
                        comment_block[2] << 16 | comment_block[3] << 24) + 4;
    copy_n(comment_block.begin(), vendor_length, 
           std::back_inserter(vcomment_vendorstring));

    std::map<QString, QString> ret;
    auto comment_pos = comment_block.begin() + vendor_length; 
    int expected_comments = get_4le_advance(comment_pos);
    while (comment_pos != comment_block.end())
    {
        int comment_length = get_4le_advance(comment_pos);
        std::string comment(comment_pos, comment_pos + comment_length);
        QString qcomment = QString::fromStdString(comment);
        size_t eq = comment.find("=", 0);
        QString first = qcomment.sliced(0,eq).toUpper(); // text before
        QString last = qcomment.sliced(eq+1);  // and after '='
        ret.insert({first,last});
        comment_pos += comment_length;
    }
    if (ret.size() != expected_comments)
        throw std::runtime_error("Unexpected number of vorbis comments");
    return ret;  
}

void writevec(std::fstream& fs, const vector<byte>& vb)
{
    for (const auto& ch : vb)
        fs << ch;
}


bool FlacFile::write_qtags()
{
    vector<QString> rejoined;
    for (const auto& qtag : QTags)
    {
        rejoined.push_back(qtag.first + '=' + qtag.second);
    }
    
    size_t origsize = metablocks.at(4).size();
    bool has_padding_block = metablocks.count(1) != 0;
    size_t padding_size = has_padding_block ? metablocks.at(1).size() : 0;
    
    // number of comments bytes(4) + vendor string (captured with size bytes)
    size_t tagsum = 4 + vcomment_vendorstring.size();
    for(const auto& p : rejoined)
        tagsum+= 4 + p.size(); // comment size bytes + comment
    
    size_t size_difference;
    bool rewrite_file = false;
    if (origsize >= tagsum)  // there's room 
         size_difference = origsize - tagsum;
    else if (origsize < tagsum &&  // there's room if we take some padding
             origsize + padding_size + 4 >= tagsum)
        size_difference = (padding_size + 4 + origsize) - tagsum;
    else // there's no room
    {
        rewrite_file = true;
        size_difference = 2000; // padding for later
    }
    
    
    if ( size_difference <= 4 ) // extra space, but not enough for padding block
    {
        rewrite_file = true;
        size_difference = 2000;
    }
    
    bool make_padding = (!has_padding_block && size_difference > 4) || rewrite_file;
    
    auto flacpath = fs::path(filename.toStdString());
    string outname = flacpath.filename().string();
    string filedir = QTags.at("ALBUM").toStdString();
    
    // replace forbidden characters for directory with a space
    string no_dir_chars = ">:\"/\\|?*";
    if (filedir.find_first_of(no_dir_chars) != string::npos)
        for (const auto& ch : no_dir_chars)
        {   
            auto index = filedir.find(ch);
            if (index != string::npos)
                filedir[index] = ' ';
        }
    
    string outrel = filedir + "\\" + outname;
    fs::create_directories(filedir);
    
    
    // should be able to remove these size checks
    
    fs::copy(flacpath, outrel);
    std::fstream biob(outrel, std::ios_base::binary
                      | std::ios_base::out | std::ios_base::in);
    biob.seekp(42, std::ios_base::beg);
    
  
    int blocks_written = 0;
    
    if (metablocks.count(3) != 0) // write application block first
    {
        biob << byte(0x03);
        writevec(biob, make_3be(metablocks.at(3).size()));
        writevec(biob, metablocks.at(3));
        ++blocks_written;
    }
    // write comment block next
    if (metablocks.size() > 2)
        biob << byte(0x04);
    else
        biob << byte(0x04 | 128); // signal last block
    writevec(biob, make_3be(tagsum));
    writevec(biob, vcomment_vendorstring);
    writevec(biob, make_4le(rejoined.size()));
            
    for ( const auto& tag : rejoined )
    {
        writevec(biob, make_4le(tag.size()));
        for (const auto& ch : tag.toStdString())
            biob << ch;
    }
    ++blocks_written;
    for (int i = 6; i != 1; --i)
    {
        
        // consider creating logic to allow for removal of pictures
        // since they really shouldn't be embedded in flac files.
        if (metablocks.count(i) == 0 || i == 4 || i == 3)
            continue; 
        else if ( (blocks_written + 1) == metablocks.size() && 
                 !has_padding_block && !make_padding ) 
            biob << byte(i | 128); // signal last block
        else
            biob << byte(i); 
        writevec(biob, make_3be(metablocks.at(i).size()));
        writevec(biob, metablocks.at(i)); // we know the block exists
        ++blocks_written;
    }
    if (origsize >= tagsum)
    {
        vector<byte> newpadding;
        if (has_padding_block)
        {
            if (size_difference > 0)
            {
                if (rewrite_file)  // then size_difference was < 4 earlier
                {
                    newpadding.reserve(2000);
                    newpadding = vector<byte>(2000, byte(0x00));
                }
                else
                {        
                    newpadding = vector<byte>{metablocks.at(1)};
                    newpadding.insert(newpadding.end(), 
                                  size_difference, byte(0x00));
                }
            }
            else 
                newpadding = metablocks.at(1);
            biob << byte(1 | 128);
            writevec(biob, make_3be(newpadding.size()));
            writevec(biob, newpadding);
        }
        else if (size_difference > 4) // we need space for padding header at least
        {
            biob << byte(0x01 | 128);
            size_t padding_chars = size_difference - 4;
            writevec(biob, make_3be(padding_chars));
            for (size_t i = 0; i != padding_chars; ++i)
                biob << byte(0x00);
        }
    }
    else if (origsize < tagsum &&
            origsize + padding_size + 4 >= tagsum)
    {
        if (size_difference > 4)
        {
            biob << byte(0x01 | 128);
            size_t padding_chars = size_difference - 4;
            writevec(biob, make_3be(padding_chars));
            for (size_t i = 0; i != padding_chars; ++i)
                biob << byte(0x00);
        }     
    
    }
    else if (rewrite_file)
    {

        biob << byte(0x01 | 128);
        size_t padding_chars = size_difference - 4;
        vector<char> newpadding(padding_chars, 0x00);
        newpadding.reserve(padding_chars);
        writevec(biob, make_3be(padding_chars));
        biob.write(newpadding.data(), padding_chars);
    }
    
    
    if (!rewrite_file)
        return true;
    else
    {
        std::ifstream mediafile(filename.toStdString(), std::ios_base::binary);
        noskipws(mediafile);
        // std::istream_iterator<byte> infile(mediafile);
        mediafile.ignore(full_headersize-1);
        
        std::vector<char> filebucket;
        filebucket.reserve(remaining_filesize);
        filebucket.assign(remaining_filesize,0);
        
        mediafile.read(filebucket.data(), filebucket.size());
        biob.write(filebucket.data(), filebucket.size());
        
        return true;
    }
}