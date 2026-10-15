#include "modbustcpmanager.h"
#include "settingsmanager.h"

/**
 * @brief Конструктор ModbusTcpManager.
 * Делегирует инициализацию базовому классу AbstractModbusManager.
 */
ModbusTcpManager::ModbusTcpManager(QObject *parent)
    : AbstractModbusManager(parent)
{
}

/**
 * @brief Инициирует подключение к Modbus TCP устройству.
 *
 * Алгоритм работы:
 * 1. Если соединение уже существует, оно принудительно разрывается.
 * 2. Создается новый экземпляр QModbusTcpClient.
 * 3. Задаются сетевые параметры (IP-адрес и порт).
 * 4. Применяются параметры таймаутов и повторных попыток из файла настроек.
 * 5. Выполняется асинхронное подключение (результат придет в слот onStateChanged).
 *
 * @param settings Структура с параметрами подключения (IP, порт и т.д.).
 */
void ModbusTcpManager::connectToDevice(const ModbusConnectionSettings &settings) {
    // Гарантируем чистое состояние: если объект устройства уже существует, удаляем его
    if (m_modbusDevice) {
        disconnectFromDevice();
    }

    // Создаем TCP-клиент. Указываем 'this' как родителя для автоматического управления памятью.
    m_modbusDevice = new QModbusTcpClient(this);

    // Настраиваем сетевые параметры
    m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, settings.ipAddress);
    m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, static_cast<int>(settings.port));

    // Подключаем внутренние сигналы базового класса (stateChanged, errorOccurred)
    connectDeviceSignals();

    // Загружаем пользовательские настройки таймаутов и ретраев
    Preferences preferences = SettingsManager::loadPreferences();
    m_modbusDevice->setTimeout(preferences.modbusTcpTimeout);
    m_modbusDevice->setNumberOfRetries(preferences.modbusTcpRetries);

    // ВАЖНОЕ ЗАМЕЧАНИЕ ПО АРХИТЕКТУРЕ TCP:
    // Для TCP-соединений количество Modbus-ретраев (setNumberOfRetries) лучше ставить 0 или 1.
    // TCP-стек ОС сам гарантирует доставку пакетов и выполняет ретраи на сетевом уровне.
    // Лишние Modbus-ретраи при физическом обрыве сети приведут к тому, что очередь запросов
    // "встанет" на долгое время (timeout * retries), блокируя опрос других устройств.

    // Инициируем процесс подключения (асинхронная операция)
    if (!m_modbusDevice->connectDevice()) {
        emit errorCriticalOccurred(tr("Failed to initiate TCP connection: ") + m_modbusDevice->errorString());
    }
}

/**
 * @brief Проверяет, установлено ли активное соединение с устройством.
 * @return true, если объект устройства существует и его состояние равно ConnectedState.
 */
bool ModbusTcpManager::isConnected() const {
    return m_modbusDevice && m_modbusDevice->state() == QModbusDevice::ConnectedState;
}

/**
 * @brief Проверяет, находится ли устройство в процессе установления соединения.
 * @return true, если объект устройства существует и его состояние равно ConnectingState.
 */
bool ModbusTcpManager::isConnecting() const {
    return m_modbusDevice && m_modbusDevice->state() == QModbusDevice::ConnectingState;
}

/**
 * @brief Динамически обновляет параметры соединения (таймауты и ретраи) без разрыва связи.
 * Позволяет применять изменения настроек "на лету", не требуя переподключения к устройству.
 *
 * @param preferences Структура с новыми параметрами из SettingsManager.
 */
void ModbusTcpManager::updateConnectionParameters(const Preferences &preferences) {
    if (m_modbusDevice) {
        m_modbusDevice->setTimeout(preferences.modbusTcpTimeout);
        m_modbusDevice->setNumberOfRetries(preferences.modbusTcpRetries);
    }
}