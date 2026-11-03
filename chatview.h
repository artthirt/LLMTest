#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <QVector>
#include <QAbstractScrollArea>
#include <QJsonObject>
#include <vector>
#include <memory>

class QTextLayout;

struct Style{
    bool bold{};
    bool italic{};
    QColor textColor{Qt::white};
    QColor background{};
};

struct Block{
    enum Type{
        User, Reasoning, Content
    };

    Type type{User};
    //Style style;
    int id{-1};
    int y{};
    QString text;
    int width{};
    int height{};
    bool selected{};
    bool hide{};
    std::vector<std::unique_ptr<QTextLayout>> layouts;
};

class ChatView: public QAbstractScrollArea
{
public:
    using ID = int;
    enum{NONE_ID = -1};

    ChatView(QWidget *parent = nullptr);
    ~ChatView();

    bool empty() const;

    ID addUser(const QString& text);
    ID addAgentReasoning(const QString& text);
    ID addAgentContent(const QString& text);

    void clearContent();
    void setContent(ID id, const QString& text);
    void relayout();

    bool leapToBottom() const {return mLeapToBottom; }
    void setLeapToBottom(bool v);

    QJsonObject toJson() const;
    QByteArray toJsonBA() const;
    void setFromJson(const QJsonObject &obj);
    void setFromJson(const QByteArray &obj);

    int blockIndexAt(int y);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    bool viewportEvent(QEvent *event) override;

private:
    std::vector<Block> mBlocks;
    int mMargin{4};
    int mPadding{8};
    int mGap{2};
    int mContentWidth{};
    int mContentHeight{};
    QColor mBackground{Qt::darkGray};
    bool mLeapToBottom{true};
    float mEmptySpaceMessage{1./8.};

    Style mUserStyle;
    Style mReasoningStyle;
    Style mContentStyle;

    ID addBlock(Block::Type type, const QString& text);
    void relayoutBlock(Block& b);
    void updateScrollbar();
    void layoutPart(QTextLayout& tl, const QString line, QFont font, int maxWidth, int &width_);
    void reposition();
    int findFirstVisibleIndex(int y0);
    void applyStyle(QPainter &p, const QRect& r, const Style &style);
    void handlePress(QMouseEvent* me);
    Style getStyle(Block::Type type);
    std::unique_ptr<QTextLayout> makeLayout(const QString& text, const QFont &f, int maxText, int &width_, int &textHeight);
};

#endif // CHATVIEW_H
