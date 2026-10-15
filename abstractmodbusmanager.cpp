#include "abstractmodbusmanager.h"
#include <QDebug>

AbstractModbusManager::AbstractModbusManager(QObject *parent)
    : QObject{parent}
    , m_modbusDevice(nullptr)
    , m_currentReply(nullptr)
    , m_isProcessing(false)
{
}

AbstractModbusManager::~AbstractModbusManager() {
    disconnectFromDevice();
}

void AbstractModbusManager::disconnectFromDevice() {
    if (m_modbusDevice) {
        m_modbusDevice->disconnectDevice();
        disconnectDeviceSignals();

        m_modbusDevice->deleteLater();
        m_modbusDevice = nullptr;
    }
    resetInternalState();
}

void AbstractModbusManager::connectDeviceSignals() {
    if (!m_modbusDevice) return;
    connect(m_modbusDevice, &QModbusClient::stateChanged, this, &AbstractModbusManager::onStateChanged);
    connect(m_modbusDevice, &QModbusClient::errorOccurred, this, &AbstractModbusManager::onErrorOccurred);
}

void AbstractModbusManager::disconnectDeviceSignals() {
    if (!m_modbusDevice) return;
    disconnect(m_modbusDevice, &QModbusClient::stateChanged, this, &AbstractModbusManager::onStateChanged);
    disconnect(m_modbusDevice, &QModbusClient::errorOccurred, this, &AbstractModbusManager::onErrorOccurred);
}

void AbstractModbusManager::resetInternalState() {
    m_requestQueue.clear();
    m_isProcessing = false;
    m_timeoutHistory.clear();

    if (m_currentReply) {
        disconnect(m_currentReply, &QModbusReply::finished, this, &AbstractModbusManager::onReplyFinished);
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

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

    m_requestQueue.enqueue(req);
    processNextRequest();
}

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

    m_requestQueue.prepend(req);
    processNextRequest();
}

void AbstractModbusManager::processNextRequest() {
    if (m_isProcessing || m_requestQueue.isEmpty() || !isConnected() || !m_modbusDevice) {
        return;
    }

    m_isProcessing = true;
    m_currentRequest = m_requestQueue.dequeue();

    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (m_currentRequest.type == ModbusRequest::Type::Read) {
        QModbusDataUnit::RegisterType type;
        switch (m_currentRequest.funcCode) {
        case 0x01: type = QModbusDataUnit::Coils; break;
        case 0x02: type = QModbusDataUnit::DiscreteInputs; break;
        case 0x03: type = QModbusDataUnit::HoldingRegisters; break;
        case 0x04: type = QModbusDataUnit::InputRegisters; break;
        default:
            emit errorOccurred(tr("Unsupported function code for reading"));
            m_isProcessing = false;
            processNextRequest();
            return;
        }
        QModbusDataUnit request(type, m_currentRequest.startAddress, m_currentRequest.count);
        m_currentReply = m_modbusDevice->sendReadRequest(request, m_currentRequest.serverAddress);

    } else if (m_currentRequest.type == ModbusRequest::Type::Write) {
        QModbusDataUnit::RegisterType type;
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
        for (int i = 0; i < m_currentRequest.count; ++i) {
            request.setValue(i, m_currentRequest.values[i]);
        }
        m_currentReply = m_modbusDevice->sendWriteRequest(request, m_currentRequest.serverAddress);
    }

    if (m_currentReply) {
        if (!m_currentReply->isFinished()) {
            connect(m_currentReply, &QModbusReply::finished, this, &AbstractModbusManager::onReplyFinished);
        } else {
            onReplyFinished();
        }
    } else {
        emit errorCriticalOccurred(tr("Failed to send request: ") + m_modbusDevice->errorString());
        m_isProcessing = false;
        processNextRequest();
    }
}

void AbstractModbusManager::onStateChanged(QModbusDevice::State state) {
    if (state == QModbusDevice::UnconnectedState) {
        qDebug() << "ModbusManager: Device disconnected.";
        emit disconnected();
    } else if (state == QModbusDevice::ConnectedState) {
        qDebug() << "ModbusManager: Device successfully connected.";
        emit connected();
    }
}

void AbstractModbusManager::onErrorOccurred(QModbusDevice::Error error) {
    if (error != QModbusDevice::NoError && m_modbusDevice) {
        emit errorOccurred(m_modbusDevice->errorString());
    }
}

void AbstractModbusManager::onReplyFinished() {
    if (!m_currentReply || !m_modbusDevice) return;

    if (m_currentReply->error() == QModbusDevice::NoError) {
        if (m_currentRequest.type == ModbusRequest::Type::Read) {

            // эмиттим dataReceived только начиная с адресов 0x0500 (значения перьев)
            quint16 startPensAddress = 0x0500;
            if (m_currentRequest.startAddress >= startPensAddress) {
                emit dataReceived(m_currentRequest.serverAddress, m_currentReply->result());
            }
        } else {
            emit writeCompleted();
        }
    } else if (m_currentReply->error() == QModbusDevice::ProtocolError) {
        QString exceptionInfo = tr("Unknown error");
        quint8 excCode = m_currentReply->rawResult().exceptionCode();
        if (excCode == 0x01) exceptionInfo = tr("Illegal Function");
        else if (excCode == 0x02) exceptionInfo = tr("Illegal Data Address");
        else if (excCode == 0x03) exceptionInfo = tr("Illegal Data Value");
        else if (excCode == 0x04) exceptionInfo = tr("Slave Device Failure");
        else if (excCode == 0x05) exceptionInfo = tr("Slave Device Busy");
        else if (excCode == 0x06) exceptionInfo = tr("Slave Device Busy");
        else if (excCode == 0x07) exceptionInfo = tr("Access Denied");

        emit errorOccurred(tr("Modbus Exception. Code: %1, Desc: %2, Unit ID: %3, Addr: %4, Func: %5")
                               .arg(excCode).arg(exceptionInfo)
                               .arg(m_currentRequest.serverAddress)
                               .arg(m_currentRequest.startAddress)
                               .arg(m_currentRequest.funcCode)
                           );
    } else if (m_currentReply->error() == QModbusDevice::TimeoutError) {
        emit errorOccurred(tr("Timeout waiting for device %1 response.").arg(m_currentRequest.serverAddress));
    } else {
        emit errorOccurred(tr("Modbus response error: %1, Unit ID: %2, Addr: %3")
                               .arg(m_currentReply->errorString()).arg(m_currentRequest.serverAddress).arg(m_currentRequest.startAddress));
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    m_isProcessing = false;
    processNextRequest();
}

