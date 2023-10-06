#ifndef FLACFILE_H
#define FLACFILE_H

#include <vector>
#include <string>
#include <map>
#include <QString>
#include "audiofile.h"

typedef unsigned char byte;

class FlacFile : public AudioFile
{
public:
    explicit FlacFile(const QString& qs)
        : AudioFile(), filename(qs) { }
    std::map<QString, QString>& get_qtags() { return QTags; }
private:
    QString filename;
    std::vector<byte> header;
    uintmax_t remaining_filesize;
    size_t full_headersize;
    std::map<byte, std::vector<byte>> make_blocks();
    std::map<byte, std::vector<byte>> metablocks = make_blocks();
    std::map<QString, QString> make_vcomments();

    
public:
    std::map<QString, QString> QTags = make_vcomments();
    bool write_qtags();
    void save_write_tags(std::map<QString, QLineEdit*>& lines);
    void save_write_folder(std::vector<AudioFile*>&, 
                           std::map<QString, QLineEdit*>&,
                           std::map<QString, QString>&,
                           QProgressBar*);
    
};

#endif // FLACFILE_H
