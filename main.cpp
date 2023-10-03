#include "musfile.h"

#include <string>
#include <vector>

#include <QApplication>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QMainWindow>
#include <QTextEdit>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <sstream>
#include <map>

void save_write_tags(MusFile& mp3, 
                     std::map<QString, QLineEdit*>& lines)
{
    // extract text from each lineedit and save to qtags
    for (const auto& line : lines)
    {  
        if (!line.second->text().isEmpty())
            mp3.show_qtags().at(line.first) = line.second->text();
        //qInfo() << mp3.show_qtags().at(line.first);
        // if it IS empty, then the original text is kept (do nothing)
    }
    
    bool success = mp3.write_qtags();
    QMessageBox msgBox;
    if (success)
        msgBox.setText("Tags written successfully");
    else
        msgBox.setText("Operation Failed");
    msgBox.exec();
}

extern std::map<QString, QString> standard_qtags;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QMainWindow w;
    w.setWindowTitle("Simple ID3 Tag Editor");
    QWidget* bigboss = new QWidget(&w);
    w.setCentralWidget(bigboss);
    
    std::map<QString, QLineEdit*> lines;
    QString filename = QFileDialog::getOpenFileName(&w,
        "Open MP3 File", "C:\\", "MP3 Files (*.mp3)");
    MusFile mp3(filename);
    
    
    QFormLayout* flayout = new QFormLayout(bigboss);
    
    
    
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
                     [&mp3, &lines] () { save_write_tags(mp3, lines); } );
    
    

    w.show();
    return a.exec();
}
