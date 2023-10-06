#ifndef AUDIOFILE_H
#define AUDIOFILE_H

#include <QString>
#include <QLineEdit>
#include <QProgressBar>


class AudioFile
{
public:
    AudioFile();
    virtual std::map<QString, QString>& get_qtags() = 0;
    virtual bool write_qtags() = 0;
    virtual void save_write_tags(std::map<QString, QLineEdit*>& lines) = 0;
    virtual void save_write_folder(std::vector<AudioFile*>&, 
                           std::map<QString, QLineEdit*>&,
                           std::map<QString, QString>&,
                           QProgressBar*) = 0;
    virtual ~AudioFile() = default;
};

#endif // AUDIOFILE_H
