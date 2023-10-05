#include <string>
#include <vector>
#include <sstream>
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

namespace fs = std::filesystem;
extern std::map<QString, QString> standard_qtags;

void save_write_tags(MusFile& mp3, 
                     std::map<QString, QLineEdit*>& lines)
{
    // extract text from each QLineEdit and save to qtags
    for (const auto& line : lines)
    {  
        if (!line.second->text().isEmpty())
            mp3.QTags.at(line.first) = line.second->text();
    }

    bool success = mp3.write_qtags();
    QMessageBox msgBox;
    if (success)
        msgBox.setText("Tags written successfully");
    else
        msgBox.setText("Operation Failed");
    msgBox.exec();
}

void save_write_folder(std::vector<MusFile>& musfolder, 
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
    for (MusFile& mus : musfolder)
        for (auto& tag : commontags)
            mus.QTags.at(tag.first) = tag.second;
    
    bool success = all_of(musfolder.begin(), musfolder.end(),
                          [&musfolder, progbar] (MusFile& mp3) 
                           {    progbar->setValue(progbar->value() + 1);
                                QApplication::processEvents();
                                return mp3.write_qtags(); } );
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
    // implement tag editing for a folder of mp3s...  
    //  -- identify tags that are the same across all files --done
    //  -- create sequence that holds these tags and iterate through
    //  it to create interface to allow editing them
    //  -- allow add/remove of tags
    //  -- write tags to files when clicked()
    
    std::vector<MusFile> musfolder;
    QString opendir = QFileDialog::getExistingDirectory(0,
                     "Choose Folder of MP3s", "C:\\", QFileDialog::ShowDirsOnly);
    fs::path filedir = opendir.toStdString();
    fs::directory_iterator dirit(filedir);
    
    int current = 1;
    for (const auto& p : dirit)
    {      
        if (p.path().extension().string() == ".mp3")
        {
            musfolder.emplace_back(QString::fromStdString(p.path().string()));
        }
    } 
    if (musfolder.empty())
        throw std::runtime_error("No MP3s in Directory");
    
    QFormLayout* flayout = new QFormLayout(central);
    std::map<QString, QString> common_tags;
    
    for (const auto& tag : musfolder[0].QTags)
    {
        if (std::all_of(musfolder.begin(), musfolder.end(), [&tag] 
                (MusFile& m)
                { if (m.QTags.count(tag.first) != 0)
                    return m.QTags.at(tag.first) == tag.second;
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
        flayout->addRow(new QLabel(standard_qtags.at(qs.first)),
                        line);
    }
    QPushButton* goButton = new QPushButton("Save ID3 tags");
    flayout->addRow(goButton);
    
    QProgressBar* folderprog = new QProgressBar();
    folderprog->setMaximum(static_cast<int>(musfolder.size()));
    folderprog->setMinimum(0);
    
    
    QObject::connect(goButton, &QPushButton::clicked, 
                     [musfolder, lines, common_tags, flayout, folderprog] () mutable 
                     { flayout->addRow(folderprog);
                       save_write_folder(musfolder, lines, common_tags, folderprog); } );   
}


void do_single_file(QWidget* central)
{
    std::map<QString, QLineEdit*> lines;
    QString filename = QFileDialog::getOpenFileName(0,
        "Open MP3 File", "C:\\", "MP3 Files (*.mp3)");
    MusFile mp3(filename);
    
    QFormLayout* flayout = new QFormLayout(central);
    
    for (const auto& qs : mp3.QTags)
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
                     [mp3, lines] () mutable { save_write_tags(mp3, lines); } );   
}


int main(int argc, char *argv[])
{
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
}
