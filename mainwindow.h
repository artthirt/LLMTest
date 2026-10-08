#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include "llamaworker.h"

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
    void on_tbtSelect_clicked();

    void on_pbLoad_clicked();

    void on_pbSend_clicked();

    void on_pbUnload_clicked();

    void onSendStatus(QString msg, bool ok);

    void onTimeout();

private:
    Ui::MainWindow *ui;

    QTimer mTimer;
    bool mIsLoading{};
    int mLoadingCnt{};

    std::unique_ptr<LlamaWorker> mWorker;
    QSettings mSetts;

    void loadSettings();
    void saveSettings();
};
#endif // MAINWINDOW_H
