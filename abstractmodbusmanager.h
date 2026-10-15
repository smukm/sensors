#ifndef ABSTRACTMODBUSMANAGER_H
#define ABSTRACTMODBUSMANAGER_H

//#include "settingsmanager.h"
#include <QObject>
#include <QModbusClient>
#include <QModbusDevice>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QQueue>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QDateTime>
#include <QSerialPort>

struct ModbusConnectionSettings {
    enum ConnectionType { Serial, Tcp };
    ConnectionType type = Serial;

    // Параметры для Serial (RTU)
    QString portName;
    int baudRate = 9600;
    int parity = QSerialPort::NoParity;
    int dataBits = QSerialPort::Data8;
    int stopBits = QSerialPort::OneStop;

    // Параметры для TCP
    QString ipAddress = "127.0.0.1";
    qint16 port = 502;
};

struct ModbusRequest {
    enum class Type { Read, Write };
    Type type;
    quint8 serverAddress;
    quint8 funcCode;
    quint16 startAddress;
    quint16 count;
    QVector<quint16> values;
};

struct Preferences;

class AbstractModbusManager : public QObject
{
    Q_OBJECT
public:
    explicit AbstractModbusManager(QObject *parent = nullptr);
    virtual ~AbstractModbusManager();

    // Чисто виртуальные методы, которые должны реализовать наследники
    virtual void connectToDevice(const ModbusConnectionSettings &settings) = 0;
    virtual bool isConnected() const = 0;
    virtual bool isConnecting() const = 0;

    /**
     * @brief Применяет новые параметры таймаута и повторных попыток к активному соединению.
     * Должен быть переопределен в наследниках для учета специфики RTU/TCP.
     */
    virtual void updateConnectionParameters(const Preferences &preferences) = 0;

    void disconnectFromDevice();
    bool isProcessing() const { return m_isProcessing; }

    void sendReadRequest(quint8 serverAddress, quint8 funcCode, quint16 startAddress, quint16 count);
    void sendWriteRequest(quint8 serverAddress, quint8 funcCode, quint16 startAddress, const QVector<quint16> &values);


signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &errorString);
    void errorCriticalOccurred(const QString &errorString);
    void dataReceived(quint8 serverAddress, const QModbusDataUnit &data);
    void writeCompleted();

protected slots:
    void onStateChanged(QModbusDevice::State state);
    void onErrorOccurred(QModbusDevice::Error error);
    void onReplyFinished();

protected:
    void connectDeviceSignals();
    void disconnectDeviceSignals();

    // Вспомогательный метод для очистки внутренних очередей и флагов
    void resetInternalState();

    void processNextRequest();
    void recordTimeout(quint8 serverAddress);

    QModbusClient *m_modbusDevice; // Указатель на базовый класс клиента
    QModbusReply *m_currentReply;

    QQueue<ModbusRequest> m_requestQueue;
    ModbusRequest m_currentRequest;
    bool m_isProcessing;

    QHash<quint8, QQueue<QDateTime>> m_timeoutHistory;

    //static constexpr int MAX_TIMEOUTS = 3;
    static constexpr int TIMEOUT_WINDOW_MS = 30000;
};

#endif // ABSTRACTMODBUSMANAGER_H
