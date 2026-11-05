#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "closebuttondelegate.h"

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

    //mTextEdit = new QTextEdit;
    //ui->vlTextEdit->addWidget(mTextEdit);
    //ui->vlTextEdit->addWidget(&mChatView);
    ui->pteMsg->installEventFilter(this);

    ui->mainSplitter->setSizes({700, 100});

    connect(&mDlgConfig, &DialogConfig::sendWeights, this, [this](QString w, QStringList ws){
        ui->cbWeights->clear();
        ui->cbWeights->setCurrentText(w);
        ui->cbWeights->addItems(ws);
    });

    mWorker.reset(new LlamaWorker);

    connect(mWorker.get(), &LlamaWorker::sendMessage, this, [this](QString msg){

    }, Qt::QueuedConnection);

    connect(mWorker.get(), &LlamaWorker::sendContent, this, &MainWindow::onSendContent, Qt::QueuedConnection);
    connect(mWorker.get(), &LlamaWorker::sendReasoning, this, &MainWindow::onSendReasoning, Qt::QueuedConnection);

    connect(mWorker.get(), &LlamaWorker::generateFinished, this, &MainWindow::onGenerationFinished, Qt::QueuedConnection);

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

    ui->leftSplitter->setSizes({100, 500});
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
        border: none;
        outline: none;
    }

    QListView::item {
        background-color: %1;
        color: %2;
        border: 1px solid transparent;
        border-radius: 5px;
        padding: 0px 10px;
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
    .arg(pal.color(QPalette::Window).name())
    .arg(pal.color(QPalette::Text).name())
    .arg(pal.color(QPalette::Mid).name())
    .arg(pal.color(QPalette::AlternateBase).name())
    .arg(pal.color(QPalette::Midlight).name())
    .arg(pal.color(QPalette::HighlightedText).name());

    auto *delegate = new CloseButtonDelegate(ui->lvTabs);
    ui->lvTabs->setItemDelegate(delegate);

    connect(delegate, &CloseButtonDelegate::closeRequested, this, &MainWindow::onCloseItemRequested);

    ui->gridLayout->addWidget(&mContextIndicator, 1, 2);

    ui->lvTabs->setStyleSheet(style);

    // makeToolButton(mTextEdit, "❌", [this](){
    //     if(mWorker){
    //         mWorker->clearMessage();
    //     }
    //     mTextEdit->clear();
    // }, "Clear", 0);
}

MainWindow::~MainWindow()
{
    saveSettings();

    mWorker.reset();
    delete ui;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(ui->pteMsg == watched && event->type() == QEvent::KeyPress){
        auto *key = static_cast<QKeyEvent*>(event);
        if(key && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
            && (key->modifiers() & Qt::ShiftModifier) == 0){
            sendMessage();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::sendMessage()
{
    if(mChatWidgets.empty()) return;

    if(!mWorker) return;
    if(mWorker->isGen()){
        mWorker->stopGenerate();
        ui->pbSend->setText("Send");
        return;
    }

    auto msg = ui->pteMsg->toPlainText();
    if(msg.isEmpty()){
        onSendStatus("Message is empty", false);
        ui->pbSend->setText("Send");
        return;
    }
    auto sys = mDlgConfig.systemPrompt();

    mSessionId = mLastRow;

    onAppendMessage(msg);

    QMetaObject::invokeMethod(mWorker.get(), "addNewMessage", Qt::QueuedConnection,
                              Q_ARG(QString, sys),
                              Q_ARG(QString, msg));

    ui->pteMsg->clear();

    ui->pbSend->setText("Stop");
}

void MainWindow::on_pbLoad_clicked()
{
    if(!mWorker) return;

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
    if(mWorker)
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
    //mCurrentContent[mSessionId].insertHtml(html);
    if(mSessionId == mLastRow){
        //mTextEdit->moveCursor(QTextCursor::End);
        //mTextEdit->insertHtml(html);
    }
    ui->statusbar->setStyleSheet(ok? "color: green;" : "color: red;");
    ui->statusbar->showMessage(msg);
}

void MainWindow::onSendPiece(QString piece)
{
    if(mWorker){
        mContextIndicator.setProgress(mWorker->contextProgress());
    }
    //mCurrentContent[mSessionId].insertPlainText(piece);
    if(mLastRow == mSessionId){
        //mTextEdit->moveCursor(QTextCursor::End);
        //mTextEdit->insertPlainText(piece);
    }
}

void MainWindow::onTimeout()
{
    if(mWorker && mWorker->isLoading() && mIsLoading){
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
    if(!mChatWidgets.count(mLastRow)) return;

    //auto res = "<span style='color:#aea;'><i>[User]> " + msg + "</i></span><br>";
    //mTextEdit->moveCursor(QTextCursor::End);
    //mTextEdit->insertHtml(res);
    mChatWidgets.at(mLastRow)->addUser(msg);
    //mCurrentContent[mSessionId].insertHtml(res);
}

void MainWindow::on_pbSend_clicked()
{
    sendMessage();
}

void MainWindow::loadSettings()
{
    auto tabsList = mDlgConfig.settings()->value("tabs_list_v").toList();
    for(const auto &v: tabsList){
        auto c = v.value<Content>();
        auto *item = new QStandardItem(c.caption);
        mTabModel.appendRow(item);
        int id = mTabModel.rowCount() - 1;
        addNewSession(id, std::move(c));
        //mCurrentContent[id] = c;
    }
    if(!tabsList.empty()){
        loadItem(0);
    }

    mDlgConfig.loadSettings();
}

void MainWindow::saveSettings()
{
    QList<QVariant> tabs;
    for(const auto &kv: mChatWidgets){
        auto *item = kv.second;
        if(item){
            tabs.append(QVariant::fromValue<Content>(item->content()));
        }
    }
    mDlgConfig.settings()->setValue("tabs_list_v", tabs);

    mDlgConfig.saveSettings();
}

void MainWindow::updateTabs()
{
    if(mTabModel.rowCount() == 0){
        auto name = "New Session";
        mTabModel.appendRow(new QStandardItem(name));
        auto index = mTabModel.indexFromItem(mTabModel.item(0));
        ui->lvTabs->setCurrentIndex(index);
        Content c{name};
        addNewSession(index.row(), std::move(c));
    }else{
        ui->lvTabs->setCurrentIndex(mTabModel.index(0, 0));
        loadItem(0);
    }
}

void MainWindow::loadItem(int row)
{
    if(mChatWidgets.count(row) == 0){
        return;
    }
    auto *item = mChatWidgets.at(row);
    if(!item) return;

    Content &data = item->content();
    if(mWorker && !data.empty()){
        mWorker->swapMessages(data.context);
        //mTextEdit->document()->setHtml(data.content);
        //mTextEdit->setHtml(data.toHtml());
        item->chat().setFromJson(data.json);
        ui->swChats->setCurrentIndex(item->indexW());
    }
}

void MainWindow::makeToolButton(QWidget *widget,
                                const QString &text, std::function<void ()> fun,
                                const QString &tooltip, int X)
{
    auto *button = new QToolButton(widget);
    button->setText(text);
    // button->setIcon(style()->standardIcon(
    //     QStyle::SP_MessageBoxInformation));
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setFixedSize(24, 24);

    button->setStyleSheet(R"(
    QToolButton {
        background: transparent;
        border: none;
    }
    QToolButton:hover {
        background: rgba(128, 128, 128, 40);
        border-radius: 4px;
    }
    QToolButton:pressed {
        background: rgba(128, 128, 128, 80);
    }
)");

    QObject::connect(button, &QToolButton::clicked, this, [this, fun]() {
        // Your action here
        if(fun){
            fun();
        }
    });

    auto updatePosition = [widget, button, X]() {
        button->move(widget->width() - button->width() - 8 - X, 4);
        button->raise();
    };

    // QObject::connect(plainTextEdit, &QTextEdit::textChanged,
    //                  this, updatePosition); // Optional; not needed for resizing.

    auto *filter = new ResizeEventFilter(
        widget, updatePosition);

    widget->installEventFilter(filter);

    updatePosition();
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
        mLastRow = mSessionId = mTabModel.rowCount() - 1;
        //mCurrentContent[mSessionId].caption = name;
        ui->lvTabs->setCurrentIndex(mTabModel.index(mSessionId, 0));
        //mTextEdit->clear();
        Content c;
        c.caption = name;
        addNewSession(mSessionId, std::move(c));
    }
}

void MainWindow::on_lvTabs_doubleClicked(const QModelIndex &index)
{
    if(index.isValid()){
        DialogSession dlg;
        if(dlg.exec()){
            auto name = dlg.sessionName();
            mTabModel.item(index.row())->setText(name);
            auto *item = mChatWidgets.at(index.row());
            if(item) item->content().caption = name;
        }
    }
}

void MainWindow::onCloseItemRequested(const QModelIndex &index)
{
    int row = index.row();
    QTimer::singleShot(10, [this, row](){
        auto wId = mTabModel.item(row)->data().toInt();
        qDebug() << row << wId;
        if(mChatWidgets.count(wId) > 0){
            bool ishere = ui->lvTabs->currentIndex().row() == row;

            if(mWorker->isGen()){
                qDebug() << "Still working. wait...";
                onSendStatus("Still working. wait...", false);
                return;
            }

            if(ishere && mTabModel.rowCount() > 1){
                int newrow = row;
                if(row + 1 < mTabModel.rowCount()){
                    newrow = row + 1;
                }else{
                    newrow = row - 1;
                }
                ui->lvTabs->setCurrentIndex(mTabModel.index(row - 1, 0));
                loadItem(newrow);
            }
            ui->lvTabs->setCurrentIndex(mTabModel.index(row - 1, 0));
            {
                auto item = mChatWidgets.at(wId);
                if(item){
                    mChatWidgets.erase(wId);
                    ui->swChats->removeWidget(item);
                    delete item;
                }
            }
            mTabModel.removeRow(row);
        }
    });
}

void MainWindow::addNewSession(int sessId, Content &&c)
{
    mChatWidgets[sessId] = new ChatWidget();
    mChatWidgets[sessId]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *w = mChatWidgets.at(sessId);
    w->content() = std::move(c);
    auto id = ui->swChats->addWidget(w);
    w->setIndexW(id);
    ui->swChats->setCurrentIndex(id);
    mTabModel.item(sessId)->setData(sessId);
}

void MainWindow::on_lvTabs_clicked(const QModelIndex &index)
{
    if(mLastRow != index.row()){
        //mTextEdit->clear();
        loadItem(index.row());
    }
    mLastRow = index.row();
}

void MainWindow::onSendReasoning(QString text)
{
    if(!mChatWidgets.count(mSessionId)) return;
    auto *item = mChatWidgets.at(mSessionId);
    if(item) item->onSendReasoning(text);
}

void MainWindow::onSendContent(QString text)
{
    if(!mChatWidgets.count(mSessionId)) return;
    auto *item = mChatWidgets.at(mSessionId);
    if(item) item->onSendContent(text);
}

void MainWindow::onGenerationFinished()
{
    if(!mChatWidgets.count(mSessionId)) return;
    auto *item = mChatWidgets.at(mSessionId);
    if(item){
        item->content().context = mWorker->messages();
        item->generationFinished();
    }
}

void MainWindow::on_pbClearAll_clicked()
{
    for(auto &kv: mChatWidgets){
        ui->swChats->removeWidget(kv.second);
        kv.second->deleteLater();
        kv.second = nullptr;
    }
    mTabModel.clear();
    mChatWidgets.clear();
}


