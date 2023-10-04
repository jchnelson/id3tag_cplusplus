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
            mp3.show_qtags().at(line.first) = line.second->text();
    }

    bool success = mp3.write_qtags();
    QMessageBox msgBox;
    if (success)
        msgBox.setText("Tags written successfully");
    else
        msgBox.setText("Operation Failed");
    msgBox.exec();
}

std::vector<MusFile> initialize_folder()
{
    // implement tag editing for a folder of mp3s... 
    //  -- identify tags that are the same across all files
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
            qInfo() << "Processing" << current++;
        }

    }
    
    return musfolder;
      
}

void do_folder(QWidget* central)
{
    std::vector<MusFile> musfolder = initialize_folder();
    qInfo() << "musfolder size" << musfolder.size();
    if (musfolder.empty())
        throw std::runtime_error("No MP3s in Directory");
    
    QFormLayout* flayout = new QFormLayout(central);
    std::map<QString, QString> common_tags;
    
    for (const auto& tag : musfolder[0].show_qtags())
    {
        if (std::all_of(musfolder.begin(), musfolder.end(), [&tag] 
                (MusFile& m)
                { if (m.show_qtags().count(tag.first) != 0)
                    return m.show_qtags().at(tag.first) == tag.second;
                  else 
                    return false; } ) )
            common_tags.insert(tag);
    }
    
    std::map<QString, QLineEdit*> lines;
    
    for (const auto& qs : common_tags)
    {
        //qInfo() << qs.first << qs.second;
        QLineEdit* line = new QLineEdit();
        line->setPlaceholderText(qs.second);
        line->setObjectName(qs.first);
        lines.insert({qs.first, line});
        flayout->addRow(new QLabel(standard_qtags.at(qs.first)),
                        line);
    }
    QPushButton* goButton = new QPushButton("Save ID3 tags");
    flayout->addRow(goButton);
    
    //QObject::connect(goButton, &QPushButton::clicked, 
                     //[mp3, lines] () mutable { save_write_tags(mp3, lines); } );   
}
    


void do_single_file(QWidget* central)
{
    std::map<QString, QLineEdit*> lines;
    QString filename = QFileDialog::getOpenFileName(0,
        "Open MP3 File", "C:\\", "MP3 Files (*.mp3)");
    MusFile mp3(filename);
    
    
    QFormLayout* flayout = new QFormLayout(central);
    
    
    
    for (const auto& qs : mp3.show_qtags())
    {
        //qInfo() << qs.first << qs.second;
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
    qInfo() << ret;
    if (ret == 0) // accept
        do_single_file(bigboss);    
    else if (ret == 1) // yes
        do_folder(bigboss);
    
    

 

    w.show();
    return a.exec();
}
