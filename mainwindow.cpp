#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsmanager.h"
#include "modbustcpmanager.h"
#include "modbusvalidator.h"
#include <QScreen>
#include <QDateTime>
#include <QButtonGroup>

/**
 * @brief Конструктор главного окна приложения.
 * Инициализирует UI, создает вспомогательные контроллеры и таймеры,
 * настраивает модель данных для таблицы и эмулирует поведение вкладок
 * с помощью QButtonGroup. Также запускает первоначальное подключение к устройству.
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uiController(new UiController(ui))
    , m_dataModel(new DataModel(this))
    , m_modbusManager(nullptr)
    , m_pollingTimer(new QTimer(this))
    , m_logManager(new LogManager(this))
{
    ui->setupUi(this);

    // Сохраняем базовые координаты элементов мнемосхемы ИЗ UI файла
    // ДО первого масштабирования, чтобы использовать их как эталон (100%).
    m_uiController->saveBasePositions();

    // Инициализация и проверка Modbus-менеджера
    ensureCorrectManager();
    connectToRMT();

    // Таймер для обновления отображения текущих даты и времени на UI
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, [=]() {
        QString timeString = QDateTime::currentDateTime().toString("dd:MM:yyyy hh:mm:ss");
        ui->dateTimeLbl->setText(timeString);
    });
    m_timer->start();

    // Таймер периодического опроса (polling) Modbus-устройств
    m_pollingTimer->setInterval(2000); // Интервал опроса в миллисекундах (2 секунды)
    connect(m_pollingTimer, &QTimer::timeout, this, &MainWindow::onPollingTimeout);
    m_pollingTimer->start();

    // Привязка модели данных к таблице на UI
    ui->tableView->setModel(m_dataModel);

    // Эмуляция вкладок (Tabs) с помощью группы кнопок.
    // Это позволяет использовать стилизованные QPushButton вместо стандартного QTabBar.
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
    connect(ui->saveSettings, &QPushButton::clicked, this, [this]() {
        m_uiController->on_saveSettings_clicked();
    });
    connect(m_uiController, &UiController::settingsApplied,
            this, &MainWindow::onSettingsChanged);

    m_uiController->initUiElements();

    // Принудительный вызов для установки начального размера окна под первую вкладку
    onTabChanged(0);
}

/**
 * @brief Деструктор главного окна.
 * Освобождает ресурсы: удаляет сгенерированный UI, контроллер интерфейса,
 * останавливает таймеры и безопасно разрывает Modbus-соединение.
 */
MainWindow::~MainWindow()
{
    delete ui;
    // UiController не наследуется от QObject, поэтому удаляем его вручную
    if (m_uiController) {
        delete m_uiController;
    }
    if (m_timer) {
        m_timer->stop();
    }
    if (m_pollingTimer) {
        m_pollingTimer->stop();
    }
    // Безопасное удаление Modbus-менеджера.
    // Используем deleteLater(), чтобы избежать краша, если деструктор вызван
    // во время обработки сигнала от самого менеджера.
    if (m_modbusManager) {
        m_modbusManager->disconnectFromDevice();
        m_modbusManager->deleteLater();
    }
}

/**
 * @brief Переопределенный метод обработки события изменения размера окна.
 * Делегирует пересчет координат элементов мнемосхемы в UiController.
 */
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    m_uiController->repositionElements();
}

/**
 * @brief Переопределенный метод обработки события первого отображения окна.
 * Необходим для корректного масштабирования элементов, так как до вызова show()
 * виджеты могут не иметь своих окончательных геометрических размеров.
 */
void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // После первого показа размеры уже корректные
    m_uiController->repositionElements();
}

/**
 * @brief Проверяет тип текущего Modbus-менеджера и пересоздает его при необходимости.
 * В данном приложении жестко задано использование ModbusTcpManager.
 * Если в будущем потребуется поддержка RTU, этот метод можно расширить.
 */
void MainWindow::ensureCorrectManager() {
    bool needRecreate = false;

    if (!m_modbusManager) {
        needRecreate = true;
    } else if (dynamic_cast<ModbusTcpManager*>(m_modbusManager) == nullptr) {
        needRecreate = true;
    }

    if (needRecreate) {
        // Безопасное удаление старого менеджера, если он существовал
        if (m_modbusManager) {
            m_modbusManager->disconnectFromDevice();
            m_modbusManager->deleteLater();
        }

        m_modbusManager = new ModbusTcpManager(this);

        // Подключаем сигналы нового менеджера к слотам окна
        setupModbusConnections();
    }
}

/**
 * @brief Центральное место подписки на сигналы Modbus-менеджера.
 * Связывает события транспортного уровня (подключение, ошибки, получение данных)
 * с логированием и обновлением UI (модели таблицы).
 * Здесь же реализована бизнес-логика фильтрации входящих данных.
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
        m_logManager->addLog("Critical Error: " + error, true);
    });

    connect(m_modbusManager, &AbstractModbusManager::dataReceived, this, [this](const ModbusRequest &currentRequest, const QModbusDataUnit &unit) {

        // обновляем таблицу только для запросов начиная с адресов 0x0500 (значения перьев)
        quint16 startPensAddress = 0x0500;
        if (currentRequest.startAddress >= startPensAddress) {
            m_dataModel->updateData(currentRequest.serverAddress, unit);
        }
        m_logManager->addLog(tr("Data received from device #%1").arg(currentRequest.serverAddress));
    });

    connect(m_modbusManager, &AbstractModbusManager::writeCompleted, this, [this]() {
        m_logManager->addLog(tr("Data write completed successfully."));
    });
}

/**
 * @brief Инициирует подключение к RMT (Modbus TCP).
 * Проверяет текущее состояние, чтобы избежать повторных подключений,
 * и применяет параметры из файла настроек (таймауты, ретраи).
 */
void MainWindow::connectToRMT() {
    if (!m_modbusManager) return;

    // Предотвращаем спам-подключения, если процесс уже идет или соединение активно
    if (m_modbusManager->isConnected() || m_modbusManager->isConnecting()) {
        return;
    }

    ModbusConnectionSettings settings = SettingsManager::loadRmtSettings();
    settings.type = ModbusConnectionSettings::ConnectionType::Tcp;

    m_modbusManager->connectToDevice(settings);

    // Применяем пользовательские настройки соединения (таймауты и т.д.)
    Preferences prefs = SettingsManager::loadPreferences();
    m_modbusManager->updateConnectionParameters(prefs);
}

/**
 * @brief Формирует и отправляет тестовый Modbus-запрос на чтение.
 * Валидирует входные параметры, защищаясь от переполнения типов,
 * и передает запрос в очередь Modbus-менеджера.
 */
void MainWindow::sendReadData() {
    if (m_modbusManager->isProcessing()) {
        return;
    }

    ModbusConnectionSettings settings = SettingsManager::loadRmtSettings();
    quint8 deviceAddr = settings.deviceNumber;
    // Захардкоженные параметры для демонстрации/тестирования
    // В релизной версии должны считываться из UI или конфигурации

    QString funcStr = "03";
    QString startAddressStr = "0x44";
    QString countStr = "2";
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

    // Modbus допустимый диапазон адресов устройств: 1-247 (0 - широковещательный)
    if (deviceAddr == 0 || deviceAddr > 247) {
        m_logManager->addLog(tr("Skipping invalid device address: '%1' (допустимо 1-247)").arg(deviceAddr), true);
        return;
    }

    ValidationResult basicCheck = ModbusValidator::validateBasicParams(deviceAddr, funcCode, startAddress, count);
    if (!basicCheck.isValid) {
        m_logManager->addLog(tr("Validation error for address %1: %2").arg(deviceAddr).arg(basicCheck.errorMessage), true);
        return;
    }

    // Передача запроса в очередь менеджера
    m_modbusManager->sendReadRequest(deviceAddr, funcCode, startAddress, count);

}

/**
 * @brief Слот, срабатывающий по таймеру опроса (polling).
 * Отвечает за автоматическое восстановление связи при обрывах
 * и периодический запрос телеметрии с устройства.
 */
void MainWindow::onPollingTimeout() {
    connectToRMT();
    if (m_modbusManager && m_modbusManager->isConnected()) {
        qDebug() << "Pollling";
        sendReadData();
    }
}

/**
 * @brief Обработчик переключения вкладок (табов).
 * Управляет поведением окна: для главной вкладки (мнемосхемы) фиксирует размер
 * и масштабирует его под доступную область экрана, сохраняя пропорции 16:9.
 * Для остальных вкладок разрешает свободное изменение размеров окна.
 *
 * @param index Индекс выбранной вкладки (0 - 5).
 */
void MainWindow::onTabChanged(int index) {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QRect availableGeometry = screen->availableGeometry();

    QPoint currentPos = pos();  // Сохраняем текущую позицию окна на экране

    if (index == 0) {
        // Вкладка 0: Мнемосхема. Фиксированный размер FullHD (1920x1080).
        int targetWidth = 1920;
        int targetHeight = 1080;

        // Адаптация под физические размеры монитора (если экран меньше FullHD или есть панели задач)
        if (targetWidth > availableGeometry.width() ||
            targetHeight > availableGeometry.height()) {
            double scaleX = static_cast<double>(availableGeometry.width()) / targetWidth;
            double scaleY = static_cast<double>(availableGeometry.height()) / targetHeight;
            double scale = qMin(scaleX, scaleY);

            targetWidth = static_cast<int>(targetWidth * scale);
            targetHeight = static_cast<int>(targetHeight * scale);
        }

        setFixedSize(targetWidth, targetHeight);
        m_uiController->repositionElements();
    } else {
        // Остальные вкладки: Разрешаем пользователю свободно менять размер окна
        setMinimumSize(800, 600);  // Минимальный разумный размер
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }

    move(currentPos);  // Восстанавливаем позицию, чтобы окно не "прыгало" в угол
}

/**
 * @brief Изменение настроек на лету
 * @param settings
 */
void MainWindow::onSettingsChanged(const ModbusConnectionSettings &settings)
{
    m_logManager->addLog(tr("Applying new Modbus settings: %1:%2, Device: %3")
                             .arg(settings.ipAddress)
                             .arg(settings.port)
                             .arg(settings.deviceNumber));

    if (m_modbusManager) {
        // Если было активное соединение, разрываем его
        bool wasConnected = m_modbusManager->isConnected();
        if (wasConnected) {
            m_modbusManager->disconnectFromDevice();
        }

        // Инициируем новое подключение.
        connectToRMT();
    }
}