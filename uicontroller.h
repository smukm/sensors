#ifndef UICONTROLLER_H
#define UICONTROLLER_H

#include "./ui_mainwindow.h"

class UiController
{
public:
    explicit UiController(Ui::MainWindow *ui);

    void setConnectedState();
    void setDisconnectedState();

private:
    Ui::MainWindow *m_ui;
};

#endif // UICONTROLLER_H
