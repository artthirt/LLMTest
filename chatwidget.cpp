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
