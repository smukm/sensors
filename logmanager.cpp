#include "logmanager.h"
#include <QDebug>

LogManager::LogManager(QObject *parent)
    : QObject{parent}
{}

void LogManager::addLog(const QString& message, bool isError) {
    if (isError) {
        qWarning() << message;
    } else {
        qDebug() << message;
    }
}