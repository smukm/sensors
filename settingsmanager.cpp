#include "settingsmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QDebug>
#include <QObject>

// Имя файла конфигурации
static const QString CONFIG_FILE_NAME = "sensors_settings.json";

// Ключи верхнего уровня (ветки)
static const QString KEY_RMT_SETTINGS  = "rmtSettings";
static const QString KEY_OTHER_SETTINGS  = "otherSettings";

// Ключи внутри ветки rmtSettings
static const QString KEY_IP_ADDRESS  = "ipAddress";
static const QString KEY_PORT  = "port";
static const QString KEY_DEVICE_NUMBER     = "deviceNumer";
static const QString KEY_SIGNAL_START  = "signalStart";
static const QString KEY_SIGNAL_REFUELING  = "signalRefueling";

// Ключи внутри ветки otherSettings



QString SettingsManager::configFilePath()
{
    // Используем стандартную папку для конфигураций приложения
    // Windows: C:/Users/<User>/AppData/Local/<AppName>/
    // Linux:   ~/.local/share/<AppName>/
    // macOS:   ~/Library/Application Support/<AppName>/
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return dir.filePath(CONFIG_FILE_NAME);
}

// =========================================================================
// Вспомогательные методы для безопасной работы с ветками JSON
// =========================================================================

QJsonObject SettingsManager::loadRootObject()
{
    const QString path = configFilePath();
    QFile file(path);

    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray jsonData = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            return doc.object();
        }
        qWarning() << QObject::tr("SettingsManager: JSON parsing error, using empty object:") << parseError.errorString();
    }
    return QJsonObject(); // Возвращаем пустой объект, если файла нет или он битый
}

bool SettingsManager::saveRootObject(const QJsonObject &rootObj)
{
    QJsonDocument doc(rootObj);
    QFile file(configFilePath());

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << QObject::tr("SettingsManager: Failed to open file for writing:") << file.errorString();
        return false;
    }

    QTextStream out(&file);
    out << doc.toJson(QJsonDocument::Indented);
    file.close();
    return true;
}


bool SettingsManager::saveRmtSettings(const ModbusConnectionSettings &settings)
{
    QJsonObject rootObj = loadRootObject();

    QJsonObject portObj;
    portObj[KEY_IP_ADDRESS] = settings.ipAddress;
    portObj[KEY_PORT] = settings.port;
    portObj[KEY_DEVICE_NUMBER]    = settings.deviceNumber;
    portObj[KEY_SIGNAL_START] = settings.signalStart;
    portObj[KEY_SIGNAL_REFUELING] = settings.signalRefueling;

    rootObj[KEY_RMT_SETTINGS] = portObj;

    return saveRootObject(rootObj);
}

ModbusConnectionSettings SettingsManager::loadRmtSettings()
{
    ModbusConnectionSettings defaults;
    defaults.ipAddress = "127.0.0.1";
    defaults.port = 502;
    defaults.deviceNumber = 1;
    defaults.signalStart = "Пуск";
    defaults.signalRefueling = "Заправка";

    QJsonObject rootObj = loadRootObject();
    if (!rootObj.contains(KEY_RMT_SETTINGS)) {
        return defaults;
    }

    QJsonObject portObj = rootObj[KEY_RMT_SETTINGS].toObject();
    ModbusConnectionSettings settings = defaults;
    settings.ipAddress = portObj[KEY_IP_ADDRESS].toString(defaults.ipAddress);
    settings.port = static_cast<qint16>(portObj[KEY_PORT].toInt(defaults.port));
    settings.deviceNumber = static_cast<qint16>(portObj[KEY_DEVICE_NUMBER].toInt(defaults.deviceNumber));
    settings.signalStart = portObj[KEY_SIGNAL_START].toString(defaults.signalStart);
    settings.signalRefueling = portObj[KEY_SIGNAL_REFUELING].toString(defaults.signalRefueling);

    return settings;
}

bool SettingsManager::settingsExist()
{
    return QFile::exists(configFilePath());
}


Preferences SettingsManager::loadPreferences() {
    Preferences defaults;


    return defaults;
}