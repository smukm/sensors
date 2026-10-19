#ifndef UICONTROLLER_H
#define UICONTROLLER_H

#include "./ui_mainwindow.h"
#include "abstractmodbusmanager.h"

class UiController : public QObject
{
    Q_OBJECT
public:
    explicit UiController(Ui::MainWindow *ui, QObject *parent = nullptr);

    void initUiElements();
    void setConnectedState();
    void setDisconnectedState();
    void saveBasePositions();
    void repositionElements();

public slots:
    void on_saveSettings_clicked();
private:

    Ui::MainWindow *m_ui;

    struct ElementInfo {
        QWidget *widget;
        QRect baseGeometry;  // координаты из UI файла (до первого масштабирования)
    };

    QList<ElementInfo> m_elements;
    bool m_positionsSaved = false;
signals:
    // Сигнал после успешного сохранения настроек
    void settingsApplied(const ModbusConnectionSettings &settings);
};

#endif // UICONTROLLER_H
