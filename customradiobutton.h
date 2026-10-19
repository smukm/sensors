#ifndef CUSTOMRADIOBUTTON_H
#define CUSTOMRADIOBUTTON_H

#include <QWidget>

class CustomRadioButton : public QWidget
{
    Q_OBJECT
public:
    explicit CustomRadioButton(const QString &text, QButtonGroup *group, QWidget *parent = nullptr);

signals:
};

#endif // CUSTOMRADIOBUTTON_H
