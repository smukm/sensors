#ifndef UICONTROLLER_H
#define UICONTROLLER_H

#include "./ui_mainwindow.h"

class UiController
{
public:
    explicit UiController(Ui::MainWindow *ui);

    void initUiElements();
    void setConnectedState();
    void setDisconnectedState();
    void saveBasePositions();
    void repositionElements();

private:

    Ui::MainWindow *m_ui;

    struct ElementInfo {
        QWidget *widget;
        QRect baseGeometry;  // координаты из UI файла (до первого масштабирования)
    };

    QList<ElementInfo> m_elements;
    bool m_positionsSaved = false;

};

#endif // UICONTROLLER_H
