#include "dialogconfig.h"
#include "ui_dialogconfig.h"

#include <QToolButton>
#include <QTimer>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>

////////////
ResizeEventFilter::ResizeEventFilter(QObject *parent, std::function<void ()> callback)
    : QObject(parent), m_callback(std::move(callback))
{
}

bool ResizeEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize) {
        m_callback();
    }

    return QObject::eventFilter(obj, event);
}

////////////////////////////////

DialogConfig::DialogConfig(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogConfig)
    //, mSetts("LLMTest", "LLMTest")
    , mSetts("config_LLMTest.ini", QSettings::IniFormat)
{
    ui->setupUi(this);

    auto keys = KVCacheType.keys();
    ui->cbKVKey->addItems(keys);
    ui->cbKVValue->addItems(keys);

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

LlamaWorker::Params DialogConfig::params() const
{
    auto params = updateParams();
    return params;
}

void DialogConfig::setDevices(const QStringList &devices)
{
    mDevices = devices;
    mDevsModel.clear();
    for(int i = 0; i < mDevices.size(); i += 2){
        QStandardItem *col1 = new QStandardItem("");
        col1->setCheckable(true);
        col1->setCheckState(mSelectedDevices.contains(mDevices[i])? Qt::Checked : Qt::Unchecked);
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
    return ui->cbWeights->currentText();
}

QSettings *DialogConfig::settings()
{
    return &mSetts;
}

void DialogConfig::loadSettings()
{
    ui->cbWeights->setCurrentText(mSetts.value("path").toString());
    ui->sbGpuLayers->setValue(mSetts.value("gpu_layers", -1).toInt());
    ui->sbMaxContext->setValue(mSetts.value("max_context", 8192).toInt());
    ui->dsbTemp->setValue(mSetts.value("temp", 0.8).toDouble());
    ui->pteSystemPrompt->setPlainText(mSetts.value("system_prompt").toString());
    ui->chbThinking->setChecked(mSetts.value("thinking").toBool());
    ui->sbMainGPU->setValue(mSetts.value("main_gpu", 0).toInt());
    ui->cbKVKey->setCurrentIndex(mSetts.value("kvkey", 0).toInt());
    ui->cbKVValue->setCurrentIndex(mSetts.value("kvval", 0).toInt());
    ui->leSplitted->setText(mSetts.value("splitting").toString());

    mSelectedDevices = mSetts.value("selected_devices").toStringList();
    mSystemPromptPresets = mSetts.value("system_prompt_presets").toMap();
    if(!mSystemPromptPresets.empty()){
        for(const auto &key: mSystemPromptPresets.keys()){
            auto lst = mSystemPromptPresets[key].toList();
            if(lst.size() >= 2){
                ui->cbPreset->addItem(lst[0].toString(), key);
            }
        }
        on_cbPreset_currentIndexChanged(0);
    }
    ui->cbPreset->setCurrentIndex(mSetts.value("system_prompt_index").toInt());

    auto weights = mSetts.value("weights").toStringList();
    auto weights_id = mSetts.value("weights_id").toInt();
    ui->cbWeights->addItems(weights);
    QTimer::singleShot(0, [this, weights, weights_id](){
        emit sendWeights(ui->cbWeights->itemText(weights_id), weights);
    });

    updateParams();
}

void DialogConfig::saveSettings()
{
    mSetts.setValue("path", ui->cbWeights->currentText());
    mSetts.setValue("gpu_layers", ui->sbGpuLayers->value());
    mSetts.setValue("max_context", ui->sbMaxContext->value());
    mSetts.setValue("temp", ui->dsbTemp->value());
    mSetts.setValue("system_prompt", ui->pteSystemPrompt->toPlainText());
    mSetts.setValue("thinking", ui->chbThinking->isChecked());
    mSetts.setValue("main_gpu", ui->sbMainGPU->value());
    mSetts.setValue("kvkey", ui->cbKVKey->currentIndex());
    mSetts.setValue("kvval", ui->cbKVValue->currentIndex());
    mSetts.setValue("splitting", ui->leSplitted->text());

    mSelectedDevices = selectedDevices();
    mSetts.setValue("selected_devices", mSelectedDevices);

    mSetts.setValue("system_prompt_presets", mSystemPromptPresets);
    mSetts.setValue("system_prompt_index", ui->cbPreset->currentIndex());

    mSetts.setValue("presets", mSystemPromptPresets);

    QStringList items;
    for(int i = 0; i < ui->cbWeights->count(); ++i){
        items << ui->cbWeights->itemText(i);
    }
    mSetts.setValue("weights", items);
    mSetts.setValue("weights_id", ui->cbWeights->currentIndex());
}

LlamaWorker::Params DialogConfig::updateParams() const
{
    LlamaWorker::Params Params;
    Params.context = ui->sbMaxContext->value();
    Params.gpu_layers = ui->sbGpuLayers->value();
    Params.temp = ui->dsbTemp->value();
    Params.system_prompt = ui->pteSystemPrompt->toPlainText();
    Params.enable_thinking = ui->chbThinking->isChecked();
    Params.main_gpu = ui->sbMainGPU->value();
    Params.type_k = KVCacheType.value(ui->cbKVKey->currentText());
    Params.type_v = KVCacheType.value(ui->cbKVValue->currentText());
    auto spl = ui->leSplitted->text().split(",");
    QVector<float> vspl;
    for(const auto &sf: spl){
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

    Params.splitBetweenDevices = vspl;
    Params.selectedDevices = ldev;
    return Params;
}

void DialogConfig::on_buttonBox_accepted()
{

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
        ui->cbWeights->setCurrentText(path);
        if(ui->cbWeights->findText(path) < 0)
            ui->cbWeights->addItem(path);
        QStringList items;
        for(int i = 0; i < ui->cbWeights->count(); ++i){
            items << ui->cbWeights->itemText(i);
        }
        emit sendWeights(ui->cbWeights->currentText(), items);

    }
    saveSettings();
}

void DialogConfig::on_cbPreset_currentIndexChanged(int index)
{
    auto key = ui->cbPreset->itemData(index).toString();
    if(mSystemPromptPresets.contains(key)){
        auto lst = mSystemPromptPresets[key].toList();
        if(lst.size() >= 2){
            ui->cbPreset->setCurrentText(lst[0].toString());
            ui->pteSystemPrompt->setPlainText(lst[1].toString());
        }
    }
}

void DialogConfig::on_pbSavePreset_clicked()
{
    auto sptext = ui->pteSystemPrompt->toPlainText();
    auto spcapt = ui->cbPreset->currentText();
    if(spcapt.isEmpty()){
        return;
    }
    int row = time(nullptr) + rand();
    auto srow = QString::number(row);
    ui->cbPreset->addItem(spcapt, srow);
    QVariantList sl;
    sl << spcapt << sptext;
    mSystemPromptPresets[srow] = sl;
}

void DialogConfig::on_tbRemovePreset_clicked()
{
    auto srow = ui->cbPreset->currentData().toString();
    if(mSystemPromptPresets.contains(srow)){
        auto id = ui->cbPreset->findData(srow);
        if(id >= 0){
            mSystemPromptPresets.remove(srow);
            ui->cbPreset->removeItem(id);

        }
    }else{
        ui->cbPreset->removeItem(ui->cbPreset->currentIndex());

    }
    on_cbPreset_currentIndexChanged(0);
}

