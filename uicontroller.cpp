#include "uicontroller.h"
#include "./ui_mainwindow.h" // Реальное определение UI
#include "settingsmanager.h"
#include <QMessageBox>
#include <QTimer>
#include <QHostAddress>
#include <QAbstractSocket>

/**
 * @brief Структура для хранения относительных координат элемента.
 * @note В текущей реализации не используется (в заголовочном файле применяется ElementInfo),
 * но оставлена здесь как пример структуры для хранения нормализованных координат (долей от 0.0 до 1.0).
 */
struct ElementPos {
    QWidget *widget;
    double x;      // доля от ширины (0.0 - 1.0)
    double y;      // доля от высоты (0.0 - 1.0)
    double w;      // доля от ширины
    double h;      // доля от высоты
};

// Эталонные (базовые) размеры мнемосхемы, заданные в .ui файле.
// Все вычисления масштаба будут производиться относительно этих значений.
static const double BASE_W = 1920.0;
static const double BASE_H = 1080.0;

/**
 * @brief Конструктор контроллера UI.
 * Сохраняет указатель на сгенерированный класс UI для прямого доступа к виджетам.
 */
UiController::UiController(Ui::MainWindow *ui, QObject *parent) : QObject(parent), m_ui(ui) {}


/**
 * @brief Инициализация начального состояния элементов интерфейса.
 * Устанавливает тексты-заглушки для полей ввода и блокирует кнопки управления,
 * которые должны стать доступны только после установления соединения с устройством.
 */
void UiController::initUiElements() {
    m_ui->c19le->setText("20.1 %");
    m_ui->c27le->setText("12.3 %");


    // Инициализация radiobuttons сверху
    // Отключаем автоматическое исключение в группе
    m_ui->connectToRmtRb->setAutoExclusive(false);
    m_ui->recordingToFileRb->setAutoExclusive(false);
    // Блокируем клики мышью
    m_ui->connectToRmtRb->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_ui->recordingToFileRb->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_ui->connectToRmtRb->setChecked(false);
    m_ui->recordingToFileRb->setChecked(false);


    // Чтение settings и заполнение ui элементов с параметрами настройки
    ModbusConnectionSettings settings = SettingsManager::loadRmtSettings();
    m_ui->ipAddressSettingsLe->setText(settings.ipAddress);
    m_ui->portSettingsLe->setText(QString::number(settings.port));
    m_ui->deviceNumberSettingsLe->setText(QString::number(settings.deviceNumber));
    m_ui->signalStartSettingsLe->setText(settings.signalStart);
    m_ui->signalRefuelingSettingsLe->setText(settings.signalRefueling);
}

/**
 * @brief Переводит UI в состояние "Подключено".
 * Разблокирует элементы управления, требующие активной связи с устройством.
 */
void UiController::setConnectedState() {
    m_ui->connectToRmtRb->setChecked(true);
}

/**
 * @brief Переводит UI в состояние "Отключено".
 * Блокирует элементы управления для предотвращения отправки запросов в пустоту.
 */
void UiController::setDisconnectedState() {
    m_ui->connectToRmtRb->setChecked(false);
}

/**
 * @brief Сохраняет исходные (эталонные) геометрии виджетов мнемосхемы.
 * Этот метод должен быть вызван СТРОГО один раз сразу после setupUi() и ДО первого
 * изменения размера окна. Он кэширует координаты из .ui файла, чтобы использовать
 * их как базу (100%) для всех последующих вычислений масштаба.
 * Повторные вызовы игнорируются благодаря флагу m_positionsSaved.
 */
void UiController::saveBasePositions()
{
    if (m_positionsSaved)
        return;

    // Список всех виджетов мнемосхемы, которые нужно масштабировать.
    // При добавлении новых элементов на схему их нужно регистрировать здесь.
    QList<QWidget*> widgets = {
        m_ui->c27lbl,
        m_ui->c27le,
        m_ui->c19lbl,
        m_ui->c19le,
        m_ui->tableView,
    };

    // Сохраняем текущую геометрию (из .ui файла) как базовую
    for (QWidget *w : widgets) {
        m_elements.append({ w, w->geometry() });
    }

    m_positionsSaved = true;
}

/**
 * @brief Пересчитывает и применяет новые координаты и размеры для всех элементов мнемосхемы.
 * Вызывается при каждом изменении размера окна (resizeEvent).
 * Работает только для первой вкладки (индекс 0), где расположена мнемосхема.
 *
 * Алгоритм:
 * 1. Вычисляются коэффициенты масштабирования (sx, sy) как отношение текущих
 *    размеров контейнера к эталонным (1920x1080).
 * 2. Базовые координаты каждого виджета умножаются на эти коэффициенты.
 * 3. Результат округляется (qRound) для избежания дробных пикселей и артефактов отрисовки.
 */
void UiController::repositionElements()
{
    // Масштабирование применяется только к мнемосхеме (вкладка 0)
    if (m_ui->stackedWidget->currentIndex() != 0)
        return;

    // Получаем текущие размеры контейнера мнемосхемы
    const int pw = m_ui->stand_1->width();
    const int ph = m_ui->stand_1->height();

    if (pw <= 0 || ph <= 0)
        return;

    // Вычисляем коэффициенты масштабирования по осям X и Y
    const double sx = static_cast<double>(pw) / BASE_W;
    const double sy = static_cast<double>(ph) / BASE_H;

    // Применяем масштаб к каждому зарегистрированному элементу
    for (const auto &e : m_elements) {
        e.widget->setGeometry(
            qRound(e.baseGeometry.x() * sx),
            qRound(e.baseGeometry.y() * sy),
            qRound(e.baseGeometry.width() * sx),
            qRound(e.baseGeometry.height() * sy)
            );
    }
}

/**
 * @brief Валидирует и сохраняет настройки вкладки Settings.
 *
 **/
void UiController::on_saveSettings_clicked()
{
    bool okPort, okDevice, okSignalStart, okSignalRefueling;

    // Чтение и очистка данных от лишних пробелов
    QString ipAddress = m_ui->ipAddressSettingsLe->text().trimmed();
    QString portStr = m_ui->portSettingsLe->text().trimmed();
    QString deviceStr = m_ui->deviceNumberSettingsLe->text().trimmed();
    QString signalStartStr = m_ui->signalStartSettingsLe->text().trimmed();
    QString signalRefuelingStr = m_ui->signalRefuelingSettingsLe->text().trimmed();

    // Валидация IP-адреса (должен быть корректным IPv4)
    QHostAddress ipAddr(ipAddress);
    if (ipAddress.isEmpty() || ipAddr.isNull() || ipAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        QMessageBox::warning(m_ui->centralwidget,
                             QObject::tr("Input Error"),
                             QObject::tr("Invalid IP address.\nPlease enter a valid IPv4 address (e.g., 192.168.1.10)."));
        return;
    }

    // Валидация Порта (1 - 65535)
    int portInt = portStr.toInt(&okPort);
    if (!okPort || portInt < 1 || portInt > 65535) {
        QMessageBox::warning(m_ui->centralwidget,
                             QObject::tr("Input Error"),
                             QObject::tr("Invalid port number.\nMust be a number between 1 and 65535."));
        return;
    }

    // Валидация номера устройства (1 - 247 для Modbus)
    int deviceInt = deviceStr.toInt(&okDevice);
    if (!okDevice || deviceInt < 1 || deviceInt > 247) {
        QMessageBox::warning(m_ui->centralwidget,
                             QObject::tr("Input Error"),
                             QObject::tr("Invalid device number.\nMust be a number between 1 and 247."));
        return;
    }

    // Валидация адреса "Signal Start"
    // (Base 0 позволяет вводить как "68", так и "0x44")
    // int signalStartInt = signalStartStr.toInt(&okSignalStart, 0);
    // if (signalStartStr.isEmpty() || !okSignalStart || signalStartInt < 0 || signalStartInt > 65535) {
    //     QMessageBox::warning(m_ui->centralwidget,
    //                          QObject::tr("Input Error"),
    //                          QObject::tr("Invalid 'Signal Start' address.\nMust be a valid number between 0 and 65535 (e.g., 68 or 0x44)."));
    //     return;
    // }

    // Валидация адреса "Signal Refueling"
    // int signalRefuelingInt = signalRefuelingStr.toInt(&okSignalRefueling, 0);
    // if (signalRefuelingStr.isEmpty() || !okSignalRefueling || signalRefuelingInt < 0 || signalRefuelingInt > 65535) {
    //     QMessageBox::warning(m_ui->centralwidget,
    //                          QObject::tr("Input Error"),
    //                          QObject::tr("Invalid 'Signal Refueling' address.\nMust be a valid number between 0 and 65535 (e.g., 68 or 0x44)."));
    //     return;
    // }

    // Если все проверки пройдены, формируем и сохраняем настройки
    ModbusConnectionSettings settings;
    settings.ipAddress = ipAddress;
    settings.port = static_cast<qint16>(portInt);
    settings.deviceNumber = static_cast<quint8>(deviceInt);
    settings.signalStart = signalStartStr;       // Сохраняем как строку, чтобы сохранить формат (например, "0x44")
    settings.signalRefueling = signalRefuelingStr;

    if (!SettingsManager::saveRmtSettings(settings)) {
        QMessageBox::critical(m_ui->centralwidget,
                              QObject::tr("Error"),
                              QObject::tr("Failed to save settings to file."));
    } else {
        // Успешное сохранение: показываем сообщение зеленым цветом
        m_ui->settingsMsgLbl->setText("<span style='color: green; font-weight: bold;'>Settings saved successfully</span>");

        QTimer::singleShot(3000, m_ui->settingsMsgLbl, &QLabel::clear);

        emit settingsApplied(settings);
    }
}

