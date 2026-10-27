#include "dialogconfig.h"
#include "ui_dialogconfig.h"

#include <QToolButton>
#include <QTimer>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>

class ResizeEventFilter : public QObject
{
public:
    ResizeEventFilter(QObject *parent, std::function<void()> callback)
        : QObject(parent), m_callback(std::move(callback))
    {
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::Resize) {
            m_callback();
        }

        return QObject::eventFilter(obj, event);
    }

private:
    std::function<void()> m_callback;
};

////////////////////////////////

DialogConfig::DialogConfig(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogConfig)
    //, mSetts("LLMTest", "LLMTest")
    , mSetts("config_LLMTest.ini", QSettings::IniFormat)
{
    ui->setupUi(this);

    ui->tvDevices->setModel(&mDevsModel);
    ui->tvDevices->horizontalHeader()->setStretchLastSection(true);
    mDevsModel.setColumnCount(2);
    mDevsModel.setHorizontalHeaderLabels({"✔", "Device"});

    makeToolButton(ui->pteSystemPrompt, "❌", [this](){
        ui->pteSystemPrompt->clear();
    }, "Clear");

    makeToolButton(ui->pteSystemPrompt, "📋", [this](){
        qApp->clipboard()->setText(ui->pteSystemPrompt->toPlainText());
    }, "Copy", 20);
}

DialogConfig::~DialogConfig()
{
    delete ui;
}

void DialogConfig::setParams(const LlamaWorker::Params &params)
{
    mParams = params;

    ui->dsbTemp->setValue(mParams.temp);
    ui->sbGpuLayers->setValue(mParams.gpu_layers);
    ui->sbMaxContext ->setValue(mParams.context);
    ui->pteSystemPrompt->setPlainText(mParams.system_prompt);
}

LlamaWorker::Params DialogConfig::params() const
{
    return mParams;
}

void DialogConfig::setDevices(const QStringList &devices)
{
    mDevices = devices;
    mDevsModel.clear();
    for(int i = 0; i < mDevices.size(); i += 2){
        QStandardItem *col1 = new QStandardItem("");
        col1->setCheckable(true);
        col1->setCheckState(Qt::CheckState::Unchecked);
        QStandardItem *col2 = new QStandardItem(mDevices[i + 1]);
        col2->setData(mDevices[i]);
        mDevsModel.appendRow({col1, col2});
    }
}

QStringList DialogConfig::selectedDevices() const
{
    QStringList ret;
    for(int i = 0; i < mDevsModel.rowCount(); ++i){
        auto *col1 = mDevsModel.item(i, 0);
        auto *col2 = mDevsModel.item(i, 1);
        if(col1->checkState() == Qt::Checked){
            ret.push_back(col2->data().toString());
        }
    }
    return ret;
}

QString DialogConfig::systemPrompt() const
{
    return ui->pteSystemPrompt->toPlainText();
}

QString DialogConfig::modelPath() const
{
    return ui->lePath->text();
}

QSettings *DialogConfig::settings()
{
    return &mSetts;
}

void DialogConfig::loadSettings()
{
    ui->lePath->setText(mSetts.value("path").toString());
    ui->sbGpuLayers->setValue(mSetts.value("gpu_layers", -1).toInt());
    ui->sbMaxContext->setValue(mSetts.value("max_context", 8192).toInt());
    ui->dsbTemp->setValue(mSetts.value("temp", 0.8).toDouble());
    ui->pteSystemPrompt->setPlainText(mSetts.value("system_prompt").toString());
    ui->chbThinking->setChecked(mSetts.value("thinking").toBool());

    updateParams();
}

void DialogConfig::saveSettings()
{
    mSetts.setValue("path", ui->lePath->text());
    mSetts.setValue("gpu_layers", ui->sbGpuLayers->value());
    mSetts.setValue("max_context", ui->sbMaxContext->value());
    mSetts.setValue("temp", ui->dsbTemp->value());
    mSetts.setValue("system_prompt", ui->pteSystemPrompt->toPlainText());
    mSetts.setValue("thinking", ui->chbThinking->isChecked());
}

void DialogConfig::updateParams()
{
    mParams.context = ui->sbMaxContext->value();
    mParams.gpu_layers = ui->sbGpuLayers->value();
    mParams.temp = ui->dsbTemp->value();
    mParams.system_prompt = ui->pteSystemPrompt->toPlainText();
    mParams.enable_thinking = ui->chbThinking->isChecked();
}

void DialogConfig::on_buttonBox_accepted()
{
    mParams.context = ui->sbMaxContext->value();
    mParams.gpu_layers = ui->sbGpuLayers->value();
    mParams.temp = ui->dsbTemp->value();
    mParams.system_prompt = ui->pteSystemPrompt->toPlainText();
    mParams.enable_thinking = ui->chbThinking->isChecked();

    auto spl = ui->leSplitted->text().split(",");
    QVector<float> vspl;
    for(auto sf: spl){
        bool ok{};
        auto f = sf.toFloat(&ok);
        if(ok){
            vspl.push_back(f);
        }
    }
    QStringList ldev;
    for(int i = 0; i < mDevsModel.rowCount(); ++i){
        auto item = mDevsModel.item(i, 0);
        auto item1 = mDevsModel.item(i, 1);
        if(item->checkState() == Qt::Checked){
            auto s = item1->data().toString();
            if(!s.isEmpty()){
                ldev.push_back(s);
            }
        }
    }

    mParams.splitBetweenDevices = vspl;
    mParams.selectedDevices = ldev;
}

void DialogConfig::makeToolButton(QPlainTextEdit* plainTextEdit,
                                  const QString &text,
                                  std::function<void()> fun,
                                  const QString &tooltip,
                                  int X)
{
    auto *button = new QToolButton(plainTextEdit);
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

    auto updatePosition = [plainTextEdit, button, X]() {
        button->move(plainTextEdit->width() - button->width() - 8 - X, 4);
        button->raise();
    };

    QObject::connect(plainTextEdit, &QPlainTextEdit::textChanged,
                     this, updatePosition); // Optional; not needed for resizing.

    auto *filter = new ResizeEventFilter(
        plainTextEdit, updatePosition);

    plainTextEdit->installEventFilter(filter);

    updatePosition();
}

void DialogConfig::on_tbtSelect_clicked()
{
    QString path = mSetts.value("path").toString();
    QString sdir = QFileInfo(path).absolutePath();
    path = QFileDialog::getOpenFileName(nullptr, "GGUF model", sdir, "*.gguf");
    if(!path.isEmpty()){
        ui->lePath->setText(path);
    }
    saveSettings();
}