#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QStandardItemModel>

#include "chatwidget.h"
#include "dialogconfig.h"
#include "circularprogress.h"

class LlamaWorker;

class QTextEdit;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pbLoad_clicked();

    void on_pbSend_clicked();

    void on_pbUnload_clicked();

    void onSendStatus(QString msg, bool ok);

    void onSendPiece(QString piece);

    void onTimeout();

    void onAppendMessage(const QString& msg);

    void on_tbtConfig_clicked();

    void on_pbNewSession_clicked();

    void on_lvTabs_clicked(const QModelIndex &index);

    void onSendReasoning(QString text);
    void onSendContent(QString);
    void onGenerationFinished();

    void on_pbClearAll_clicked();

    void on_lvTabs_doubleClicked(const QModelIndex &index);

    void onCloseItemRequested(const QModelIndex &index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Ui::MainWindow *ui;
    QStandardItemModel mTabModel;
    DialogConfig mDlgConfig;
    //std::map<int, Content> mCurrentContent;
    std::map<int, ChatWidget*> mChatWidgets;
    CircularProgress mContextIndicator;
    int mLastRow{};
    int mSessionId{};

    //QTextEdit *mTextEdit{};

    QTimer mTimer;
    bool mIsLoading{};
    int mLoadingCnt{};

    std::unique_ptr<LlamaWorker> mWorker;

    void sendMessage();

    void loadSettings();
    void saveSettings();

    void updateTabs();

    void loadItem(int row);
    void addNewSession(int sessId, Content &&c);

    void makeToolButton(QWidget *widget,
                        const QString &text,
                        std::function<void()> fun,
                        const QString &tooltip,
                        int X);
};
#endif // MAINWINDOW_H
