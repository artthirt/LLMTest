#include "dlgstyle.h"

#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>

void setLabelColor(QLabel* lbl, QColor c)
{
    lbl->setFixedSize(24, 24);
    auto style = QString(R"(
    border-radius: 12px;
    background-color: "%1";
)").arg(c.name());
    qDebug() << style;
    lbl->setStyleSheet(style);
}

DlgStyle::DlgStyle()
    : QDialog()
{
    setWindowTitle("Set Style");

    auto *grid = new QGridLayout();
    setLayout(grid);

    grid->addWidget(new QLabel("Text Color"), 0, 0);
    mLblTxt = new QLabel;
    grid->addWidget(mLblTxt, 0, 1);
    mTextColorBtn = new QToolButton();
    mTextColorBtn->setText("🎨");
    grid->addWidget(mTextColorBtn, 0, 2);
    connect(mTextColorBtn, &QToolButton::clicked, this, [this](){
        QColorDialog dlg;
        dlg.setCurrentColor(mStyle.textColor);
        if(dlg.exec()){
            mStyle.textColor = dlg.currentColor();
            setLabelColor(mLblTxt, dlg.currentColor());
        }
    });

    grid->addWidget(new QLabel("Background Color"), 1, 0);
    mLblBck = new QLabel;
    grid->addWidget(mLblBck, 1, 1);
    mBackColorBtn = new QToolButton();
    mBackColorBtn->setText("🎨");
    grid->addWidget(mBackColorBtn, 1, 2);
    connect(mBackColorBtn, &QToolButton::clicked, this, [this](){
        QColorDialog dlg;
        dlg.setCurrentColor(mStyle.background);
        if(dlg.exec()){
            mStyle.background = dlg.currentColor();
            setLabelColor(mLblBck, dlg.currentColor());
        }
    });

    mBold = new QCheckBox("Bold");
    connect(mBold, &QCheckBox::clicked, this, [this](bool ok){
        mStyle.bold = ok;
    });
    grid->addWidget(mBold, 2, 0);
    mItalic = new QCheckBox("Italic");
    connect(mItalic, &QCheckBox::clicked, this, [this](bool ok){
        mStyle.italic = ok;
    });
    grid->addWidget(mItalic, 3, 0);

    auto *pbOk = new QPushButton("Ok");
    connect(pbOk, &QPushButton::clicked, this, [this](){ accept(); });
    grid->addWidget(pbOk, 4, 0);
    auto *pbCancel = new QPushButton("Cancel");
    connect(pbCancel, &QPushButton::clicked, this, [this](){ reject(); });
    grid->addWidget(pbCancel, 4, 1);
}

void DlgStyle::setStyle(const Style s)
{
    mStyle = s;

    setLabelColor(mLblBck, s.background);
    setLabelColor(mLblTxt, s.textColor);
    mBold->setCheckState(s.bold? Qt::Checked : Qt::Unchecked);
    mItalic->setCheckState(s.italic? Qt::Checked : Qt::Unchecked);
}

Style DlgStyle::style() const
{
    return mStyle;
}
