#include "chatview.h"

#include <QPainter>
#include <QTextLayout>
#include <QScrollBar>
#include <QPaintEvent>
#include <QJsonArray>
#include <qapplication.h>
#include <qclipboard.h>
#include <qmenu.h>

#include <chrono>
#include <qtimer.h>

ChatView::ChatView(QWidget *parent): QAbstractScrollArea(parent)
{
    {
        mUserStyle.italic = false;
        mUserStyle.background = QColor(Qt::gray).lighter(60);
    }
    {
        mReasoningStyle.italic = true;
        mReasoningStyle.background = QColor(Qt::gray).lighter(40);
        mReasoningStyle.background.setBlue(mReasoningStyle.background.blue() + 10);
    }
    {
        mContentStyle.italic = false;
        mContentStyle.background = QColor(Qt::gray).lighter(40);
    }
    auto *v = verticalScrollBar();
    v->setSingleStep(10);
}

ChatView::~ChatView()
{
}

bool ChatView::empty() const
{
    return mBlocks.empty();
}

void ChatView::clearContent()
{
    mBlocks.clear();
    relayout();
}

ChatView::ID ChatView::addUser(const QString &text)
{
    return addBlock(Block::User, text);
}

ChatView::ID ChatView::addAgentReasoning(const QString &text)
{
    return addBlock(Block::Reasoning, text);
}

ChatView::ID ChatView::addAgentContent(const QString &text)
{
    return addBlock(Block::Content, text);
}

void ChatView::setContent(ID id, const QString &text)
{
    if(id >= 0 && id < mBlocks.size()){
        mBlocks[id].text = text;
        relayout();
    }
}

void ChatView::finalize(ID id)
{
    if(id >= 0 && id < mBlocks.size()){
        qDebug() << "finalize" << id;
        mBlocks[id].formatted = true;
        relayout();
    }
}

void ChatView::resizeEvent(QResizeEvent *event)
{
    relayout();
    QAbstractScrollArea::resizeEvent(event);
}

void ChatView::paintEvent(QPaintEvent *event)
{
    static auto start = std::chrono::steady_clock::now();
    auto lstart = std::chrono::steady_clock::now();

    QPainter painter(viewport());

    painter.fillRect(event->rect(), mBackground);

    int scroll = verticalScrollBar()->value();
    auto r = event->rect();
    int y0 = scroll + r.top() - mGap;
    int y1 = scroll + r.bottom() + mGap;

    painter.setPen(Qt::black);
    for(int i = findFirstVisibleIndex(y0); i < mBlocks.size() && mBlocks[i].y < y1; ++i){
        auto &b = mBlocks[i];
        if(b.text.isEmpty()) continue;
        int y = b.y - scroll;
        int x = mMargin;
        if(b.type == Block::User){
            x = viewport()->width() - mMargin - b.width - 2 * mPadding;
        }else{

        }
        painter.save();
        painter.translate(x, y);
        QRect r(0, 0, b.width + 2 * mPadding, b.height);
        applyStyle(painter, r, getStyle(b.type));

        //int ty = mPadding;
        if(b.doc){
            painter.save();
            painter.translate(mPadding, mPadding);
            b.doc->drawContents(&painter, QRectF(0, 0, b.width, b.height - 2 * mPadding));
            painter.restore();
        }
        // for(const auto &l: b.layouts){
        //     for(int i = 0; i < l->lineCount(); ++i){
        //         auto l_ = l->lineAt(i);
        //         l_.draw(&painter, QPointF(mPadding, ty));
        //         ty += l_.height();
        //     }
        // }
        painter.restore();
    }

    auto ddur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    static int frames{};
    static int accddur{};
    accddur += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - lstart).count();
    frames++;
    if(ddur > 1000){
        auto fps = 1000.f * frames / ddur;
        float avg = frames > 0? accddur / frames : 0;
        qDebug() << "FPS(repaint) =" << fps << " | avg(repaint, ms) = " << avg/1000.f;
        frames = 0;
        accddur = 0;
        start = std::chrono::steady_clock::now();
    }
}

bool ChatView::viewportEvent(QEvent *event)
{
    if(event->type() == QEvent::MouseButtonPress){
        handlePress(static_cast<QMouseEvent*>(event));
        return true;
    }
    if(event->type() == QEvent::ContextMenu){
        auto *ce = static_cast<QContextMenuEvent*>(event);
        int y = ce->pos().y() + verticalScrollBar()->value();
        int index = blockIndexAt(y);
        if(index >= 0 && index < mBlocks.size()){
            QMenu menu(viewport());
            menu.addAction(tr("Copy"), this, [text = mBlocks[index].text] {
                QApplication::clipboard()->setText(text);
            });
            menu.addAction(tr("To markdown"), this, [this, index] {
                finalize(index);
            });
            menu.exec(ce->globalPos());
        }else{
            return false;
        }
        return true;
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void ChatView::applyStyle(QPainter &p, const QRect &r, const Style &style)
{
    if(style.background.isValid()){
        p.setBrush(style.background);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(r, 5, 5);
    }else{
        p.setBrush(Qt::NoBrush);
    }
    if(style.textColor.isValid()){
        p.setPen(style.textColor);
    }else{
        p.setPen(Qt::black);
    }
}

void ChatView::handlePress(QMouseEvent *me)
{
    int y = me->pos().y() + verticalScrollBar()->value();
    int index = blockIndexAt(y);
    for(auto& b: mBlocks){
        b.selected = false;
    }
    if(index >= 0 && index < mBlocks.size()){
        mBlocks[index].selected = true;
        if(me->button() == Qt::LeftButton && mBlocks[index].type == Block::Reasoning){
            mBlocks[index].hide = !mBlocks[index].hide;
            relayout();
        }
    }
}

Style ChatView::getStyle(Block::Type type)
{
    if(type == Block::Content){
        return mContentStyle;
    }else if(type == Block::Reasoning){
        return mReasoningStyle;
    }
    return mUserStyle;
}

ChatView::ID ChatView::addBlock(Block::Type type, const QString &text)
{
    Block b;
    b.type = type;
    b.text = text;
    b.id = mBlocks.size() - 1;

    // switch (type) {
    // case Block::User:{
    //     b.style = mUserStyle;
    // }
    // break;
    // case Block::Reasoning:{
    //     b.style = mReasoningStyle;
    // }
    // break;
    // case Block::Content:{
    //     b.style = mContentStyle;
    // }
    // break;
    // default:
    //     break;
    // }

    mBlocks.push_back(std::move(b));
    relayout();
    return mBlocks.size() - 1;
}

void ChatView::relayout()
{
    QTimer::singleShot(0, [this](){
        for(Block& b: mBlocks){
            relayoutBlock(b);
        }
        reposition();
        updateScrollbar();
        viewport()->update();
    });
}

void ChatView::setLeapToBottom(bool v)
{
    mLeapToBottom = v;
    relayout();
}

QJsonObject ChatView::toJson() const
{
    QJsonArray arr;
    QJsonObject obj;

    auto getJsonStyle = [this](const Style &s){
        QJsonObject o;
        o["bold"] = s.bold;
        o["italic"] = s.italic;
        o["textColor"] = s.textColor.name();
        o["background"] = s.background.name();
        return o;
    };

    obj["user_style"] = getJsonStyle(mUserStyle);
    obj["reasoning_style"] = getJsonStyle(mReasoningStyle);
    obj["content_style"] = getJsonStyle(mContentStyle);

    for(const auto &b: mBlocks){
        QJsonObject o;
        o["text"] = b.text;
        o["type"] = b.type;
        o["formatted"] = b.formatted;
        arr.append(o);
    }

    obj["messages"] = arr;
    return obj;
}

QByteArray ChatView::toJsonBA() const
{
    auto o = toJson();
    QJsonDocument d(o);
    return d.toJson(QJsonDocument::Compact);
}

void ChatView::setFromJson(const QJsonObject &obj)
{
    if(obj.empty() || !obj.contains("messages")){
        return;
    }
    auto arr = obj["messages"].toArray();
    if(arr.empty()) return;

    auto getStyle = [this](QJsonObject o){
        Style s;
        s.background = QColor(o["background"].toString());
        s.textColor = QColor(o["textColor"].toString());
        s.bold = o["bold"].toBool();
        s.italic = o["background"].toBool();
        return s;
    };

    if(obj.contains("user_style")){
        mUserStyle = getStyle(obj["user_style"].toObject());
        mReasoningStyle = getStyle(obj["reasoning_style"].toObject());
        mContentStyle = getStyle(obj["content_style"].toObject());
    }

    clearContent();
    for(int i = 0; i < arr.size(); ++i){
        auto o = arr[i].toObject();
        Block b;
        b.text = o["text"].toString();
        b.type = (Block::Type)o["type"].toInt();
        b.formatted = o["formatted"].toBool();
        mBlocks.push_back(std::move(b));
    }
    relayout();
}

void ChatView::setFromJson(const QByteArray &obj)
{
    QJsonObject o = QJsonDocument::fromJson(obj).object();
    setFromJson(o);
}

int ChatView::blockIndexAt(int y)
{
    int i = 0;
    for(const auto &b: mBlocks){
        if(y >= b.y && y < b.y + b.height){
            return i;
        }
        ++i;
    }
    return NONE_ID;
}

void ChatView::relayoutBlock(Block &b)
{
    int w = width() - 2 * mMargin;
    int maxText = w - w * mEmptySpaceMessage;

    //b.layouts.clear();

    auto style = getStyle(b.type);

    auto fi = fontInfo();
    QFont f(fi.family());
    f.setBold(style.bold);
    f.setItalic(style.italic);

    QFontMetricsF fm(font());
    // int textHeight{};
    // int width_{};
    makeDocument(b, f, maxText);
    // if(b.hide){
        // auto layout = makeLayout("    ...    ", f, maxText, width_, textHeight);
        // b.layouts.push_back(std::move(layout));
    // }else{
        // b.doc = makeDocument(b.text, b, f, maxText, width_, textHeight);
        // for(const auto &text: b.text.split('\n')){
        //     auto layout = makeLayout(text, f, maxText, width_, textHeight);
        //     b.layouts.push_back(std::move(layout));
        // }
    // }

    // b.width = width_;
    // b.height = textHeight + 2 * mPadding;
}

std::unique_ptr<QTextLayout> ChatView::makeLayout(const QString& text, const QFont &f, int maxText, int &width_, int &textHeight)
{
    auto layout = std::make_unique<QTextLayout>();
    layoutPart(*layout, text, f, maxText, width_);
    for(int i = 0; i < layout->lineCount(); ++i){
        textHeight += layout->lineAt(i).height();
    }
    return std::move(layout);
}

void ChatView::makeDocument(Block &b, const QFont &f, int maxText)
{
    if(!b.doc)
        b.doc = std::make_unique<QTextDocument>();
    b.doc->clear();
    b.doc->setDocumentMargin(0);
    //}
    if(b.hide)           b.doc->setPlainText("...");
    else if(b.formatted) b.doc->setMarkdown(b.text);
    else                 b.doc->setPlainText(b.text);
    b.doc->setTextWidth(maxText);

    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    b.doc->setDefaultTextOption(opt);
    b.doc->setDefaultFont(f);

    int natural = b.doc->idealWidth();
    int width_ = maxText > 0? std::min(maxText, natural) : natural;
    if(width_ < natural)
        b.doc->setTextWidth(width_);

    int textHeight = b.doc->size().height();

    b.width = width_;
    b.height = textHeight + 2 * mPadding;
}

void ChatView::layoutPart(QTextLayout &tl, const QString text, QFont font, int maxWidth, int &width_)
{
    tl.setFont(font);
    tl.setText(text);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    tl.setTextOption(opt);

    tl.beginLayout();
    QTextLine line = tl.createLine();
    while(line.isValid())
        line = tl.createLine();
    tl.endLayout();

    int natural = tl.boundingRect().width();
    int w = maxWidth > 0? std::min(maxWidth, natural) : natural;
    if(w < natural){
        tl.beginLayout();
        line = tl.createLine();
        while(line.isValid()){
            line.setLineWidth(w);
            line = tl.createLine();
        }
        tl.endLayout();
    }
    width_ = std::max(width_, w);
}

void ChatView::reposition()
{
    int y = mMargin;
    for(auto& b: mBlocks){
        b.y = y;
        y += b.height + mGap;
    }
    mContentHeight = mBlocks.empty()? 0 : y - mGap + mMargin;
}

int ChatView::findFirstVisibleIndex(int y0)
{
    int id = 0;
    for(int i = 0; i < mBlocks.size(); ++i){
        if(mBlocks[i].y + mBlocks[i].height >= y0){
            return i;
        }
    }
    return id;
}

void ChatView::updateScrollbar()
{
    auto *v = verticalScrollBar();
    auto nv = v->value();
    auto mv = v->maximum();
    v->setRange(0, std::max(0, mContentHeight - viewport()->height()));
    v->setPageStep(viewport()->height());
    if(mLeapToBottom && std::abs(mv - nv) < 3){
        v->setValue(v->maximum());
    }else{
        v->setValue(std::min(v->value(), v->maximum()));
    }
    horizontalScrollBar()->setRange(0, 0);
}

