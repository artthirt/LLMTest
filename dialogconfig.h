#ifndef DIALOGCONFIG_H
#define DIALOGCONFIG_H

#include "llamaworker.h"
#include <QDialog>
#include <QStandardItemModel>
#include <qsettings.h>

namespace Ui {
class DialogConfig;
}

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

    void loadSettings(QSettings &setts);
    void saveSettings(QSettings &setts);

private slots:
    void on_buttonBox_accepted();

private:
    Ui::DialogConfig *ui;
    QStandardItemModel mDevsModel;
    LlamaWorker::Params mParams;
    QStringList mDevices;
};

#endif // DIALOGCONFIG_H
