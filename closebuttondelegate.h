#ifndef CLOSEBUTTONDELEGATE_H
#define CLOSEBUTTONDELEGATE_H

#include <QStyledItemDelegate>

class CloseButtonDelegate: public QStyledItemDelegate
{
    Q_OBJECT
public:
    CloseButtonDelegate(QObject* parent = nullptr);

    // QAbstractItemDelegate interface
public:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;
    bool helpEvent(
        QHelpEvent *event,
        QAbstractItemView *view,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) override;

signals:
    void closeRequested(const QModelIndex &index);

private:
    QModelIndex mHoveredIndex{};

    QRect closeButtonRect(const QStyleOptionViewItem &option) const;
};

#endif // CLOSEBUTTONDELEGATE_H
