#include "chatwidget.h"
#include "ui_chatwidget.h"

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatWidget)
{
    ui->setupUi(this);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *v = layout(); //new QVBoxLayout;
    //setLayout(v);
    v->addWidget(&mChatView);
}

ChatWidget::~ChatWidget()
{
    delete ui;
}

void ChatWidget::addUser(const QString &msg)
{
    mChatView.addUser(msg);
    mChatReasonongId = ChatView::NONE_ID;
    mChatContentId = ChatView::NONE_ID;
    mContent.json = mChatView.toJsonBA();
}

void ChatWidget::generationFinished()
{
    mChatContentId = mChatReasonongId = -1;
    mContent.json = mChatView.toJsonBA();
}

void ChatWidget::onSendReasoning(QString text)
{
    if(mChatReasonongId == ChatView::NONE_ID){
        mChatReasonongId = mChatView.addAgentReasoning(text);
    }else{
        mChatView.setContent(mChatReasonongId, text);
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
