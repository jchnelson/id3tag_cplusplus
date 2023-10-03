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



class MusFile
{
public:
    typedef unsigned char byte;
    explicit MusFile(const QString& s)
        : filename(s) { }
    const std::vector<std::vector<byte>> 
         show_bintags() const { return bintags; }
    const std::map<std::vector<byte>, std::vector<byte>>& 
        show_tagmap() const { return tagmap; }
    std::map<QString, QString>& show_qtags() { return QTags; }
    std::ostream& print_tags(std::ostream&) const;
    void reassemble();
    void change_tag(const std::string&, const std::string&);
    bool write_tags();
    bool write_qtags();


private:
    QString filename;
    std::vector<byte> filebytes;
    std::vector<byte> make_filebytes();
    std::vector<byte> tagbytes = make_filebytes();
    std::vector<byte>::iterator filepos = tagbytes.begin() + 10;
    std::vector<byte> get_tag(); 
    std::vector<std::vector<byte>> maketags();
    std::vector<std::vector<byte>> bintags = maketags();
    std::map<std::vector<byte>, std::vector<byte>> make_tagmap();
    std::map<std::vector<byte>, std::vector<byte>> tagmap = make_tagmap();
    std::vector<byte> get_id3_size(int);
    std::map<QString, QString> make_qtags();
    std::map<QString, QString> QTags = make_qtags();
};

std::ostream& print_vecbyte(std::ostream& os, std::vector<MusFile::byte> vb);

