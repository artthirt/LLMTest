#ifndef DLGSTYLE_H
#define DLGSTYLE_H

#include "chatview.h"

#include <QDialog>

class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QLabel;
class QLineEdit;
class QToolButton;
class QCheckBox;

class DlgStyle: public QDialog
{
    Q_OBJECT
public:
    DlgStyle();

    void setStyle(const Style s);

    Style style() const;

private:
    Style mStyle;

    QToolButton *mTextColorBtn{};
    QToolButton *mBackColorBtn{};
    QCheckBox   *mBold{};
    QCheckBox   *mItalic{};
    QLabel      *mLblTxt{};
    QLabel      *mLblBck{};
};

#endif // DLGSTYLE_H
