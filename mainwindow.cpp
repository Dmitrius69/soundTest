#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>



#define INBUFF 16384
#define OUTBUFF 32768

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionOpen_triggered()
{
    QMessageBox myMsgBox;
    QString _fileName = QFileDialog::getOpenFileName(this, tr("OPEN FILE"), getenv("HOME"), tr("All (*.*)"));
    //myMsgBox.setText(_fileName);
    //myMsgBox.exec();
    ui->labelFN->setText(_fileName);
    read_audio_file(_fileName);

}

void MainWindow::on_pushButton_clicked()
{
    //start audio here

}

void MainWindow::on_pushButton_2_clicked()
{
    //stop audio here
}


int MainWindow::read_audio_file(QString flName)
{
    unsigned char inbuff[INBUFF];
    int handle, bytes_read;

    if((handle = open(flName.toStdString().c_str() ,O_RDONLY))== -1)
    {
          ui->labelMsg->setText(tr("Ошибка при открытии файла.\n"));
          return 0;
    }

    if((bytes_read = read(handle,inbuff,INBUFF))==-1)
    {
       ui->labelMsg->setText(tr("Ошибка чтения.\n"));
       return 0;
    }


    ui->labelMsg->setText(QString("Bytes read %1").arg(bytes_read));
    return 1;
}
