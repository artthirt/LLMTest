#ifndef DIALOGCONFIG_H
#define DIALOGCONFIG_H

#include "llamaworker.h"
#include <QDialog>
#include <QStandardItemModel>
#include <qsettings.h>

namespace Ui {
class DialogConfig;
}

class ResizeEventFilter : public QObject
{
public:
    ResizeEventFilter(QObject *parent, std::function<void()> callback);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    std::function<void()> m_callback;
};

class QPlainTextEdit;

class DialogConfig : public QDialog
{
    Q_OBJECT

public:
    explicit DialogConfig(QWidget *parent = nullptr);
    ~DialogConfig();

    void setParams(const LlamaWorker::Params &params);
    LlamaWorker::Params params() const;

    void setDevices(const QStringList &devices);
    QStringList selectedDevices() const;

    QString systemPrompt() const;

    QString modelPath() const;

    void loadSettings();
    void saveSettings();

    QSettings* settings();

    void updateParams();

private slots:
    void on_buttonBox_accepted();

    void on_tbtSelect_clicked();

private:
    Ui::DialogConfig *ui;
    QStandardItemModel mDevsModel;
    LlamaWorker::Params mParams;
    QStringList mDevices;
    QSettings mSetts;

    void makeToolButton(QPlainTextEdit* plainTextEdit,
                        const QString &text,
                        std::function<void()> fun,
                        const QString &tooltip = {},
                        int X = 0);
};

#endif // DIALOGCONFIG_H
