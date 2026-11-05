#include "chatwidget.h"
#include "dialogconfig.h"
#include "ui_chatwidget.h"
#include <qtoolbutton.h>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatWidget)
{
    ui->setupUi(this);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *v = layout(); //new QVBoxLayout;
    //setLayout(v);
    auto tbt = new QToolButton();
    tbt->setText("🧹");
    tbt->setToolTip("Clear");
    tbt->setFixedSize(24, 24);

    connect(tbt, &QToolButton::clicked, this, [this](){
        mChatView.clearContent();
        mContent.json = {};
        mContent.context = {};
    });

    v->addWidget(tbt);
    v->addWidget(&mChatView);

    // makeToolButton(this, "🧹", [this](){
    //     mChatView.clearContent();
    //     mContent.json = {};
    //     mContent.context = {};
    // }, "Clear", 0);
}

ChatWidget::~ChatWidget()
{
    delete ui;
}

void ChatWidget::addUser(const QString &msg)
{
    mChatView.addUser(msg);
    mChatReasoningId = ChatView::NONE_ID;
    mChatContentId = ChatView::NONE_ID;
    mContent.json = mChatView.toJsonBA();
}

void ChatWidget::generationFinished()
{
    mChatView.finalize(mChatReasoningId);
    mChatView.finalize(mChatContentId);
    mChatContentId = mChatReasoningId = -1;
    mContent.json = mChatView.toJsonBA();
}

void ChatWidget::onSendReasoning(QString text)
{
    if(mChatReasoningId == ChatView::NONE_ID){
        mChatReasoningId = mChatView.addAgentReasoning(text);
    }else{
        mChatView.setContent(mChatReasoningId, text);
    }
}

void ChatWidget::onSendContent(QString text)
{
    if(mChatContentId == ChatView::NONE_ID){
        mChatContentId = mChatView.addAgentContent(text);
    }else{
        mChatView.setContent(mChatContentId, text);
    }
}

void ChatWidget::makeToolButton(QWidget *widget, const QString &text, std::function<void ()> fun, const QString &tooltip, int X)
{
    auto *button = new QToolButton(widget);
    button->setText(text);
    // button->setIcon(style()->standardIcon(
    //     QStyle::SP_MessageBoxInformation));
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setFixedSize(24, 24);

    auto p = qApp->palette();

    auto trn = [](QColor c, int a){
        return QString("rgba(%1, %2, %3, %4)")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
    };

    button->setStyleSheet(QString(R"(
    QToolButton {
        background-color: %1;
        border: 1px solid %2;
        border-radius: 3px;
    }
    QToolButton:hover {
        background-color: %3;
        border-radius: 4px;
    }
    QToolButton:pressed {
        background-color: %4;
    }
    )")
        .arg(trn(p.color(QPalette::Button), 200))
        .arg(trn(p.color(QPalette::Dark), 200))
        .arg(trn(p.color(QPalette::Midlight), 200))
        .arg(trn(p.color(QPalette::Mid), 200)));

    QObject::connect(button, &QToolButton::clicked, this, [this, fun]() {
        // Your action here
        if(fun){
            fun();
        }
    });

    auto updatePosition = [widget, button, X]() {
        button->move(14 + X, 14);
        button->raise();
    };

    // QObject::connect(plainTextEdit, &QTextEdit::textChanged,
    //                  this, updatePosition); // Optional; not needed for resizing.

    auto *filter = new ResizeEventFilter(
        widget, updatePosition);

    widget->installEventFilter(filter);

    updatePosition();
}
