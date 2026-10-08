#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QSettings>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mSetts("LLMTest", "LLMTest")
{
    ui->setupUi(this);

    ui->mainSplitter->setSizes({600, 100});

    mWorker.reset(new LlamaWorker);

    connect(mWorker.get(), &LlamaWorker::sendMessage, this, [this](QString msg){

    }, Qt::QueuedConnection);

    connect(mWorker.get(), &LlamaWorker::generateFinished, this, [this](){
        ui->pteOutput->appendPlainText("[End]\n");
    }, Qt::QueuedConnection);

    connect(mWorker.get(), &LlamaWorker::sendPiece, this, [this](QString piece){
        ui->pteOutput->moveCursor(QTextCursor::End);
        ui->pteOutput->insertPlainText(piece);
    });
    connect(mWorker.get(), &LlamaWorker::modelLoaded, this, [this](){
        mIsLoading = false;
        onSendStatus("Model loaded", true);
        ui->lbProgress->setText("Model loaded");
    });
    connect(mWorker.get(), &LlamaWorker::modelError, this, [this](){
        mIsLoading = false;
        ui->lbProgress->setText("Model error");
    });
    connect(mWorker.get(), &LlamaWorker::modelUnloaded, this, [this](){
        mIsLoading = false;
        onSendStatus("Model unload", true);
        ui->lbProgress->setText("Model unloaded");
    });
    connect(mWorker.get(), &LlamaWorker::stopGenerated, this, [this](){
        ui->pbSend->setText("Send");
    });
    connect(mWorker.get(), &LlamaWorker::sendStatistics, this, [this](float p, float s){
        ui->statusbar->showMessage(QString("prefill=%1 t/s  |  gen=%2 t/s")
                                   .arg(p, 0, 'f', 1)
                                   .arg(s, 0, 'f', 1));
    });

    connect(&mTimer, &QTimer::timeout, this, &MainWindow::onTimeout);
    mTimer.start(400);

    connect(mWorker.get(), &LlamaWorker::sendStatus, this, &MainWindow::onSendStatus);

    connect(mWorker.get(), &LlamaWorker::sendProgress, this, [this](float val){
        mIsLoading = false;
        ui->lbProgress->setText(QString("%1 %").arg(int(val * 100)));
    }, Qt::QueuedConnection);

    loadSettings();
}

MainWindow::~MainWindow()
{
    saveSettings();

    mWorker.reset();
    delete ui;
}

void MainWindow::on_tbtSelect_clicked()
{
    QString path = mSetts.value("path").toString();
    QString sdir = QFileInfo(path).absolutePath();
    path = QFileDialog::getOpenFileName(nullptr, "GGUF model", sdir, "*.gguf");
    if(!path.isEmpty()){
        ui->lePath->setText(path);
    }
    saveSettings();
}

void MainWindow::on_pbLoad_clicked()
{
    LlamaWorker::Params params;
    params.context = ui->sbMaxContext->value();
    params.gpu_layers = ui->sbGpuLayers->value();
    params.temp = ui->dsbTemp->value();
    params.enable_thinking = ui->chbThinking->isChecked();

    QString modelpath = ui->lePath->text();

    onSendStatus("Model loading...", true);
    mIsLoading = true;
    mLoadingCnt = {};

    QMetaObject::invokeMethod(mWorker.get(), "loadModel", Qt::QueuedConnection,
                              Q_ARG(QString, modelpath),
                              Q_ARG(LlamaWorker::Params, params));
}

void MainWindow::on_pbUnload_clicked()
{
    QMetaObject::invokeMethod(mWorker.get(), "unloadModel", Qt::QueuedConnection);
}

void MainWindow::onSendStatus(QString msg, bool ok)
{
    QString html, font;
    if(ok){
        font = "<span style='color:green;'>%1</span>";
    }else{
        font = "<span style='color:red;'>%1</span>";
    }
    html = font.arg(msg);
    ui->pteOutput->appendHtml(html);
    ui->statusbar->setStyleSheet(ok? "color: green;" : "color: red;");
    ui->statusbar->showMessage(msg);
}

void MainWindow::onTimeout()
{
    if(mWorker->isLoading() && mIsLoading){
        mLoadingCnt = (mLoadingCnt + 1) % 4;
        QString spt = QString(".").repeated(mLoadingCnt);
        ui->lbProgress->setText("Model loading" + spt);
    }
}

void MainWindow::on_pbSend_clicked()
{
    if(mWorker->isGen()){
        mWorker->stopGenerate();
        return;
    }
    QString msg = ui->pteMsg->toPlainText();
    if(msg.isEmpty()){
        onSendStatus("Message is empty", false);
        return;
    }

    ui->pteOutput->appendHtml("<span>==== User ====</span><br><i>" + msg + "</i><br><span>============</span><br>");

    QMetaObject::invokeMethod(mWorker.get(), "addNewMessage", Qt::QueuedConnection,
                              Q_ARG(QString, msg));

    ui->pteMsg->clear();

    ui->pbSend->setText("Stop");
}

void MainWindow::loadSettings()
{
    ui->lePath->setText(mSetts.value("path").toString());
    ui->sbGpuLayers->setValue(mSetts.value("gpu_layers", -1).toInt());
    ui->sbMaxContext->setValue(mSetts.value("max_context", 8192).toInt());
    ui->dsbTemp->setValue(mSetts.value("temp", 0.8).toDouble());
}

void MainWindow::saveSettings()
{
    mSetts.setValue("path", ui->lePath->text());
    mSetts.setValue("gpu_layers", ui->sbGpuLayers->value());
    mSetts.setValue("max_context", ui->sbMaxContext->value());
    mSetts.setValue("temp", ui->dsbTemp->value());
}
