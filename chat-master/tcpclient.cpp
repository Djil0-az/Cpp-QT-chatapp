#include "tcpclient.h"
#include "ui_tcpclient.h"
#include <QTcpSocket>
#include <QMessageBox>

TcpClient::TcpClient(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TcpClient)
{
    ui->setupUi(this);
    setFixedSize(350,180);

    TotalBytes = 0;//total file size
    bytesReceive = 0;// size of file currently recieved
    fileNameSize = 0;// file name size
    tcpClient = new QTcpSocket(this);
    tcpPort = 6666;
    // tcp client connect to server
    connect(tcpClient,SIGNAL(readyRead()),this,SLOT(readMessage()));
    connect(tcpClient,SIGNAL(error(QAbstractSocket::SocketError)),
            this,SLOT(displayError(QAbstractSocket::SocketError)));
}

TcpClient::~TcpClient()
{
    delete ui;
}

void TcpClient::setHostAddress(QHostAddress address)
{
    hostAddress = address;
    newConnect();
}

void TcpClient::setFileName(QString fileName)
{
    localFile = new QFile(fileName);
}

void TcpClient::closeEvent(QCloseEvent *)
{
    on_tcpClientCloseBtn_clicked();
}

void TcpClient::newConnect()
{   //client connect t
    blockSize = 0;
    tcpClient->abort();
    tcpClient->connectToHost(hostAddress,tcpPort);
    time.start();
}

void TcpClient::readMessage()
{
    //read messages sent, show ftp progress and time elapsed

    QDataStream in(tcpClient);
    in.setVersion(QDataStream::Qt_5_4);//read the file content

    float useTime = time.elapsed();
    qDebug() << bytesReceive << endl;
    //
    if(bytesReceive <= sizeof(qint64)*2)//max byte to recieve size is qint64*2
    {
        if((tcpClient->bytesAvailable()>=sizeof(qint64) * 2) && (fileNameSize == 0))
        {   //determine file size
            in >> TotalBytes >> fileNameSize;
            bytesReceive += sizeof(qint64) * 2;
        }
        //if file size is too large
        if((tcpClient->bytesAvailable() >= fileNameSize) && (fileNameSize != 0)) {
            in >> fileName;
            bytesReceive += fileNameSize;
            if(!localFile->open(QFile::WriteOnly)) {
                QMessageBox::warning(this,tr("program"),tr("Cannot recieve file %1：\n %2")
                                     .arg(fileName).arg(localFile->errorString()));
                return;
            }

        }
        else
        {
            return;
        }
    }
    if(bytesToReceive < TotalBytes)
    {   // increase space and write data
        bytesReceive += tcpClient->bytesAvailable();
        inBlock = tcpClient->readAll();
        localFile->write(inBlock);
        inBlock.resize(0);
    }
    // show progress
    ui->progressBar->setMaximum(TotalBytes);
    ui->progressBar->setValue(bytesReceive);
    double speed = bytesReceive / useTime;
    ui->tcpClientStatuslabel->setText(tr("recieved %1 MB (%2MB/s)"
                                         "\ntotal:%3MB.time elapsed：%4s\nETA：%5s")
                                      .arg(bytesReceive / (1024*1024))
                                      .arg(speed * 1000/(1024 * 1024),0,'f',2)
                                      .arg(TotalBytes / (1024*1024))
                                      .arg(useTime/1000,0,'f',0)
                                      .arg(TotalBytes/speed/1000 - useTime/1000,0,'f',0));
    // transfer is complete
    if(bytesReceive == TotalBytes) {
        localFile->close();
        tcpClient->close();
        ui->tcpClientStatuslabel->setText(tr("file transfer %1 complete").arg(fileName));
    }
}

void TcpClient::displayError(QAbstractSocket::SocketError socketError)
{   //Display Client socket error when remote host closes the connection
    switch(socketError)
    {
    case QAbstractSocket::RemoteHostClosedError: break;
    default :
        qDebug() << tcpClient->errorString();
    }
}

void TcpClient::on_tcpClientBtn_clicked() //Cancel FTP
{
    tcpClient->abort();
    if(localFile->isOpen())
        localFile->close();
}

void TcpClient::on_tcpClientCloseBtn_clicked() //close widget
{
    tcpClient->abort();
    if(localFile->isOpen())
        localFile->close();
    close();
}

