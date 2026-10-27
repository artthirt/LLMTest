#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDialog>
#include <QFileDialog>
#include <QSettings>
#include <QFileInfo>
#include <QTextEdit>
#include <QTextCursor>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class DialogSession: public QDialog{
public:
    DialogSession(): QDialog(){
        auto hl = new QHBoxLayout;
        auto le = new QLineEdit;
        auto pb = new QPushButton("Set");
        hl->addWidget(le);
        hl->addWidget(pb);

        le->setText(mName);

        connect(pb, &QPushButton::clicked, this, [this, le](){
            mName = le->text();
            accept();
        });

        setLayout(hl);
    }
    QString sessionName() const{
        return mName;
    }
private:
    QString mName{"New Session"};
};

/////////////////////////////

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    qRegisterMetaType<Content>("Content");

    ui->setupUi(this);

    mTextEdit = new QTextEdit;
    ui->vlTextEdit->addWidget(mTextEdit);

    ui->mainSplitter->setSizes({600, 100});

    mWorker.reset(new LlamaWorker);

    connect(mWorker.get(), &LlamaWorker::sendMessage, this, [this](QString msg){

    }, Qt::QueuedConnection);

    connect(mWorker.get(), &LlamaWorker::generateFinished, this, [this](){
    }, Qt::QueuedConnection);

    connect(mWorker.get(), &LlamaWorker::sendPiece, this, &MainWindow::onSendPiece);
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

    ui->lvTabs->setModel(&mTabModel);

    loadSettings();

    updateTabs();

    mDlgConfig.setDevices(mWorker->getDevices());

    ui->splitter->setSizes({1, 3});
    //onAppendMessage("Test Messsage");
    //onSendPiece("Test Answer\n");
    QPalette pal = ui->lvTabs->palette();
    auto col = pal.color(QPalette::Base);
    col = col.lighter();
    pal.setColor(QPalette::AlternateBase, col);
    ui->lvTabs->setPalette(pal);

    QString style = QString(R"(
    QListView {
        background-color: %1;
        color: %2;
        border: 1px solid %3;
        outline: none;
    }

    QListView::item {
        background-color: %1;
        color: %2;
        border: 1px solid transparent;
        padding: 6px 10px;
    }

    QListView::item:hover {
        background-color: %4;
    }

    QListView::item:selected {
        background-color: %5;
        color: %6;
        border: 1px solid %3;
    }
    QListView {
        border-right: none;
    }

    QListView::item:selected {
        border-right: 1px solid #ffffff;
    }
    )")
    .arg(pal.color(QPalette::Base).name())
    .arg(pal.color(QPalette::Text).name())
    .arg(pal.color(QPalette::Mid).name())
    .arg(pal.color(QPalette::AlternateBase).name())
    .arg(pal.color(QPalette::Highlight).name())
    .arg(pal.color(QPalette::HighlightedText).name());

    ui->lvTabs->setStyleSheet(style);
}

MainWindow::~MainWindow()
{
    saveSettings();

    mWorker.reset();
    delete ui;
}

void MainWindow::on_pbLoad_clicked()
{
    LlamaWorker::Params params = mDlgConfig.params();

    QString modelpath = mDlgConfig.modelPath();

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
        font = "<span style='color:green;'>[System]> %1</span><br>";
    }else{
        font = "<span style='color:red;'>[System]> %1</span><br>";
    }
    html = font.arg(msg);
    mTextEdit->moveCursor(QTextCursor::End);
    mTextEdit->insertHtml(html);
    ui->statusbar->setStyleSheet(ok? "color: green;" : "color: red;");
    ui->statusbar->showMessage(msg);
}

void MainWindow::onSendPiece(QString piece)
{
    mTextEdit->moveCursor(QTextCursor::End);
    mTextEdit->insertPlainText(piece);
}

void MainWindow::onTimeout()
{
    if(mWorker->isLoading() && mIsLoading){
        mLoadingCnt = (mLoadingCnt + 1) % 4;
        QString spt = QString(".").repeated(mLoadingCnt);
        ui->lbProgress->setText("Model loading" + spt);
    }
}

void MainWindow::onAppendMessage(const QString &msg)
{
    if(msg.isEmpty()){
        onSendStatus("Message is empty", false);
        return;
    }
    mTextEdit->moveCursor(QTextCursor::End);
    mTextEdit->insertHtml("<span style='color:#aea;'><i>[User]> " + msg + "</i></span><br>");
}

void MainWindow::on_pbSend_clicked()
{
    if(mWorker->isGen()){
        mWorker->stopGenerate();
        return;
    }

    auto msg = ui->pteMsg->toPlainText();
    if(msg.isEmpty()){
        onSendStatus("Message is empty", false);
        return;
    }
    auto sys = mDlgConfig.systemPrompt();

    onAppendMessage(msg);

    QMetaObject::invokeMethod(mWorker.get(), "addNewMessage", Qt::QueuedConnection,
                              Q_ARG(QString, sys),
                              Q_ARG(QString, msg));

    ui->pteMsg->clear();

    ui->pbSend->setText("Stop");
}

void MainWindow::loadSettings()
{
    auto tabsList = mDlgConfig.settings()->value("tabs_list").toList();
    for(auto v: tabsList){
        auto c = v.value<Content>();
        auto *item = new QStandardItem(c.caption);
        item->setData(QVariant::fromValue<Content>(c));
        mTabModel.appendRow(item);
    }

    mDlgConfig.loadSettings();
}

void MainWindow::saveSettings()
{
    QList<QVariant> tabs;
    for(int i = 0; i < mTabModel.rowCount(); ++i){

        auto item = mTabModel.item(i, 0);
        auto text = item->text();
        auto content = item->data().value<Content>();
        content.caption = text;
        tabs.append(QVariant::fromValue<Content>(content));
    }
    mDlgConfig.settings()->setValue("tabs_list", tabs);

    mDlgConfig.saveSettings();
}

void MainWindow::updateTabs()
{
    if(mTabModel.rowCount() == 0){
        mTabModel.appendRow(new QStandardItem("New Session"));
        auto index = mTabModel.indexFromItem(mTabModel.item(0));
        ui->lvTabs->setCurrentIndex(index);
    }else{
        ui->lvTabs->setCurrentIndex(mTabModel.index(0, 0));
        loadItem(0);
    }
}

void MainWindow::saveLastSession()
{
    if(mLastRow >= 0 && mLastRow < mTabModel.rowCount()){
        mCurrentContent.caption = mTabModel.item(mLastRow)->text();
        mCurrentContent.content = mTextEdit->document()->toHtml();
        mCurrentContent.context = mWorker->messages();
        mTabModel.item(mLastRow)->setData(QVariant::fromValue<Content>(mCurrentContent));
    }
}

void MainWindow::loadItem(int row)
{
    auto data = mTabModel.item(row)->data().value<Content>();
    if(!data.empty()){
        mWorker->swapMessages(data.context);
        mTextEdit->document()->setHtml(data.content);
    }

}

void MainWindow::on_tbtConfig_clicked()
{
    mDlgConfig.show();
}

void MainWindow::on_pbNewSession_clicked()
{
    DialogSession dlg;
    if(dlg.exec()){
        auto name = dlg.sessionName();
        mTabModel.appendRow(new QStandardItem(name));
        saveLastSession();

    }
}

void MainWindow::on_lvTabs_clicked(const QModelIndex &index)
{
    if(mLastRow != index.row()){

        saveLastSession();

        loadItem(index.row());
        mLastRow = index.row();
    }
}

