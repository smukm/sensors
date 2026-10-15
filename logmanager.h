#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>

class LogManager : public QObject
{
    Q_OBJECT
public:
    explicit LogManager(QObject *parent = nullptr);
    void addLog(const QString &message, bool isError = false);
    void clear();

signals:
};

#endif // LOGMANAGER_H
