#include "mainwindow.h"

#include <QApplication>
#include <QStyleFactory>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    // Регистрация кастомного типа для сигналов/слотов
    qRegisterMetaType<ModbusRequest>("ModbusRequest");
    qRegisterMetaType<QModbusDataUnit>("QModbusDataUnit");

    // Включаем поддержку высокого DPI
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    const QString ORGANIZATION_NAME = "Smukm";
    const QString APP_NAME = "Sensors";
    QCoreApplication::setOrganizationDomain(ORGANIZATION_NAME);
    QCoreApplication::setApplicationName(APP_NAME);

    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "sensors_" + QLocale(locale).name();
        if (translator.load(baseName, ":/translations")) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
}
