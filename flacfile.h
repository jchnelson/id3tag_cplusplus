#ifndef FLACFILE_H
#define FLACFILE_H

#include <vector>
#include <string>
#include <map>
#include <QString>
#include "audiofile.h"

typedef unsigned char byte;
extern std::map<QString, QString> vorbis_qtags;

class FlacFile : public AudioFile
{
public:
    explicit FlacFile(const QString& qs)
        : AudioFile(), filename(qs) { }
    std::map<QString, QString>& get_qtags() { return QTags; }
    std::map<QString, QString> get_standard() { return vorbis_qtags; }
private:
    QString filename;
    std::vector<byte> header;
    std::vector<byte> vcomment_vendorstring;
    uintmax_t remaining_filesize;
    size_t full_headersize;
    std::map<byte, std::vector<byte>> make_blocks();
    std::map<byte, std::vector<byte>> metablocks = make_blocks();
    std::map<QString, QString> make_vcomments();
    std::map<QString, QString> QTags = make_vcomments();

    
public:
    bool write_qtags();
    
};

#endif // FLACFILE_H
