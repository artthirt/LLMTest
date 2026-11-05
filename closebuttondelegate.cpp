#include "closebuttondelegate.h"

#include <QPainter>
#include <QEvent>
#include <QMouseEvent>
#include <QPalette>
#include <qabstractitemview.h>
#include <QToolTip>

CloseButtonDelegate::CloseButtonDelegate(QObject *parent): QStyledItemDelegate(parent)
{
    auto *view = qobject_cast<QAbstractItemView *>(parent);

    if (view)
    {
        view->setMouseTracking(true);
        view->viewport()->setMouseTracking(true);
    }
}

void CloseButtonDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    painter->save();

    bool hovered = (index == mHoveredIndex);

    QRect buttonRect = closeButtonRect(option);

    QPalette pal = qApp->palette();

    painter->setPen(Qt::NoPen);
    if(!hovered){
        painter->setBrush(Qt::NoBrush); //pal.color(QPalette::Button));
    }else{
        auto col = pal.color(QPalette::Window);
        col.setAlpha(40);
        painter->setBrush(col);
    }

    painter->drawRoundedRect(buttonRect, 4, 4);

    painter->setPen(Qt::white);
    painter->drawText(buttonRect, Qt::AlignCenter, "❌");

    painter->restore();
}

bool CloseButtonDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                      const QStyleOptionViewItem &option, const QModelIndex &index)
{
    auto *view = qobject_cast<QAbstractItemView *>(parent());

    if (event->type() == QEvent::MouseButtonRelease){
        auto *mouseEvent = static_cast<QMouseEvent *>(event);

        if (mouseEvent->button() == Qt::LeftButton &&
            closeButtonRect(option).contains(
                mouseEvent->position().toPoint()))
        {
            emit closeRequested(index);
            return true;
        }
    }

    if (!view)
        return false;

    if (event->type() == QEvent::MouseMove)
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);

        QModelIndex hovered;

        if (closeButtonRect(option).contains(
                mouseEvent->position().toPoint()))
        {
            hovered = index;
        }

        if (hovered != mHoveredIndex){
            QModelIndex previous = mHoveredIndex;
            mHoveredIndex = hovered;

            if (previous.isValid())
                view->viewport()->update(
                    view->visualRect(previous));

            if (mHoveredIndex.isValid())
                view->viewport()->update(
                    view->visualRect(mHoveredIndex));
        }

        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease)
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);

        if (mouseEvent->button() == Qt::LeftButton &&
            closeButtonRect(option).contains(
                mouseEvent->position().toPoint()))
        {
            emit closeRequested(index);
            return true;
        }
    }

    return false;
}

bool CloseButtonDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (closeButtonRect(option).contains(event->pos()))
    {
        QToolTip::showText(event->globalPos(), tr("Close"), view);
        return true;
    }

    return QStyledItemDelegate::helpEvent(
        event, view, option, index);
}

QRect CloseButtonDelegate::closeButtonRect(const QStyleOptionViewItem &option) const
{
    constexpr int buttonSize = 20;
    constexpr int margin = 4;

    return QRect(
        option.rect.right() - buttonSize - margin,
        option.rect.center().y() - buttonSize / 2,
        buttonSize,
        buttonSize
        );
}