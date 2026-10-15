#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "uicontroller.h"
#include "datamodel.h"
#include "abstractmodbusmanager.h"
#include "logmanager.h"
#include <QMainWindow>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void connectToRMT();
    void ensureCorrectManager();
    void setupModbusConnections();
    void sendReadData();

    Ui::MainWindow *ui;
    UiController* m_uiController; // Вспомогательный контроллер для управления состоянием элементов UI.

    QTimer* m_timer;
    DataModel* m_dataModel;
    AbstractModbusManager* m_modbusManager;
    QTimer *m_pollingTimer; // Таймер для автоматического периодического опроса устройств.
    LogManager *m_logManager;

private slots:
    void onTabChanged(int index);
    void onPollingTimeout();
};
#endif // MAINWINDOW_H
