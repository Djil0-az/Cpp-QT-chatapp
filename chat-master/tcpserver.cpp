#include "tcpserver.h"
#include "ui_tcpserver.h"
#include <QTcpServer>
#include <QMessageBox>
#include <QFileDialog>
#include <QTcpSocket>
#include <QDataStream>
#include <QTime>

TcpServer::TcpServer(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TcpServer)
{
    ui->setupUi(this);

    //setFixedSize(350,180);
    tcpPort = 6666;
    tcpServer = new QTcpServer(this);
    connect(tcpServer,SIGNAL(newConnection()),this,SLOT(sendMessage()));

    initServer();
}

TcpServer::~TcpServer()
{
    delete ui;
}

void TcpServer::initServer()
{   // prepare server for FTP
    payloadSize = 64*1024;
    totalBytes = 0;
    bytesWritten = 0;
    bytesToWrite = 0;
    ui->serverStatuslabel->setText(tr("please choose file to be sent"));
    ui->progressBar->reset();
    ui->serverOpenBtn->setEnabled(true);
    ui->serverSendBtn->setEnabled(false);
    tcpServer->close();
}

void TcpServer::refused()
{
    tcpServer->close();
    ui->serverStatuslabel->setText(tr("connection has been refused"));
}

void TcpServer::closeEvent(QCloseEvent *)
{
    on_serverCloseBtn_clicked();
}

void TcpServer::sendMessage()
{
    ui->serverSendBtn->setEnabled(false);
    clientConnection = tcpServer->nextPendingConnection();
    //ftp signal send file and upate the progress
    connect(clientConnection,SIGNAL(bytesWritten(qint64)),
            this,SLOT(updateClientProgress(qint64)));
    ui->serverStatuslabel->setText(tr("Sending file %1 ！").arg(theFileName));

    localFile = new QFile(fileName);
    if(!localFile->open((QFile::ReadOnly)))
    {   //file is readonly error
        QMessageBox::warning(this,tr("Error"),tr("Cannot send file %1:\n %2")
                             .arg(fileName).arg(localFile->errorString()));
        return;
    }
    //find total size of file to be sent
    totalBytes = localFile->size();
    QDataStream sendOut(&outBlock,QIODevice::WriteOnly);
    sendOut.setVersion(QDataStream::Qt_5_4);
    time.start();
    QString currentFile = fileName.right(fileName.size()
                                         - fileName.lastIndexOf('/')-1);
    //send file to stream
    sendOut << qint64(0) << qint64(0) << currentFile;
    totalBytes += outBlock.size();
    sendOut.device()->seek(0);
    sendOut << totalBytes << qint64((outBlock.size() - sizeof(qint64) * 2));
    bytesToWrite = totalBytes - clientConnection->write(outBlock);
    outBlock.resize(0);
}

void TcpServer::updateClientProgress(qint64 numBytes)
{   //Bytes sent and time elapsed
    qApp->processEvents();
    bytesWritten += (int)numBytes;
    if(bytesWritten > 0) {
        outBlock = localFile->read(qMin(bytesToWrite,payloadSize));
        bytesToWrite -= (int)clientConnection->write(outBlock);
        outBlock.resize(0);
    }
    else
    {   //cannot send file
        localFile->close();
    }
    ui->progressBar->setMaximum(totalBytes);
    ui->progressBar->setValue(bytesWritten);

    float userTime = time.elapsed();
    double speed = bytesWritten / userTime;
    ui->serverStatuslabel->setText(tr("file sent %1MB（%2MB/s)"
                                   "\ntotal%3MB time elapsed：%4s\n ETA:%5s")
            .arg(bytesWritten / (1024*1024))
            .arg(speed * 1000 / (1024*1024),0,'f',2)
            .arg(totalBytes / (1024*1024))
                                   .arg(userTime/1000,0,'f',0));
}

void TcpServer::on_serverOpenBtn_clicked()
{   //choose file to send then enable the buttons to send
    fileName = QFileDialog::getOpenFileName(this);
    if(!fileName.isEmpty())
    {
        theFileName = fileName.right(fileName.size() - fileName.lastIndexOf('/') - 1);
        ui->serverStatuslabel->setText(tr("fiel to be sent ：%1").arg(theFileName));
        ui->serverSendBtn->setEnabled(true);
        ui->serverOpenBtn->setEnabled(false);
    }
}

void TcpServer::on_serverSendBtn_clicked()
{
    if(!tcpServer->listen(QHostAddress::Any,tcpPort))
    {   //No connection
        qDebug() << tcpServer->errorString();
        close();
        return;
    }
    ui->serverStatuslabel->setText(tr("Sending file to client... ..."));
    ui->serverSendBtn->setEnabled(false);
    emit sendFileName(theFileName);
}

void TcpServer::on_serverCloseBtn_clicked()
{
    if(tcpServer->isListening())
    {
        tcpServer->close();
        if(localFile->isOpen())
            localFile->close();
        clientConnection->abort();
    }
    close();
}
