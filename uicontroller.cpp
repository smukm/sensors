#include "uicontroller.h"
#include "./ui_mainwindow.h" // Реальное определение UI

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
UiController::UiController(Ui::MainWindow *ui) : m_ui(ui) {}


/**
 * @brief Инициализация начального состояния элементов интерфейса.
 * Устанавливает тексты-заглушки для полей ввода и блокирует кнопки управления,
 * которые должны стать доступны только после установления соединения с устройством.
 */
void UiController::initUiElements() {
    m_ui->c19le->setText("20.1 %");
    m_ui->c27le->setText("12.3 %");

    m_ui->connectToRmtRb->setEnabled(false);
    m_ui->recordingToFileRb->setEnabled(false);
}

/**
 * @brief Переводит UI в состояние "Подключено".
 * Разблокирует элементы управления, требующие активной связи с устройством.
 */
void UiController::setConnectedState() {
    m_ui->connectToRmtRb->setEnabled(true);
}

/**
 * @brief Переводит UI в состояние "Отключено".
 * Блокирует элементы управления для предотвращения отправки запросов в пустоту.
 */
void UiController::setDisconnectedState() {
    m_ui->connectToRmtRb->setEnabled(false);
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