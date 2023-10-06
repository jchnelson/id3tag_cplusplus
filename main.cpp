#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <type_traits>

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
extern std::map<QString, QString> standard_qtags;





void do_folder(QWidget* central)
{
    //  TODO -- allow add/remove of tags 
 
    
    
    QString opendir = QFileDialog::getExistingDirectory(0,
                     "Choose Folder of only MP3 or only FLAC", "C:\\", QFileDialog::ShowDirsOnly);
    fs::path filedir = opendir.toStdString();
    fs::directory_iterator dirit(filedir);
    
    bool mp3_type;
    if (std::all_of(begin(dirit), end(dirit), [] (const fs::directory_entry& entry)
            { return entry.path().extension().string() == ".mp3"; } ))
        mp3_type = true;
    else if (std::all_of(begin(dirit = fs::directory_iterator(filedir)), end(dirit), [] (const fs::directory_entry& entry)
    { return entry.path().extension().string() == ".flac"; } ))
        mp3_type = false;
    else
        throw std::runtime_error("Mixed filetypes not supported");
    
    std::vector<AudioFile*> audiofolder;
    
    dirit = fs::directory_iterator(filedir); // reinitialize iterator
    for (const auto& p : dirit)
    {
        if (mp3_type)
            audiofolder.emplace_back(new MusFile(QString::fromStdString(p.path().string())));
        else
            audiofolder.emplace_back(new FlacFile(QString::fromStdString(p.path().string())));
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
        // following line needs to change
        flayout->addRow(new QLabel(standard_qtags.at(qs.first)),
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
                       audiofolder[0]->save_write_folder(audiofolder, lines, common_tags, folderprog); } );   
}


void do_single_file(QWidget* central)
{
    std::map<QString, QLineEdit*> lines;
    QString filename = QFileDialog::getOpenFileName(0,
        "Open Audio file", "C:\\", "MP3 Files (*.mp3), FLAC Files (*.flac)");
    
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
        flayout->addRow(new QLabel(standard_qtags.at(qs.first)),
                        line);
    }
    QPushButton* goButton = new QPushButton("Save ID3 tags");
    flayout->addRow(goButton);
    
    QObject::connect(goButton, &QPushButton::clicked, 
                     [audiofile, lines] () mutable 
                     { audiofile->save_write_tags(lines);
                                    delete audiofile; } );   
}


int main(int argc, char *argv[])
{
    FlacFile bob("C:\\Users\\Jakob\\Music\\Jork - Superannuated\\Jork - Superannuated - 01 Fusion, part 2.flac");
    qInfo() << "test";
    /*
    QApplication a(argc, argv);
    QMainWindow w;
    w.setWindowTitle("Simple ID3 Tag Editor");
    QWidget* bigboss = new QWidget(&w);
    w.setCentralWidget(bigboss);
    
    QMessageBox initBox;
    initBox.setText("Welcome, please choose to edit tags for"
                    "\na single mp3 or a folder of them");
    initBox.addButton("Single mp3", QMessageBox::AcceptRole);
    initBox.addButton("Folder of Mp3s", QMessageBox::YesRole);
    int ret = initBox.exec();
    if (ret == 0) // accept
        do_single_file(bigboss);    
    else if (ret == 1) // yes
        do_folder(bigboss);

    w.show();
    return a.exec();
    */
}
