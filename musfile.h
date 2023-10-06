#pragma once

#include <iostream>
#include <ios>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>
#include <cstddef>
#include <map>
#include <QString>
#include <QLineEdit>
#include <QProgressBar>

#include "audiofile.h"


class MusFile : public AudioFile
{
public:
    typedef unsigned char byte;
    explicit MusFile(const QString& s)
        : AudioFile(), filename(s) { }
    const std::vector<std::vector<byte>> 
         show_bintags() const { return bintags; }
    std::map<QString, QString>& get_qtags() { return QTags; }

private:
    QString filename;
    size_t id3_orig;
    uintmax_t remaining_filesize;
    std::vector<byte> make_filebytes();
    std::vector<byte> tagbytes = make_filebytes();
    std::vector<byte>::iterator filepos = tagbytes.begin() + 10;
    std::vector<byte> get_tag(); 
    std::vector<std::vector<byte>> maketags();
    std::vector<std::vector<byte>> bintags = maketags();
    std::vector<byte> get_id3_size(int);
    std::map<QString, QString> make_qtags();
public:  
    std::map<QString, QString> QTags = make_qtags();
    bool write_qtags();
    void save_write_tags(std::map<QString, QLineEdit*>& lines);
    void save_write_folder(std::vector<AudioFile*>& musfolder, 
                           std::map<QString, QLineEdit*>& lines,
                           std::map<QString, QString>& commontags,
                           QProgressBar* progbar);
    
};

std::ostream& print_vecbyte(std::ostream& os, std::vector<MusFile::byte> vb);

