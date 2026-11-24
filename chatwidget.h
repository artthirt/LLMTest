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

    void addContent(Content&& c);

    void addUser(const QString &msg);

    void setIndexW(int v) { mIndexW = v; }
    int indexW() const { return mIndexW; }

    void setUserStyle(const Style &s);
    Style userStyle() const;
    void setReasoningStyle(const Style &s);
    Style reasoningStyle() const;
    void setContentStyle(const Style &s);
    Style contentStyle() const;
    void setBackgroundColor(QColor c);
    QColor backgroundColor() const;

    QByteArray toJsonStyleBA() const;
    void setFromJsonStyle(const QByteArray &obj);

    void generationFinished();

signals:
    void clearContent(int64_t sessId);

public slots:
    void onSendReasoning(QString text);
    void onSendContent(QString text);

private:
    Ui::ChatWidget *ui;
    int mIndexW{-1};

    Content mContent;
    ChatView mChatView;
    int mChatReasoningId{-1};
    int mChatContentId{-1};

    void makeToolButton(QWidget *widget,
                        const QString &text,
                        std::function<void()> fun,
                        const QString &tooltip,
                        int X);
};

#endif // CHATWIDGET_H
