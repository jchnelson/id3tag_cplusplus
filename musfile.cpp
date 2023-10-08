#include <iostream>
#include <ios>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <filesystem>
#include <QDebug>
#include <QString>
#include <QMessageBox>
#include <QApplication>
#include "musfile.h"

typedef unsigned char byte;
using std::vector;
using std::string;
using std::map;
namespace fs = std::filesystem;

vector<byte> MusFile::make_filebytes()
{
    std::ifstream mediafile{ filename.toStdString(), 
                             std::ios_base::binary };
    noskipws(mediafile);
    std::istream_iterator<byte> infile{mediafile}, eof;
    
    vector<byte> ret;
    std::copy_n(infile, 10, std::back_inserter(ret));
    ++infile;
    
    // ID3 header size bytes(4) begin after "ID3", two version bytes, and 
    // one flag byte. size bytes ignore most significant bit of each byte.
    size_t id3_length = ret[6] * pow(2, 21) + ret[7] * pow(2, 14) +
        ret[8] * pow(2, 7) + ret[9] * (1);
    std::copy_n(infile, static_cast<__int64>(id3_length), 
                        std::back_inserter(ret));
    id3_orig = id3_length;
    auto fsize = fs::file_size(fs::path(filename.toStdString()));
    remaining_filesize = fsize - id3_length - 128;
    return ret;
}

vector<byte> MusFile::get_tag()
{
    // get 4-byte int(BE) for tag size 
    int tagsize = int( filepos[4] << 24 | filepos[5] << 16 |
                       filepos[6] << 8  | filepos[7] );
    if (tagsize == 0)
        return vector<byte>();
    vector<byte> ret;
    ret.reserve(tagsize+10);  // ten bytes for tag header
    ret.assign(filepos, filepos+tagsize+10);
    return ret;
}




vector<vector<byte>> MusFile::maketags()
{
    vector<vector<byte>> ret;
    vector<byte> next_tag{1}; // initial value just so it's not empty
    while (!next_tag.empty())
    { 
        next_tag = get_tag();
        if (!next_tag.empty())
        { 
            filepos += next_tag.size();
            ret.reserve(next_tag.size());
            ret.push_back(next_tag);
        }
    }
    if (ret.size() == 0)
        throw std::runtime_error("No tags ID3v2 tags found in file");
    return ret;
}





std::vector<byte> MusFile::get_id3_size(int i)
{  
    if (i <= 127) // 7 places max value
        return std::vector<byte>({0,0,0,static_cast<byte>(i)});
    else if (i <= 16383) // 14 places max
    {
        auto i2 = i;
        auto byte1 = static_cast<byte>(i2 & 127);
        i2 = i; 
        auto byte2 = static_cast<byte>((i2 & 16256) >> 7); 
        return std::vector<byte>({0,0,byte2,byte1});
    }
    else if (i <= 2097151) // 21 places max
    {
        auto i2 = i;
        auto byte1 = static_cast<byte>(i2 & 127);
        i2 = i;
        auto byte2 = static_cast<byte>((i2 & 16256) >> 7);
        i2 = i;
        auto byte3 = static_cast<byte>((i2 & 2080768) >> 14);
        return std::vector<byte>({ 0,byte3,byte2,byte1 });
    }
    else
        throw std::out_of_range("ID3 header too large");
        // it could technically be larger (28 significant bits), but 
        // something most likely went wrong if the header is that large
}

std::map<QString, QString> MusFile::make_qtags()
{
    // go straight from bintags, which is a vector of byte vectors,
    // to qtags, a map of QString, QString pairs
    map<QString, QString> tagmap;
    for (int i = 0; i != bintags.size(); ++i)
    {
        string tagtype;
        string tag;
        for (int j = 0; j != 4; ++j)
            tagtype.push_back(bintags[i][j]);
        

        for (int j = 10; j != bintags[i].size(); ++j)
        {
            if (bintags[i][j] == 1)
            {
                // 1 indicates UTF-16, FF FE for little endian
                j += 2; // skip three bytes, two and then
                continue; // this one
            }
            else if (bintags[i][j] == 255)
            {
                // stray byte order mark not at beginning of tag
                j+= 1; // skip two bytes, one and then
                continue; // this one
            }
            else if (bintags[i][j] == 0)
                continue; // also skip zero padding
            tag.push_back(bintags[i][j]);
        }
        tagmap.insert({ QString::fromStdString(tagtype),
                        QString::fromStdString(tag) });
    }
    return tagmap;    
} 



bool MusFile::write_qtags()
{
    
    int tagsum = 0;
    for(const auto& p : QTags)
        tagsum+= 10 + 3 + (p.second.size() * 2);
        // tag type (4) + ( tag length * 2(zeroes) ) + 3 (BOM) + 2 (flags) 
    // add back in zeroes and encoding-type byte + order mark (2 bytes)
    
    
    auto mp3path = fs::path(filename.toStdString());
    string outname = mp3path.filename().string();
    string filedir = QTags.at("TALB").toStdString();
    
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
    
    if (id3_orig >= tagsum)  // overwrite id3 header, maintain size
    {
        fs::copy(mp3path, outrel);
        std::fstream biob(outrel, std::ios_base::binary
                          | std::ios_base::out | std::ios_base::in);
        biob.seekp(10, std::ios_base::beg);
        
        for (const auto& p : QTags)
        {
            for (const auto& ch : p.first.toStdString())
                biob << ch;
            
            // four size bytes
            size_t size = (p.second.size() * 2) + 3;
            if (size <= 255)
            {
                biob << byte(0x00)<< byte(0x00)<< byte(0x00);
                biob << static_cast<byte>(size);
            }
            else if (size <= 65535)
            {
                biob << byte(0x00)<< byte(0x00);
                biob << static_cast<byte>(size >> 8) << byte(0xFF);
            }
            else 
                throw std::out_of_range("ID3 tag too big");
                // again, if it's that big there's something wrong
            
            biob << byte(0x00) << byte(0x00); // two flag bytes
            biob << byte(0x01) << byte(0xFF) << byte(0xFE); // UTF-16LE
            for (const auto& ch : p.second.toStdString())
            {
                biob << ch;
                biob << byte(0x00);
            }    
        }
        
        auto size_difference = id3_orig - tagsum;
        for (int i = 0; i != size_difference; ++i)
            biob << byte(0x00);
        biob.seekp(128, std::ios_base::end);
        for (int i = 0; i != 127; ++i)
            biob << byte(0x00);
        biob << byte(0xFF);
        return true;
    }
    else  // not enough space for new ID3 header, rewrite entire file
    {
        // binary out bucket -- bob
        std::ofstream bob(outrel, std::ios_base::binary);
        
        bob << 'I' << 'D' << '3' << byte(0x03) << byte(0x00)
            << byte(0x00); // "ID3" and version bytes (ID3v2.3.0)
        // add up all the sizes of the tags and pass result to get_id3_size
    
        auto sizebytes = get_id3_size(tagsum);
        for (const auto& ch : sizebytes)
            bob << ch;
        for (const auto& p : QTags)
        {
            for (const auto& ch : p.first.toStdString())
                bob << ch;
            
            // four size bytes
            size_t size = (p.second.size() * 2) + 3;
            if (size <= 255)
            {
                bob << byte(0x00)<< byte(0x00)<< byte(0x00);
                bob << static_cast<byte>(size);
            }
            else if (size <= 65535)
            {
                bob << byte(0x00)<< byte(0x00);
                bob << static_cast<byte>(size >> 8) << byte(0xFF);
            }
            else 
                throw std::out_of_range("ID3 tag too big");
                // again, if it's that big there's something wrong
            
            bob << byte(0x00) << byte(0x00); // two flag bytes
            bob << byte(0x01) << byte(0xFF) << byte(0xFE); // UTF-16LE
            for (const auto& ch : p.second.toStdString())
            {
                bob << ch;
                bob << byte(0x00);
            }    
        }
        
        std::ifstream mediafile{ filename.toStdString(), 
                                 std::ios_base::binary };
        noskipws(mediafile);
        mediafile.ignore(id3_orig+10); // position after original header
        
        std::vector<char> filebucket;
        filebucket.reserve(remaining_filesize);
        filebucket.assign(remaining_filesize,0);
        
        mediafile.read(filebucket.data(), filebucket.size());
        bob.write(filebucket.data(), filebucket.size());
        
        vector<char> zeroes;
        zeroes.reserve(127);
        zeroes.assign(127, 0);
        
        bob.seekp(128, std::ios_base::end);
        bob.write(zeroes.data(), 127);
        bob << byte(0xFF);

        return true;
    }
}
