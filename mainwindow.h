#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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
    void saveBasePositions();
    void repositionElements();

    Ui::MainWindow *ui;
    struct ElementInfo {
        QWidget *widget;
        QRect baseGeometry;  // координаты из UI файла (до первого масштабирования)
    };

    QList<ElementInfo> m_elements;
    bool m_positionsSaved = false;

private slots:
    void onTabChanged(int index);
};
#endif // MAINWINDOW_H
