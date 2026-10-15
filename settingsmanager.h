#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H
#include "abstractmodbusmanager.h"

struct ModbusLastParams {
    // Параметры чтения
    QString readDeviceAddrs = "1";
    quint8 readFuncCode = 0x03;
    quint16 readStartAddr = 0;
    quint16 readCount = 1;

    // Параметры записи
    quint8 writeDeviceAddr = 1;
    quint8 writeFuncCode = 0x06;
    quint16 writeStartAddr = 0;
    quint16 writeCount = 1;
    QString writeData = "";
};

// Настройки программы
struct Preferences {
    quint16 pollInterval = 2000; // Интервал опроса в миллисекундах
    quint16 modbusRtuTimeout = 1000; // Таймаут на чтение устройства по RTU
    quint8 modbusRtuRetries = 3; // Количество попыток чтения устройства по RTU
    quint16 modbusTcpTimeout = 1000; // Таймаут на чтение устройства по TCP
    quint8 modbusTcpRetries = 1; // Количество попыток чтения устройства по TCP
};

class SettingsManager
{
public:
    // Возвращает полный путь к файлу конфигурации
    static QString configFilePath();

    // Сохраняет настройки порта в JSON-файл. Возвращает true при успехе.
    static bool savePortSettings(const ModbusConnectionSettings &settings);

    // Загружает настройки порта из JSON-файла.
    // Если файла нет или он поврежден, возвращает настройки по умолчанию.
    static ModbusConnectionSettings loadPortSettings();

    // Проверяет, существует ли файл конфигурации
    static bool settingsExist();

    // Последние параметры команд (чтение/запись)
    static bool saveLastParams(const ModbusLastParams &params);
    static ModbusLastParams loadLastParams();

    static bool savePreferences(const Preferences& preferences);
    static Preferences loadPreferences();

private:
    // Вспомогательные методы для работы с корневым JSON-объектом
    static QJsonObject loadRootObject();
    static bool saveRootObject(const QJsonObject &rootObj);
};

#endif // SETTINGSMANAGER_H
