#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsmanager.h"
#include "modbustcpmanager.h"
#include "modbusvalidator.h"
#include <QScreen>
#include <QDateTime>
#include <QButtonGroup>


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
    , m_dataModel(new DataModel(this))
    , m_modbusManager(nullptr)
    , m_pollingTimer(new QTimer())
    , m_logManager(new LogManager(this))
{
    ui->setupUi(this);

    // Сохраняем базовые координаты ИЗ UI файла ДО первого масштабирования
    saveBasePositions();

    ensureCorrectManager();
    connectToRMT();

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, [=]() {
        QString timeString = QDateTime::currentDateTime().toString("dd:MM:yyyy hh:mm:ss");
        ui->dateTimeLbl->setText(timeString);
    });
    m_timer->start();

    m_pollingTimer->setInterval(2000); // Интервал опроса в миллисекундах (2 секунды)
    connect(m_pollingTimer, &QTimer::timeout, this, &MainWindow::onPollingTimeout);
    m_pollingTimer->start();

    ui->tableView->setModel(m_dataModel);

    QButtonGroup *tabGroup = new QButtonGroup(this);
    tabGroup->addButton(ui->btnTab1, 0);
    tabGroup->addButton(ui->btnTab2, 1);
    tabGroup->addButton(ui->btnTab3, 2);
    tabGroup->addButton(ui->btnTab4, 3);
    tabGroup->addButton(ui->btnTab5, 4);
    tabGroup->addButton(ui->btnTab6, 5);
    tabGroup->setExclusive(true); // Автоматически снимает выделение с других
    connect(tabGroup, &QButtonGroup::idClicked, this, [this](int id) {
        ui->stackedWidget->setCurrentIndex(id);
        onTabChanged(id);
    });


    connect(m_modbusManager, &AbstractModbusManager::errorOccurred, this, [this](const QString &error) {
        m_logManager->addLog(error, true);
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
    if (m_pollingTimer) {
        m_pollingTimer->stop();
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
        ui->tableView,
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

/**
 * @brief Проверяет и при необходимости пересоздает менеджер Modbus.
 * @param type Требуемый тип соединения.
 */
void MainWindow::ensureCorrectManager() {
    bool needRecreate = false;

    if (!m_modbusManager) {
        needRecreate = true;
    } else if (dynamic_cast<ModbusTcpManager*>(m_modbusManager) == nullptr) {
        needRecreate = true;
    }

    if (needRecreate) {
        // Безопасное удаление старого менеджера
        if (m_modbusManager) {
            m_modbusManager->disconnectFromDevice();
            m_modbusManager->deleteLater();
        }

        m_modbusManager = new ModbusTcpManager(this);

        // Подключаем сигналы
        setupModbusConnections();
    }
}

/**
 * @brief Подключает сигналы менеджера Modbus к лямбда-выражениям главного окна.
 */
void MainWindow::setupModbusConnections() {
    if (!m_modbusManager) return;

    connect(m_modbusManager, &AbstractModbusManager::connected, this, [this]() {
        m_logManager->addLog(tr("Modbus: Connected successfully."));
        m_uiController->setConnectedState();
    });

    connect(m_modbusManager, &AbstractModbusManager::disconnected, this, [this]() {
        m_logManager->addLog(tr("Modbus: Disconnected."));
        m_uiController->setDisconnectedState();
    });

    connect(m_modbusManager, &AbstractModbusManager::errorOccurred, this, [this](const QString &error) {
        m_logManager->addLog(error, true);
    });

    connect(m_modbusManager, &AbstractModbusManager::errorCriticalOccurred, this, [this](const QString &error) {
        m_logManager->addLog("Critical Error" + error, true);
    });

    connect(m_modbusManager, &AbstractModbusManager::dataReceived, this, [this](quint8 serverAddress, const QModbusDataUnit &unit) {
        m_dataModel->updateData(serverAddress, unit);
        m_logManager->addLog(tr("Data received from device #%1").arg(serverAddress));
    });

    connect(m_modbusManager, &AbstractModbusManager::writeCompleted, this, [this]() {
        m_logManager->addLog(tr("Data write completed successfully."));
    });
}


void MainWindow::connectToRMT() {
    if (!m_modbusManager) return;

    if (m_modbusManager->isConnected() || m_modbusManager->isConnecting()) {
        return;
    }

    ModbusConnectionSettings settings;
    settings.type = ModbusConnectionSettings::ConnectionType::Tcp;
    settings.ipAddress = "127.0.0.1";
    settings.port = 502;
    m_modbusManager->connectToDevice(settings);
    Preferences prefs = SettingsManager::loadPreferences();
    m_modbusManager->updateConnectionParameters(prefs);
}

void MainWindow::onPollingTimeout() {
    connectToRMT();
    if (m_modbusManager && m_modbusManager->isConnected()) {
        qDebug() << "Pollling";
        sendReadData();
    }
}

/**
 * @brief Формирует и отправляет Modbus-запрос на чтение на основе данных из полей ввода.
 * Поддерживает множественные адреса устройств через запятую.
 */
void MainWindow::sendReadData() {
    if (m_modbusManager->isProcessing()) {
        return;
    }

    QString addrStr = "1";
    QString funcStr = "03";
    QString startAddressStr = "1";
    QString countStr = "4";
    // Парсим общие параметры (они одинаковы для всех устройств в этом запросе)
    bool okAddr, okQty;
    quint8 funcCode = static_cast<quint8>(funcStr.toInt());
    quint16 startAddress = startAddressStr.toInt(&okAddr, 0);
    quint16 count = countStr.toInt(&okQty, 0);

    if (!okAddr || startAddress > 65535) {
        m_logManager->addLog(tr("Invalid register address (0-65535)!"), true);
        return;
    }
    if (!okQty || count < 1 || count > 65535) {
        m_logManager->addLog(tr("Invalid number of registers (1-65535)!"), true);
        return;
    }


    bool okDev;
    // Modbus допустимый диапазон адресов устройств: 1-247 (0 - широковещательный)
    quint8 deviceAddr = addrStr.trimmed().toUInt(&okDev);

    if (!okDev || deviceAddr == 0 || deviceAddr > 247) {
        m_logManager->addLog(tr("Skipping invalid device address: '%1' (допустимо 1-247)").arg(addrStr), true);
        //continue;
    }
    qDebug() << funcCode;
    ValidationResult basicCheck = ModbusValidator::validateBasicParams(deviceAddr, funcCode, startAddress, count);
    if (!basicCheck.isValid) {
        m_logManager->addLog(tr("Validation error for address %1: %2").arg(deviceAddr).arg(basicCheck.errorMessage), true);
        //continue;
    }

    // Ставим запрос в очередь
    m_modbusManager->sendReadRequest(deviceAddr, funcCode, startAddress, count);

}
