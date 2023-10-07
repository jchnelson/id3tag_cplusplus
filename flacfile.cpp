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
    ret.push_back(sz >> 16);
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
    ret.push_back(sz >> 24);
    
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
    qInfo() << char(header[0]) << char(header[1]) 
            << char(header[2]) << char(header[3]); // should print f L a C
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
    qInfo() << expected_comments << "comments expected, actual number was" << ret.size();
    
    return ret;  
}

bool FlacFile::write_qtags()
{
    vector<QString> rejoined;
    for (const auto& qtag : QTags)
    {
        rejoined.push_back(qtag.first + '=' + qtag.second);
    }
    // number of comments bytes(4) + vendor string (captured with size bytes)
    size_t tagsum = 4 + vcomment_vendorstring.size();
    for(const auto& p : rejoined)
        tagsum+= 4 + p.size(); // comment size bytes + comment
    
    // if comment block is smaller, do normally,
    // if comment block is bigger, but there's room in the padding block
    // make a smaller padding block if the extra space is enough 
    // if the extra space isn't enough, rewrite entire file.  
    
    bool has_padding_block = metablocks.count(1) != 0;
    size_t padding_size = has_padding_block ? metablocks.at(1).size() : 0; 
    // padding block bytes + padding block if present in file
    
    if (has_padding_block)
        qInfo() << "padding size" << padding_size;
    else
        qInfo() << "no padding block";
    
    auto flacpath = fs::path(filename.toStdString());
    string outname = flacpath.filename().string();
    string filedir = QTags.at("ALBUM").toStdString();
    string outrel = filedir + "\\" + outname;
    fs::create_directories(filedir);
    
    if (metablocks.at(4).size() >= tagsum ||
            metablocks.at(4).size() + padding_size + 4 >= tagsum) 
            // if second part, padding will be cannibalized
        // overwrite header, maintain size
    {
        // we will need to make the padding block LARGER here, or create one
        // if it doesn't exist.
        qInfo() << "it fits!" << metablocks.size() << "blocks";

        qInfo() << tagsum << " not bigger than original" << metablocks.at(4).size();
        
        fs::copy(flacpath, outrel);
        std::fstream biob(outrel, std::ios_base::binary
                          | std::ios_base::out | std::ios_base::in);
        biob.seekp(42, std::ios_base::beg);
        
        int blocks_written = 0;
        
        if (metablocks.count(3) != 0) // write application block first
        {
            biob << byte(0x03);
            for ( const auto& ch : make_3be(metablocks.at(3).size()))
                biob << ch;
            for ( const auto& ch : metablocks.at(3) )     // it exists
                biob << ch;
            ++blocks_written;
        }
        
        // write comment block next
        if (metablocks.size() > 2) // then there's another block after this
            biob << byte(0x04);
        else
            biob << byte(0x04 | 128); // signal last block
        for ( const auto& ch : make_3be(tagsum) )
            biob << ch;
        for ( const auto& ch : vcomment_vendorstring)
            biob << ch;
        for ( const auto& ch : make_4le(rejoined.size()))
            biob << ch;
        for ( const auto& tag : rejoined )
        {
            for ( const auto& ch : make_4le(tag.size()))
                biob << ch;
            for (const auto& ch : tag.toStdString())
                biob << ch;
        }
        size_t size_difference = metablocks.at(4).size() - tagsum;
        // this is the number of bytes that will need to be added to padding block
        // if the padding block doesn't exist and this is less than 4...
        // might have to cheat and add a space or two in a tag lmao
        bool oh_no = size_difference <= 4 && metablocks.count(1) == 0;
        if (oh_no)
            throw std::runtime_error("I regret my decision"); 
        bool make_padding = metablocks.count(1) == 0 && size_difference > 4;
        ++blocks_written;
        
        for (int i = 6; i != 1; --i)
        {
            
            // consider creating logic to allow for removal of pictures
            // since they really shouldn't be embedded in flac files.
            if (metablocks.count(i) == 0 || i == 4 || i == 3)
                continue;  // no block of this type or we already wrote it
            else if (blocks_written + 1 == metablocks.size() && 
                     metablocks.count(1) == 0 && !make_padding) 
                biob << byte(i | 128); // signal last block
            else
                biob << byte(i); // then there's another block after this
            
            for ( const auto& ch : make_3be(metablocks.at(i).size()))
                biob << ch;
            for ( const auto& ch : metablocks.at(i) )     // it exists
                biob << ch;
            ++blocks_written;
        }
        
        
        // handling the padding seems more complex than it needs to be
        // but it seems worth avoiding the performance hit from 
        // rewriting the entire file. 
        
        size_t origsize = metablocks.at(4).size();
        
        if (origsize >= tagsum)
        {
            vector<byte> newpadding;
            if (metablocks.count(1) != 0)
            {
                if (size_difference > 0)
                {
                    newpadding = vector<byte>{metablocks.at(1)};
                    newpadding.insert(newpadding.end(), 
                                      size_difference, byte(0x00));
                }
                else  // then it's 0, or we wouldn't be in this block
                    newpadding = metablocks.at(1);
                biob << byte(1 | 128);
                for ( const auto& ch : make_3be(newpadding.size()))
                    biob << ch;
                for (const auto& ch : newpadding)
                    biob << ch;
            }
            else if (size_difference > 4) // we need space for padding header at least
            {
                biob << byte(0x01 | 128);
                size_t padding_chars = size_difference - 4;
                for (const auto& ch : make_3be(padding_chars))
                    biob << ch;
                for (size_t i = 0; i != padding_chars; ++i)
                    biob << byte(0x00);
            }
        }
        else if (origsize < tagsum &&
                origsize + padding_size + 4 >= tagsum)
        {
            // check if the tagsize allows for padding still
            size_difference = (padding_size + 4 + origsize) - tagsum;
            // if this size_difference is less than four, that's 
            // also difficult to handle
            if (size_difference > 4)
            {
                biob << byte(0x01 | 128);
                size_t padding_chars = size_difference - 4;
                for (const auto& ch : make_3be(padding_chars))
                    biob << ch;
                for (size_t i = 0; i != padding_chars; ++i)
                    biob << byte(0x00);
            }     
            else
                throw std::runtime_error("Goddamn it");
        }
        return true;
        
    } 
//    else  // not enough space for comment block, rewrite entire file
//    {
//        // binary out bucket -- bob
//        std::ofstream bob(outrel, std::ios_base::binary);
        
//        bob << 'I' << 'D' << '3' << byte(0x03) << byte(0x00)
//            << byte(0x00); // "ID3" and version bytes (ID3v2.3.0)
//        // add up all the sizes of the tags and pass result to get_id3_size
    
//        auto sizebytes = get_id3_size(tagsum);
//        for (const auto& ch : sizebytes)
//            bob << ch;
//        for (const auto& p : QTags)
//        {
//            for (const auto& ch : p.first.toStdString())
//                bob << ch;
            
//            // four size bytes
//            size_t size = (p.second.size() * 2) + 3;
//            if (size <= 255)
//            {
//                bob << byte(0x00)<< byte(0x00)<< byte(0x00);
//                bob << static_cast<byte>(size);
//            }
//            else if (size <= 65535)
//            {
//                bob << byte(0x00)<< byte(0x00);
//                bob << static_cast<byte>(size >> 8) << byte(0xFF);
//            }
//            else 
//                throw std::out_of_range("ID3 tag too big");
//                // again, if it's that big there's something wrong
            
//            bob << byte(0x00) << byte(0x00); // two flag bytes
//            bob << byte(0x01) << byte(0xFF) << byte(0xFE); // UTF-16LE
//            for (const auto& ch : p.second.toStdString())
//            {
//                bob << ch;
//                bob << byte(0x00);
//            }    
//        }

//        std::ostream_iterator<byte> bob_it(bob);
        
//        std::ifstream mediafile{ filename.toStdString(), 
//                                 std::ios_base::binary };
//        noskipws(mediafile);
//        std::istream_iterator<byte> infile{mediafile};
//        std::advance(infile, id3_orig+10);
//        std::copy_n(infile, remaining_filesize, bob_it);
    
//        for (int i = 0; i != 127; ++i)
//            bob << byte(0x00);
//        bob << byte(0xFF);
//    }
    return true;
}