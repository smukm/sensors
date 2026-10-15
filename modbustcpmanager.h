#ifndef MODBUSTCPMANAGER_H
#define MODBUSTCPMANAGER_H

#include "abstractmodbusmanager.h"
#include <QModbusTcpClient>

class ModbusTcpManager : public AbstractModbusManager
{
    Q_OBJECT
public:
    explicit ModbusTcpManager(QObject *parent = nullptr);

    void connectToDevice(const ModbusConnectionSettings &settings) override;
    bool isConnected() const override;
    bool isConnecting() const override;

    void updateConnectionParameters(const Preferences &preferences) override;
};

#endif // MODBUSTCPMANAGER_H
