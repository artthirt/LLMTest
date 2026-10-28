#ifndef CIRCULARPROGRESS_H
#define CIRCULARPROGRESS_H

#include <QWidget>

class CircularProgress : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int progress READ progress WRITE setProgress)

public:
    explicit CircularProgress(QWidget *parent = nullptr);

    int progress() const { return m_progress; }

public slots:
    void setProgress(int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;

private:
    int m_progress = 0;
};

#endif // CIRCULARPROGRESS_H
