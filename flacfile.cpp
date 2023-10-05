#include "flacfile.h"
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


using std::vector; 
namespace fs = std::filesystem;

int get_4le_advance(std::vector<byte>::iterator& it)
{
    int ret =  (*it) | (*(it + 1)) << 8 | (*(it + 2)) << 16 | (*(it + 3)) << 24;
    it += 4;
    return ret;
}

std::map<byte, vector<byte>> FlacFile::make_blocks()
{
    std::ifstream mediafile(filename, std::ios_base::binary);
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
    auto fsize = fs::file_size(fs::path(filename));
    remaining_filesize = fsize - full_headersize;
    return metablocks; 
    
}

std::vector<QString> FlacFile::make_vcomments()
{
    auto comment_block = metablocks.at(4);
    
    // extract vendor string
    int vendor_length = (comment_block[0] | comment_block[1] << 8 |
                        comment_block[2] << 16 | comment_block[3] << 24) + 4;
    vector<byte> vcomment_vendorstring;
    copy_n(comment_block.begin(), vendor_length, 
           std::back_inserter(vcomment_vendorstring));
    
    // create vector with vorbis comments
    vector<QString> ret;
    auto comment_pos = comment_block.begin() + vendor_length; 
    int expected_comments = get_4le_advance(comment_pos);
    while (comment_pos != comment_block.end())
    {
        int comment_length = get_4le_advance(comment_pos);
        std::string comment(comment_pos, comment_pos + comment_length);
        ret.emplace_back(QString::fromStdString(comment));
        comment_pos += comment_length;
    }
    qInfo() << expected_comments << "comments expected, actual number was" << ret.size();
    
    return ret;  
}

bool FlacFile::write_qtags()
{
    return true;
}