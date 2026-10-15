#include "datamodel.h"

DataModel::DataModel(QObject *parent)
    : QStandardItemModel{parent}
{
    setColumnCount(5);
    setHorizontalHeaderLabels({
        tr("Time min"),
        tr("min"),
        tr("Point"),
        tr("max"),
        tr("Time max")
    });
}
