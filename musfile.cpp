#include <iostream>
#include <ios>
#include <iomanip>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include "musfile.h"

typedef unsigned char byte;
using std::vector;
using std::string;
using std::map;

extern std::map<std::string, std::string> standard_tags;

vector<byte> MusFile::make_filebytes()
{
    std::ifstream mediafile{ filename.toStdString(), std::ios_base::binary};
    noskipws(mediafile);
    std::istream_iterator<byte> infile{mediafile}, eof;
    vector<byte> ret;
    std::copy_n(infile, 10, std::back_inserter(ret));
    ++infile;
    // seventh byte ([6]) is always where size bytes begin, after "ID3", 
    // two version bytes, and one flag byte.  size bytes ignore most 
    // significant bit of each byte. this equation provided by ID3 standard to 
    // easily calculate the size of the ID3 header from these four bytes
    double id3_length = ret[6] * pow(2, 21) + ret[7] * pow(2, 14) +
        ret[8] * pow(2, 7) + ret[9] * (1);
    std::copy_n(infile, static_cast<__int64>(id3_length), std::back_inserter(ret));
    ++infile;
    filebytes.assign(infile, eof);  // assign remaining bytes to filebytes
    auto fb = filebytes.end() - 128;  // set iterator to last 128 bytes
    for (fb; fb != filebytes.end();)  // and zero those ID3v1 bytes out
        fb = filebytes.erase(fb);
    return ret;
}

vector<byte> MusFile::get_tag()
{
    // get tag size from bytes 5-8, return vector of chars to end of tag
    int tagsize = int( filepos[4] << 24 | filepos[5] << 16 |
                       filepos[6] << 8  | filepos[7] );
    if (tagsize == 0)
        return vector<byte>();
    return vector<byte>(filepos, filepos+tagsize+10);  // 
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
            ret.push_back(next_tag);
        }
    }
    return ret;
}

map<vector<byte>, vector<byte>> MusFile::make_tagmap()
{
    map<vector<byte>, vector<byte>> tagmap;
    for (int i = 0; i != bintags.size(); ++i)
    {
        vector<byte> tagtype;
        vector<byte> tag;
        for (int j = 0; j != 4; ++j)
            tagtype.push_back(bintags[i][j]);

        /*
        for (int j = 4; j != bintags[i].size(); ++j)
        {
            tag.push_back(bintags[i][j]);
        }
        tagmap.insert({ tagtype, tag });
        */
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
        tagmap.insert({ tagtype, tag });
    }
    return tagmap;
}

std::map<QString, QString> MusFile::make_qtags()
{
    map<QString, QString> qret;
    for (const auto& elem : tagmap)
    {
        QString tagtype;
        QString tag;
        for (const auto& b: elem.first)
        {
            tagtype.append(static_cast<const char>(b));
        }
        for (const auto& b: elem.second)
        {
            tag.append(static_cast<const char>(b));
        }
        
        qret.insert({ tagtype, tag });
    }
    return qret;
}


std::ostream& print_vecbyte(std::ostream& os, std::vector<byte> vb)
{
    for (const byte& b : vb)
        os << b;
    os << '\n';
    for (const byte& b: vb)
        os << static_cast<int>(b) << " ";
    return os;
}

std::ostream& MusFile::print_tags(std::ostream& os) const
{
    for (const auto& elem : tagmap)
    {
        string tagtype;
        for (int i = 0; i != elem.first.size(); ++i)
            tagtype += elem.first[i];

        string tagtext;
        for (int i = 0; i != elem.second.size(); ++i)
            if (isalnum(elem.second[i]) || isspace(elem.second[i]))
                tagtext += elem.second[i];


        os << std::left << std::setw(30) << std::setfill('-') << standard_tags.at(tagtype);
        os << std::right << std::setw(50) << std::setfill('-') << tagtext << '\n';

    }
    return os;
}

void MusFile::reassemble()
{
    std::ofstream bob("output.mp3", std::ios_base::binary);
    for (const auto& ch : tagbytes)
        bob << ch;
    for (const auto& ch : filebytes)
        bob << ch;
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

void MusFile::change_tag(const string& tag, const string& newtag) 
{
    auto tagloc = tagmap.find(std::vector<byte>(tag.begin(), tag.end()));
    tagloc->second.clear();
    
    tagloc->second.push_back(0x00);
    tagloc->second.push_back(0x00);
    tagloc->second.push_back(0x00);
    // I'm assuming I'm not writing a tag greater than 255 length for now,
    // so three zeroed-out bytes for the first three ID3 frame size bytes

    byte size = static_cast<byte>(newtag.size()) * 2 + 3; // characters and their nulls, and 
                                       // unicode byte order mark
    tagloc->second.push_back(size);

    tagloc->second.push_back(0x00);  // the two flag bytes
    tagloc->second.push_back(0x00);
    
    tagloc->second.push_back(0x01); // unicode byte order mark
    tagloc->second.push_back(0xFF);
    tagloc->second.push_back(0xFE);

    for (const auto& ch : newtag)  // size bytes not inserted
    {
        tagloc->second.push_back(ch);
        tagloc->second.push_back(0x00);
    }
}

bool MusFile::write_tags()
{
    std::ofstream bob("output.mp3", std::ios_base::binary);
    bob << 'I' << 'D' << '3' << byte(0x03) << byte(0x00)
        << byte(0x00);  // "ID3" and version bytes
    // add up all the sizes of the tags and pass result to get_id3_size
    int tagsum = 0;
    for(const auto& p : tagmap)
        tagsum+= static_cast<int>(p.first.size() + p.second.size());
    auto sizebytes = get_id3_size(tagsum);
    for (const auto& ch : sizebytes)
        bob << ch;
    for (const auto& p : tagmap)
    {
        for (const auto& ch : p.first)
            bob << ch;
        for (const auto& ch : p.second)
            bob << ch;
    }
    for (const auto& ch : filebytes)
        bob << ch;
    for (int i = 0; i != 127; ++i)
        bob << byte(0x00);
    bob << byte(0xFF);
    return true;
}

bool MusFile::write_qtags()
{
    // binary out bucket -- bob
    std::ofstream bob("output.mp3", std::ios_base::binary);
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
    for (const auto& ch : filebytes)
        bob << ch;
    for (int i = 0; i != 127; ++i)
        bob << byte(0x00);
    bob << byte(0xFF);
    return true;
}