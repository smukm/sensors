#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H
#include "abstractmodbusmanager.h"


// Настройки программы
struct Preferences {
    quint16 pollInterval = 2000; // Интервал опроса в миллисекундах
    quint16 modbusRtuTimeout = 1000; // Таймаут на чтение устройства по RTU
    quint8 modbusRtuRetries = 3; // Количество попыток чтения устройства по RTU
    quint16 modbusTcpTimeout = 1000; // Таймаут на чтение устройства по TCP
    quint8 modbusTcpRetries = 0; // Количество попыток чтения устройства по TCP
};

class SettingsManager
{
public:
    // Возвращает полный путь к файлу конфигурации
    static QString configFilePath();

    // Сохраняет настройки порта в JSON-файл. Возвращает true при успехе.
    static bool saveRmtSettings(const ModbusConnectionSettings &settings);

    // Загружает настройки порта из JSON-файла.
    // Если файла нет или он поврежден, возвращает настройки по умолчанию.
    static ModbusConnectionSettings loadRmtSettings();

    // Проверяет, существует ли файл конфигурации
    static bool settingsExist();

    static Preferences loadPreferences();

private:
    // Вспомогательные методы для работы с корневым JSON-объектом
    static QJsonObject loadRootObject();
    static bool saveRootObject(const QJsonObject &rootObj);
};

#endif // SETTINGSMANAGER_H
