#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>

#include <QApplication>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QMainWindow>
#include <QProgressBar>
#include <QTextEdit>
#include <QLabel>
#include <QDebug>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>

#include "musfile.h"
#include "flacfile.h"

namespace fs = std::filesystem;

void save_write_tags(AudioFile* audio, std::map<QString, QLineEdit*>& lines)
{
    // extract text from each QLineEdit and save to qtags
    for (const auto& line : lines)
    {  
        if (!line.second->text().isEmpty())
            audio->get_qtags().at(line.first) = line.second->text();
    }

    bool success = audio->write_qtags();
    QMessageBox msgBox;
    if (success)
        msgBox.setText("Tags written successfully");
    else
        msgBox.setText("Operation Failed");
    msgBox.exec();
}

void save_write_folder(std::vector<AudioFile*>& audiofolder, 
                       std::map<QString, QLineEdit*>& lines,
                       std::map<QString, QString>& commontags,
                       QProgressBar* progbar)
{
    // extract text from each QLineEdit and save to qtags
    for (const auto& line : lines)
    {  
        if (!line.second->text().isEmpty())
            commontags.at(line.first) = line.second->text();
    }
    for (AudioFile* audio : audiofolder)
        for (auto& tag : commontags)
            audio->get_qtags().at(tag.first) = tag.second;
    
    bool success = all_of(audiofolder.begin(), audiofolder.end(),
                          [&audiofolder, progbar] (AudioFile* audio) 
                           {    progbar->setValue(progbar->value() + 1);
                                QApplication::processEvents();
                                return audio->write_qtags(); } );
    for (auto audio: audiofolder)
        delete audio;
    
    QMessageBox msgBox;
    if (success)
    {
        progbar->setValue(progbar->maximum());
        msgBox.setText("Tags written successfully");
    }
    else
        msgBox.setText("Operation Failed");
    msgBox.exec();
}


void do_folder(QWidget* central)
{
    //  TODO -- allow add/remove of tags 
 
    
    
    QString opendir = QFileDialog::getExistingDirectory(0,
                     "Choose Folder of only MP3 or only FLAC", "C:\\", QFileDialog::ShowDirsOnly);
    fs::path filedir = opendir.toStdString();
    fs::directory_iterator dirit(filedir);
    
    bool mp3_type;
    // allow other non-audio types, there has to be a better way to do this
    std::vector<std::string> filetypes{".jpg", ".jpeg", ".md5", ".cue", ".nfo",
                             ".m3u", ".bmp", ".tiff", ".log", ".txt",
                             ".png", ".crc", ".html"};
    if (std::all_of(begin(dirit), end(dirit), [&] (const fs::directory_entry& entry)
            {       
                std::string ext = entry.path().extension().string();
                for (auto& ch : ext)
                    ch = tolower(ch);
                    
                
                    
                if (find(filetypes.begin(), filetypes.end(), ext) != filetypes.end())
                {  
                    qInfo() << ext;
                    return true;
                }
                else
                    return ext == ".mp3"; 
            } ))
        mp3_type = true;
    else if (std::all_of(begin(dirit), end(dirit), [&] (const fs::directory_entry& entry)
            {       
                std::string ext = entry.path().extension().string();
                for (auto& ch : ext)
                    ch = tolower(ch);
                
                if (find(filetypes.begin(), filetypes.end(), ext) != filetypes.end())
                {  
                    qInfo() << ext;
                    return true;
                }
                else
                    return ext == ".flac"; 
            } ))
        mp3_type = false;
    else
    {
        throw std::runtime_error("Mixed audio types not supported");
    }
    
    std::vector<AudioFile*> audiofolder;
    
    dirit = fs::directory_iterator(filedir); // reinitialize iterator
    for (const auto& p : dirit)
    {
        std::string ext = p.path().extension().string();
        for (auto& ch : ext)
            ch = tolower(ch);
        if (mp3_type)
        {
            if (ext == ".mp3")
                audiofolder.emplace_back(new MusFile(QString::fromStdString(p.path().string())));
            
        }
        else
        {
            if (ext == ".flac")
            audiofolder.emplace_back(new FlacFile(QString::fromStdString(p.path().string())));
        }
    } 
    if (audiofolder.empty())
        throw std::runtime_error("No Files in Directory");
    
    QFormLayout* flayout = new QFormLayout(central);
    std::map<QString, QString> common_tags;
    
    for (const auto& tag : audiofolder[0]->get_qtags())
    {
        if (std::all_of(audiofolder.begin(), audiofolder.end(), [&tag] 
                (AudioFile* audio)
                { if (audio->get_qtags().count(tag.first) != 0)
                    return audio->get_qtags().at(tag.first) == tag.second;
                  else 
                    return false; } ) )
            common_tags.insert(tag);
    }
    
    
    std::map<QString, QLineEdit*> lines;
    
    for (const auto& qs : common_tags)
    {
        QLineEdit* line = new QLineEdit();
        line->setPlaceholderText(qs.second);
        line->setObjectName(qs.first);
        lines.insert({qs.first, line});
        
        flayout->addRow(new QLabel(audiofolder[0]->get_standard().at(qs.first)),
                    line);
           
    }
    QPushButton* goButton = new QPushButton("Save tags");
    flayout->addRow(goButton);
    
    QProgressBar* folderprog = new QProgressBar();
    folderprog->setMaximum(static_cast<int>(audiofolder.size()));
    folderprog->setMinimum(0);
    
    
    QObject::connect(goButton, &QPushButton::clicked, 
                     [audiofolder, lines, common_tags, flayout, folderprog] () mutable 
                     { flayout->addRow(folderprog);
                       save_write_folder(audiofolder, lines, common_tags, folderprog); } );   
}


void do_single_file(QWidget* central)
{
    std::map<QString, QLineEdit*> lines;
    QString filename = QFileDialog::getOpenFileName(0,
        "Open Audio file", "C:\\", "MP3 or FLAC Files (*.mp3 *.flac)");
    
    AudioFile* audiofile;
    if (fs::path(filename.toStdString()).filename().extension() == ".mp3") 
        audiofile = new MusFile(filename);
    else
        audiofile = new FlacFile(filename);
    
    QFormLayout* flayout = new QFormLayout(central);
    
    for (const auto& qs : audiofile->get_qtags())
    {
        QLineEdit* line = new QLineEdit();
        line->setPlaceholderText(qs.second);
        line->setObjectName(qs.first);
        lines.insert({qs.first, line});
        flayout->addRow(new QLabel(audiofile->get_standard().at(qs.first)),
                        line);
    }
    QPushButton* goButton = new QPushButton("Save ID3 tags");
    flayout->addRow(goButton);
    
    QObject::connect(goButton, &QPushButton::clicked, 
                     [audiofile, lines] () mutable 
                     { save_write_tags(audiofile, lines);
                       delete audiofile; } );   
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QMainWindow w;
    w.setWindowTitle("Simple ID3/Vorbis Tag Editor");
    QWidget* bigboss = new QWidget(&w);
    w.setCentralWidget(bigboss);
    
    QMessageBox initBox;
    initBox.setText("Welcome, please choose to edit tags for"
                    "\na single Mp3/FLAC or a folder of all one type");
    initBox.addButton("Single file", QMessageBox::AcceptRole);
    initBox.addButton("Folder", QMessageBox::YesRole);
    int ret = initBox.exec();
    if (ret == 0) // accept
        do_single_file(bigboss);    
    else if (ret == 1) // yes
        do_folder(bigboss);

    w.show();
    return a.exec();
}
