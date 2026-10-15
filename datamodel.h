#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QStandardItemModel>

class DataModel : public QStandardItemModel
{
public:
    explicit DataModel(QObject *parent = nullptr);
};

#endif // DATAMODEL_H
