#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>
#include <QDateTime>


// Структура для хранения относительных координат элемента
struct ElementPos {
    QWidget *widget;
    double x;      // доля от ширины (0.0 - 1.0)
    double y;      // доля от высоты (0.0 - 1.0)
    double w;      // доля от ширины
    double h;      // доля от высоты
};

static const double BASE_W = 1920.0;
static const double BASE_H = 1080.0;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uiController(new UiController(ui))
{
    ui->setupUi(this);

    // Сохраняем базовые координаты ИЗ UI файла ДО первого масштабирования
    saveBasePositions();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [=]() {
        QString timeString = QDateTime::currentDateTime().toString("dd:MM:yyyy hh:mm:ss");
        ui->dateTimeLbl->setText(timeString);
    });
    m_timer->start();

    // Подключаем кнопки табов
    connect(ui->btnTab1, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(0);
        ui->btnTab1->setChecked(true);
        ui->btnTab2->setChecked(false);
        ui->btnTab3->setChecked(false);
        ui->btnTab4->setChecked(false);
        ui->btnTab5->setChecked(false);
        ui->btnTab6->setChecked(false);
        onTabChanged(0);
    });
    connect(ui->btnTab2, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(1);
        ui->btnTab1->setChecked(false);
        ui->btnTab2->setChecked(true);
        ui->btnTab3->setChecked(false);
        ui->btnTab4->setChecked(false);
        ui->btnTab5->setChecked(false);
        ui->btnTab6->setChecked(false);
        onTabChanged(1);
    });
    connect(ui->btnTab3, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(2);
        ui->btnTab1->setChecked(false);
        ui->btnTab2->setChecked(false);
        ui->btnTab3->setChecked(true);
        ui->btnTab4->setChecked(false);
        ui->btnTab5->setChecked(false);
        ui->btnTab6->setChecked(false);
        onTabChanged(2);
    });
    connect(ui->btnTab4, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(3);
        ui->btnTab1->setChecked(false);
        ui->btnTab2->setChecked(false);
        ui->btnTab3->setChecked(false);
        ui->btnTab4->setChecked(true);
        ui->btnTab5->setChecked(false);
        ui->btnTab6->setChecked(false);
        onTabChanged(3);
    });
    connect(ui->btnTab5, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(4);
        ui->btnTab1->setChecked(false);
        ui->btnTab2->setChecked(false);
        ui->btnTab3->setChecked(false);
        ui->btnTab4->setChecked(false);
        ui->btnTab5->setChecked(true);
        ui->btnTab6->setChecked(false);
        onTabChanged(4);
    });
    connect(ui->btnTab6, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(5);
        ui->btnTab1->setChecked(false);
        ui->btnTab2->setChecked(false);
        ui->btnTab3->setChecked(false);
        ui->btnTab4->setChecked(false);
        ui->btnTab5->setChecked(false);
        ui->btnTab6->setChecked(true);
        onTabChanged(4);
    });


    ui->c19le->setText("20.1 %");
    ui->c27le->setText("12.3 %");

    onTabChanged(0);
}

MainWindow::~MainWindow()
{
    delete ui;
    if (m_timer) {
        m_timer->stop();
    }
}

void MainWindow::saveBasePositions()
{
    if (m_positionsSaved)
        return;

    // Список всех виджетов, которые нужно позиционировать
    // Добавляем сюда новые элементы — координаты берутся автоматически из UI
    QList<QWidget*> widgets = {
        ui->c27lbl,
        ui->c27le,
        ui->c19lbl,
        ui->c19le,
    };

    for (QWidget *w : widgets) {
        m_elements.append({ w, w->geometry() });
    }

    m_positionsSaved = true;
}

void MainWindow::repositionElements()
{
    if (ui->stackedWidget->currentIndex() != 0)
        return;

    const int pw = ui->stand_1->width();
    const int ph = ui->stand_1->height();

    if (pw <= 0 || ph <= 0)
        return;

    const double sx = static_cast<double>(pw) / BASE_W;
    const double sy = static_cast<double>(ph) / BASE_H;

    for (const auto &e : m_elements) {
        e.widget->setGeometry(
            qRound(e.baseGeometry.x() * sx),
            qRound(e.baseGeometry.y() * sy),
            qRound(e.baseGeometry.width() * sx),
            qRound(e.baseGeometry.height() * sy)
            );
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    repositionElements();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // После первого показа размеры уже корректные
    repositionElements();
}

void MainWindow::onTabChanged(int index) {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QRect availableGeometry = screen->availableGeometry();

    QPoint currentPos = pos();  // Сохраняем позицию

    if (index == 0) {
        // Первая вкладка - фиксированный размер FullHD (адаптированный)
        int targetWidth = 1920;
        int targetHeight = 1080;

        // Адаптируем под экран
        if (targetWidth > availableGeometry.width() ||
            targetHeight > availableGeometry.height()) {
            double scaleX = static_cast<double>(availableGeometry.width()) / targetWidth;
            double scaleY = static_cast<double>(availableGeometry.height()) / targetHeight;
            double scale = qMin(scaleX, scaleY);

            targetWidth = static_cast<int>(targetWidth * scale);
            targetHeight = static_cast<int>(targetHeight * scale);
        }

        setFixedSize(targetWidth, targetHeight);
    } else {
        // Другие вкладки - разрешаем ресайз
        setMinimumSize(800, 600);  // Минимальный разумный размер
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        // resize(1280, 720);
    }

    move(currentPos);  // Восстанавливаем позицию
}
