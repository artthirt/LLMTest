#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>

#include "chatview.h"
#include "llamaworker.h"

namespace Ui {
class ChatWidget;
}

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget();

    ChatView& chat()   { return mChatView; }
    Content& content() { return mContent; }

    void addUser(const QString &msg);

    void setIndexW(int v) { mIndexW = v; }
    int indexW() const { return mIndexW; }

    void generationFinished();

public slots:
    void onSendReasoning(QString text);
    void onSendContent(QString text);

private:
    Ui::ChatWidget *ui;
    int mIndexW{-1};

    Content mContent;
    ChatView mChatView;
    int mChatReasonongId{-1};
    int mChatContentId{-1};

};

#endif // CHATWIDGET_H
