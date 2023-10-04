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
#include "musfile.h"

typedef unsigned char byte;
typedef std::chrono::milliseconds milli;
typedef std::chrono::system_clock sclock;


using std::vector;
using std::string;
using std::map;
using std::chrono::duration_cast;

void  print_millies(const sclock::time_point& s1, 
                    const sclock::time_point& s2, QString message)
{
    auto millies = duration_cast<milli>(s2 - s1).count() / 1000.0;
    qInfo() << message << millies << "s";
}

namespace fs = std::filesystem;

extern std::map<std::string, std::string> standard_tags;

vector<byte> MusFile::make_filebytes()
{
    std::ifstream mediafile{ filename.toStdString(), 
                             std::ios_base::binary };
    noskipws(mediafile);
    std::istream_iterator<byte> infile{mediafile}, eof;
    
    vector<byte> ret;
    std::copy_n(infile, 10, std::back_inserter(ret));
    ++infile;
    // seventh byte ([6]) is always where size bytes begin, after "ID3", 
    // two version bytes, and one flag byte.  size bytes ignore most 
    // significant bit of each byte. this equation provided by ID3 standard to 
    // easily calculate the size of the ID3 header from these four bytes
    size_t id3_length = ret[6] * pow(2, 21) + ret[7] * pow(2, 14) +
        ret[8] * pow(2, 7) + ret[9] * (1);
    std::copy_n(infile, static_cast<__int64>(id3_length), 
                        std::back_inserter(ret));
    ++infile;
    id3_orig = id3_length;
    auto fsize = fs::file_size(fs::path(filename.toStdString()));
    remaining_filesize = fsize - id3_length - 128;
    //filebytes = infile;
    // maybe store an iterator here instead of copying all the bytes,
    // then use that iterator to copy when writing tags
    //filebytes.reserve(filesize-id3_length);
    //filebytes.assign(infile, eof);  // assign remaining bytes to filebytes
    //filebytes.erase(filebytes.end()-128, filebytes.end()); // erase ID3v1 bytes
    return ret;
}

vector<byte> MusFile::get_tag()
{
    // get tag size from bytes 5-8, return vector of chars to end of tag
    int tagsize = int( filepos[4] << 24 | filepos[5] << 16 |
                       filepos[6] << 8  | filepos[7] );
    if (tagsize == 0)
        return vector<byte>();
    vector<byte> ret;
    ret.reserve(tagsize+10);
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
    return ret;
}





std::vector<byte> MusFile::get_id3_size(int i)
{  // id3 standard -- four bytes for header size, zeroed-out most significant
    // bit for each of those bytes, treated as if that bit doesn't exist for value. 
    if (i <= 127) // 7 places max value
        return std::vector<byte>({0,0,0,static_cast<byte>(i)});
    else if (i <= 16383) // 14 places max
    {
        auto i2 = i;
        auto byte1 = static_cast<byte>(i2 & 127);  // & all ones for first 7 to isolate
        i2 = i; 
        auto byte2 = static_cast<byte>((i2 & 16256) >> 7); // & all ones for next seven
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
        // it could technically be larger (28 bits), but 
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
    // binary out bucket -- bob
    auto mp3path = fs::path(filename.toStdString());
    string outname = mp3path.filename().string();
    string filedir = QTags.at("TALB").toStdString();
    string outrel = filedir + "\\" + outname;
    fs::create_directories(filedir);
    std::ofstream bob(outrel, std::ios_base::binary);
    bob << 'I' << 'D' << '3' << byte(0x03) << byte(0x00)
        << byte(0x00); // "ID3" and version bytes
    // add up all the sizes of the tags and pass result to get_id3_size
    int tagsum = 0;
    for(const auto& p : QTags)
        tagsum+= static_cast<int>(10 + 3 + (p.second.size() * 2));
        // tag type (4) + ( tag length * 2(zeroes) ) + 3 (BOM) + 2 (flags) 
    // add back in zeroes and encoding-type byte + order mark (2 bytes)
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
        bob << byte(0x01) << byte(0xFF) << byte(0xFE);
        for (const auto& ch : p.second.toStdString())
        {
            bob << ch;
            bob << byte(0x00);
        }
        
    }
    std::ostream_iterator<byte> bob_it(bob);
    
    std::ifstream mediafile{ filename.toStdString(), 
                             std::ios_base::binary };
    noskipws(mediafile);
    std::istream_iterator<byte> infile{mediafile};
    std::advance(infile, id3_orig);
    qInfo() << "advance okay";
    std::copy_n(infile, remaining_filesize, bob_it);
    qInfo() << "Copy with bob_it successful";
    //for (const auto& ch : filebytes)
        //bob << ch;
    for (int i = 0; i != 127; ++i)
        bob << byte(0x00);
    bob << byte(0xFF);
    return true;
}