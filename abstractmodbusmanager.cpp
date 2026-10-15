#include "abstractmodbusmanager.h"
#include <QDebug>

/**
 * @brief Конструктор класса AbstractModbusManager.
 * Инициализирует базовый класс QObject и устанавливает начальные значения
 * для указателей и флагов состояния (устройство не создано, обработка не идет).
 */
AbstractModbusManager::AbstractModbusManager(QObject *parent)
    : QObject{parent}
    , m_modbusDevice(nullptr)
    , m_currentReply(nullptr)
    , m_isProcessing(false)
{
}

/**
 * @brief Деструктор класса.
 * Гарантирует корректное отключение от устройства и освобождение ресурсов
 * перед уничтожением объекта менеджера.
 */
AbstractModbusManager::~AbstractModbusManager() {
    disconnectFromDevice();
}

/**
 * @brief Безопасно разрывает соединение с Modbus-устройством.
 * Отключает сигналы, планирует удаление объекта устройства через deleteLater()
 * (чтобы избежать крашей, если удаление происходит внутри обработки сигнала),
 * и полностью сбрасывает внутреннее состояние менеджера (очереди и флаги).
 */
void AbstractModbusManager::disconnectFromDevice() {
    if (m_modbusDevice) {
        m_modbusDevice->disconnectDevice();
        disconnectDeviceSignals();
        // deleteLater гарантирует безопасное удаление в следующем цикле событий Qt
        m_modbusDevice->deleteLater();
        m_modbusDevice = nullptr;
    }
    resetInternalState();
}

/**
 * @brief Вспомогательный метод для подключения сигналов базового QModbusClient.
 * Связывает изменения состояния и ошибки транспортного уровня с внутренними слотами менеджера.
 */
void AbstractModbusManager::connectDeviceSignals() {
    if (!m_modbusDevice) return;
    connect(m_modbusDevice, &QModbusClient::stateChanged, this, &AbstractModbusManager::onStateChanged);
    connect(m_modbusDevice, &QModbusClient::errorOccurred, this, &AbstractModbusManager::onErrorOccurred);
}

/**
 * @brief Вспомогательный метод для отключения сигналов базового QModbusClient.
 * Вызывается перед удалением устройства, чтобы предотвратить вызов слотов уже уничтожаемого объекта.
 */
void AbstractModbusManager::disconnectDeviceSignals() {
    if (!m_modbusDevice) return;
    disconnect(m_modbusDevice, &QModbusClient::stateChanged, this, &AbstractModbusManager::onStateChanged);
    disconnect(m_modbusDevice, &QModbusClient::errorOccurred, this, &AbstractModbusManager::onErrorOccurred);
}

/**
 * @brief Сбрасывает внутреннее состояние менеджера.
 * Очищает очередь запросов, снимает флаг блокировки (m_isProcessing) и
 * безопасно уничтожает объект текущего ожидаемого ответа (m_currentReply),
 * предотвращая утечки памяти и зависания очереди.
 */
void AbstractModbusManager::resetInternalState() {
    m_requestQueue.clear();
    m_isProcessing = false;

    if (m_currentReply) {
        disconnect(m_currentReply, &QModbusReply::finished, this, &AbstractModbusManager::onReplyFinished);
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

/**
 * @brief Ставит запрос на чтение в очередь.
 * Проверяет наличие активного соединения, формирует структуру запроса,
 * добавляет его в конец очереди (FIFO) и запускает обработчик очереди.
 *
 * @param serverAddress Адрес Modbus-устройства (Unit ID).
 * @param funcCode Код функции Modbus (например, 0x03).
 * @param startAddress Начальный адрес регистра.
 * @param count Количество регистров для чтения.
 */
void AbstractModbusManager::sendReadRequest(quint8 serverAddress, quint8 funcCode, quint16 startAddress, quint16 count) {
    if (!isConnected()) {
        emit errorOccurred(tr("Connection is not established!"));
        return;
    }

    ModbusRequest req;
    req.type = ModbusRequest::Type::Read;
    req.serverAddress = serverAddress;
    req.funcCode = funcCode;
    req.startAddress = startAddress;
    req.count = count;

    // Чтение добавляется в конец очереди (стандартный порядок FIFO)
    m_requestQueue.enqueue(req);
    processNextRequest();
}

/**
 * @brief Ставит запрос на запись в очередь.
 * Проверяет соединение и наличие данных, формирует структуру запроса.
 * ВАЖНО: Запросы на запись добавляются в НАЧАЛО очереди (prepend),
 * обеспечивая им приоритет над запросами на чтение (LIFO для записи).
 * Это полезно для срочных команд управления.
 *
 * @param serverAddress Адрес Modbus-устройства (Unit ID).
 * @param funcCode Код функции Modbus (например, 0x06 или 0x10).
 * @param startAddress Начальный адрес регистра.
 * @param values Вектор значений для записи.
 */
void AbstractModbusManager::sendWriteRequest(quint8 serverAddress, quint8 funcCode, quint16 startAddress, const QVector<quint16> &values) {
    if (!isConnected()) {
        emit errorOccurred(tr("Connection is not established!"));
        return;
    }
    if (values.isEmpty()) {
        emit errorOccurred(tr("No data to write!"));
        return;
    }

    ModbusRequest req;
    req.type = ModbusRequest::Type::Write;
    req.serverAddress = serverAddress;
    req.funcCode = funcCode;
    req.startAddress = startAddress;
    req.count = static_cast<quint16>(values.size());
    req.values = values;

    // Запись добавляется в начало очереди (приоритетное выполнение)
    m_requestQueue.prepend(req);
    processNextRequest();
}

/**
 * @brief Ядро обработчика очереди запросов.
 * Извлекает следующий запрос из очереди, маппит Modbus-код функции на
 * внутренний тип регистров Qt (QModbusDataUnit::RegisterType) и отправляет
 * запрос через базовый QModbusClient. Обеспечивает последовательное выполнение
 * запросов (только один активный запрос в один момент времени).
 */
void AbstractModbusManager::processNextRequest() {
    // Защита от рекурсии и отправки запросов при отсутствии соединения
    if (m_isProcessing || m_requestQueue.isEmpty() || !isConnected() || !m_modbusDevice) {
        return;
    }

    m_isProcessing = true; // Блокируем очередь до получения ответа
    m_currentRequest = m_requestQueue.dequeue();

    // Очистка предыдущего объекта ответа, если он вдруг остался
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    // Обработка запросов на ЧТЕНИЕ
    if (m_currentRequest.type == ModbusRequest::Type::Read) {
        QModbusDataUnit::RegisterType type;
        // Маппинг стандартных кодов функций Modbus на типы регистров Qt
        switch (m_currentRequest.funcCode) {
        case 0x01: type = QModbusDataUnit::Coils; break;
        case 0x02: type = QModbusDataUnit::DiscreteInputs; break;
        case 0x03: type = QModbusDataUnit::HoldingRegisters; break;
        case 0x04: type = QModbusDataUnit::InputRegisters; break;
        default:
            emit errorOccurred(tr("Unsupported function code for reading"));
            m_isProcessing = false;
            processNextRequest(); // Переходим к следующему запросу
            return;
        }
        QModbusDataUnit request(type, m_currentRequest.startAddress, m_currentRequest.count);
        m_currentReply = m_modbusDevice->sendReadRequest(request, m_currentRequest.serverAddress);

      // Обработка запросов на ЗАПИСЬ
    } else if (m_currentRequest.type == ModbusRequest::Type::Write) {
        QModbusDataUnit::RegisterType type;
        // Маппинг кодов функций для записи (одиночные и множественные регистры/коилы)
        if (m_currentRequest.funcCode == 0x05 || m_currentRequest.funcCode == 0x0F) {
            type = QModbusDataUnit::Coils;
        } else if (m_currentRequest.funcCode == 0x06 || m_currentRequest.funcCode == 0x10) {
            type = QModbusDataUnit::HoldingRegisters;
        } else {
            emit errorOccurred(tr("Unsupported function code for writing"));
            m_isProcessing = false;
            processNextRequest();
            return;
        }
        QModbusDataUnit request(type, m_currentRequest.startAddress, m_currentRequest.count);
        // Заполнение Data Unit значениями из вектора
        for (int i = 0; i < m_currentRequest.count; ++i) {
            request.setValue(i, m_currentRequest.values[i]);
        }
        m_currentReply = m_modbusDevice->sendWriteRequest(request, m_currentRequest.serverAddress);
    }

    // Подключение к сигналу завершения запроса
    if (m_currentReply) {
        if (!m_currentReply->isFinished()) {
            connect(m_currentReply, &QModbusReply::finished, this, &AbstractModbusManager::onReplyFinished);
        } else {
            // Если ответ уже получен (синхронный сбой или кэш), обрабатываем сразу
            onReplyFinished();
        }
    } else {
        // Критическая ошибка: Qt не смог создать объект запроса
        emit errorCriticalOccurred(tr("Failed to send request: ") + m_modbusDevice->errorString());
        m_isProcessing = false;
        processNextRequest();
    }
}

/**
 * @brief Слот, реагирующий на изменение состояния соединения.
 * Транслирует внутренние состояния QModbusDevice в пользовательские сигналы менеджера.
 */
void AbstractModbusManager::onStateChanged(QModbusDevice::State state) {
    if (state == QModbusDevice::UnconnectedState) {
        qDebug() << "ModbusManager: Device disconnected.";
        emit disconnected();
    } else if (state == QModbusDevice::ConnectedState) {
        qDebug() << "ModbusManager: Device successfully connected.";
        emit connected();
    }
}

/**
 * @brief Слот для обработки ошибок транспортного уровня (сети/COM-порта).
 * Игнорирует NoError, при возникновении проблем эмитит сигнал с текстом ошибки.
 */
void AbstractModbusManager::onErrorOccurred(QModbusDevice::Error error) {
    if (error != QModbusDevice::NoError && m_modbusDevice) {
        emit errorOccurred(m_modbusDevice->errorString());
    }
}

/**
 * @brief Слот, вызываемый при получении ответа от Modbus-устройства (успех или ошибка).
 * Анализирует результат, эмитит соответствующие сигналы (dataReceived, writeCompleted или errorOccurred),
 * очищает объект ответа, снимает блокировку очереди и запускает обработку следующего запроса.
 */
void AbstractModbusManager::onReplyFinished() {
    if (!m_currentReply || !m_modbusDevice) return;

    // Успешное выполнение запроса
    if (m_currentReply->error() == QModbusDevice::NoError) {
        if (m_currentRequest.type == ModbusRequest::Type::Read) {
            emit dataReceived(m_currentRequest, m_currentReply->result());

        } else {
            emit writeCompleted();
        }
    // Ошибка протокола Modbus (устройство ответило, но с кодом исключения)
    } else if (m_currentReply->error() == QModbusDevice::ProtocolError) {
        QString exceptionInfo = tr("Unknown error");
        quint8 excCode = m_currentReply->rawResult().exceptionCode();
        // Декодирование стандартных кодов исключений Modbus
        if (excCode == 0x01) exceptionInfo = tr("Illegal Function");
        else if (excCode == 0x02) exceptionInfo = tr("Illegal Data Address");
        else if (excCode == 0x03) exceptionInfo = tr("Illegal Data Value");
        else if (excCode == 0x04) exceptionInfo = tr("Slave Device Failure");
        else if (excCode == 0x05) exceptionInfo = tr("Acknowledge");
        else if (excCode == 0x06) exceptionInfo = tr("Slave Device Busy");
        else if (excCode == 0x07) exceptionInfo = tr("Negative Acknowledge");

        emit errorOccurred(tr("Modbus Exception. Code: %1, Desc: %2, Unit ID: %3, Addr: %4, Func: %5")
                               .arg(excCode).arg(exceptionInfo)
                               .arg(m_currentRequest.serverAddress)
                               .arg(m_currentRequest.startAddress)
                               .arg(m_currentRequest.funcCode)
                           );
    // Таймаут ожидания ответа
    } else if (m_currentReply->error() == QModbusDevice::TimeoutError) {
        emit errorOccurred(tr("Timeout waiting for device %1 response.").arg(m_currentRequest.serverAddress));
    // Прочие ошибки (например, обрыв связи во время передачи)
    } else {
        emit errorOccurred(tr("Modbus response error: %1, Unit ID: %2, Addr: %3")
                               .arg(m_currentReply->errorString()).arg(m_currentRequest.serverAddress).arg(m_currentRequest.startAddress));
    }

    // Очистка ресурсов и переход к следующему запросу в очереди
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    m_isProcessing = false; // Снимаем блокировку очереди
    processNextRequest(); // Запускаем следующий запрос
}

