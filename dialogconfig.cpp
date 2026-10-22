#include "dialogconfig.h"
#include "ui_dialogconfig.h"

DialogConfig::DialogConfig(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogConfig)
{
    ui->setupUi(this);

    ui->tvDevices->setModel(&mDevsModel);
    ui->tvDevices->horizontalHeader()->setStretchLastSection(true);
    mDevsModel.setColumnCount(2);
    mDevsModel.setHorizontalHeaderLabels({"✔", "Device"});
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

void DialogConfig::loadSettings(QSettings &setts)
{
    ui->sbGpuLayers->setValue(setts.value("gpu_layers", -1).toInt());
    ui->sbMaxContext->setValue(setts.value("max_context", 8192).toInt());
    ui->dsbTemp->setValue(setts.value("temp", 0.8).toDouble());
    ui->pteSystemPrompt->setPlainText(setts.value("system_prompt").toString());
}

void DialogConfig::saveSettings(QSettings &setts)
{
    setts.setValue("gpu_layers", ui->sbGpuLayers->value());
    setts.setValue("max_context", ui->sbMaxContext->value());
    setts.setValue("temp", ui->dsbTemp->value());
    setts.setValue("system_prompt", ui->pteSystemPrompt->toPlainText());
}

void DialogConfig::on_buttonBox_accepted()
{
    mParams.context = ui->sbMaxContext->value();
    mParams.gpu_layers = ui->sbGpuLayers->value();
    mParams.temp = ui->dsbTemp->value();
    mParams.system_prompt = ui->pteSystemPrompt->toPlainText();

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

