#ifndef MODBUSVALIDATOR_H
#define MODBUSVALIDATOR_H
#include <QString>
#include <QVector>

struct ValidationResult {
    bool isValid;
    QString errorMessage;
};

class ModbusValidator
{
public:
    // Валидация базовых параметров
    static ValidationResult validateBasicParams(quint8 deviceAddr, quint8 funcCode, quint16 startAddr, quint16 count);

    // Валидация данных для записи
    static ValidationResult validateWriteData(quint8 funcCode, const QString &dataText, QVector<quint16> &outValues);
};

#endif // MODBUSVALIDATOR_H
