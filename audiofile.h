#ifndef AUDIOFILE_H
#define AUDIOFILE_H

#include <QString>
#include <QLineEdit>
#include <QProgressBar>


class AudioFile
{
public:
    AudioFile() = default;
    virtual std::map<QString, QString>& get_qtags() = 0;
    virtual std::map<QString, QString> get_standard() = 0;
    virtual bool write_qtags() = 0;
    virtual ~AudioFile() = default;
};

#endif // AUDIOFILE_H
