#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QStandardItemModel>
#include <QModbusDataUnit>

class DataModel : public QStandardItemModel
{
public:
    explicit DataModel(QObject *parent = nullptr);

    void updateData(quint8 serverAddress, const QModbusDataUnit &unit);
};

#endif // DATAMODEL_H
