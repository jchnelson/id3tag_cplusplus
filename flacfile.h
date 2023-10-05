#ifndef FLACFILE_H
#define FLACFILE_H

#include <vector>
#include <string>
#include <map>

#include <QString>


typedef unsigned char byte;


// starting reading at byte 42, copy first 42 bytes into preserved header
// 

class FlacFile
{
public:
    explicit FlacFile(const std::string& s)
        : filename(s) { }
    bool write_qtags();
private:
    std::string filename;
    std::vector<byte> header;
    uintmax_t remaining_filesize;
    size_t full_headersize;
    std::map<byte, std::vector<byte>> make_blocks();
    std::map<byte, std::vector<byte>> metablocks = make_blocks();
    std::vector<QString> make_vcomments();

    
public:
    std::vector<QString> vcomments = make_vcomments();
    
};

#endif // FLACFILE_H
