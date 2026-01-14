/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "main_window_initializer.h"

#include <cmath>
#include <memory>
#include <utility>

#include <QAbstractButton>
#include <QCompleter>
#include <QDebug>
#include <QDoubleValidator>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHostAddress>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QObject>
#include <QPoint>
#include <QShortcut>
#include <QSize>
#include <QSplitter>
#include <QString>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

#include "main_window.h"
#include "settings_manager.h"
#include "ui/go_to_widget.h"
#include "ui/symbol_completer.h"
#include "ui_main_window.h"

namespace oid
{

MainWindowInitializer::MainWindowInitializer(Dependencies deps)
    : deps_{std::move(deps)}
{
}

void MainWindowInitializer::initialize()
{
    initialize_ui_icons();
    initialize_ui_signals();
    initialize_timers();
    initialize_symbol_completer();
    initialize_left_pane();
    initialize_auto_contrast_form();
    initialize_toolbar();
    initialize_status_bar();
    initialize_visualization_pane();
    initialize_go_to_widget();
    initialize_shortcuts();
    initialize_networking();
}

void MainWindowInitializer::initialize_ui_icons() const
{
    if (QFontDatabase::addApplicationFont(":/resources/icons/fontello.ttf") <
        0) {
        qWarning() << "Could not load ionicons font file!";
    }

    setFontIcon(deps_.ui_components.ui->acEdit, L"\ue803");
    setFontIcon(deps_.ui_components.ui->acToggle, L"\ue804");
    setFontIcon(deps_.ui_components.ui->reposition_buffer, L"\ue800");
    setFontIcon(deps_.ui_components.ui->linkViewsToggle, L"\ue805");
    setFontIcon(deps_.ui_components.ui->rotate_90_ccw, L"\ue801");
    setFontIcon(deps_.ui_components.ui->rotate_90_cw, L"\ue802");
    setFontIcon(deps_.ui_components.ui->decrease_float_precision, L"\ue806");
    setFontIcon(deps_.ui_components.ui->increase_float_precision, L"\ue807");
    setFontIcon(deps_.ui_components.ui->go_to_pixel, L"\uf031");

    setFontIcon(deps_.ui_components.ui->ac_reset_min, L"\ue808");
    setFontIcon(deps_.ui_components.ui->ac_reset_max, L"\ue808");

    setVectorIcon(
        deps_.ui_components.ui->label_c1_min,
        QString("label_%1_channel.svg").arg(deps_.channel_names.name_channel_1),
        10,
        10);
    setVectorIcon(
        deps_.ui_components.ui->label_c1_max,
        QString("label_%1_channel.svg").arg(deps_.channel_names.name_channel_1),
        10,
        10);
    setVectorIcon(
        deps_.ui_components.ui->label_c2_min,
        QString("label_%1_channel.svg").arg(deps_.channel_names.name_channel_2),
        10,
        10);
    setVectorIcon(
        deps_.ui_components.ui->label_c2_max,
        QString("label_%1_channel.svg").arg(deps_.channel_names.name_channel_2),
        10,
        10);
    setVectorIcon(
        deps_.ui_components.ui->label_c3_min,
        QString("label_%1_channel.svg").arg(deps_.channel_names.name_channel_3),
        10,
        10);
    setVectorIcon(
        deps_.ui_components.ui->label_c3_max,
        QString("label_%1_channel.svg").arg(deps_.channel_names.name_channel_3),
        10,
        10);
    setVectorIcon(
        deps_.ui_components.ui->label_c4_min,
        QString("label_%1_channel.svg").arg(deps_.channel_names.name_channel_4),
        10,
        10);
    setVectorIcon(
        deps_.ui_components.ui->label_c4_max,
        QString("label_%1_channel.svg").arg(deps_.channel_names.name_channel_4),
        10,
        10);

    setVectorIcon(
        deps_.ui_components.ui->label_minmax, "lower_upper_bound.svg", 8, 35);
}

void MainWindowInitializer::initialize_ui_signals()
{
    QObject::connect(
        deps_.ui_components.ui->splitter,
        &QSplitter::splitterMoved,
        &deps_.main_window,
        [this]() { deps_.settings_manager.persist_settings_deferred(); });

    QObject::connect(
        deps_.ui_components.ui->acEdit,
        &QAbstractButton::clicked,
        &deps_.main_window,
        [this]() { deps_.settings_manager.persist_settings_deferred(); });

    QObject::connect(
        deps_.ui_components.ui->acToggle,
        &QAbstractButton::clicked,
        &deps_.main_window,
        [this]() { deps_.settings_manager.persist_settings_deferred(); });

    QObject::connect(
        deps_.ui_components.ui->linkViewsToggle,
        &QAbstractButton::clicked,
        &deps_.main_window,
        [this]() { deps_.settings_manager.persist_settings_deferred(); });
}

void MainWindowInitializer::initialize_timers()
{
    QObject::connect(&deps_.ui_components.settings_persist_timer,
                     &QTimer::timeout,
                     &deps_.main_window,
                     [this]() { deps_.settings_manager.persist_settings(); });
    deps_.ui_components.settings_persist_timer.setSingleShot(true);

    QObject::connect(&deps_.ui_components.update_timer,
                     &QTimer::timeout,
                     &deps_.main_window,
                     [this]() { deps_.main_window.loop(); });
}

void MainWindowInitializer::initialize_shortcuts()
{
    auto symbol_list_focus_shortcut = std::make_unique<QShortcut>(
        QKeySequence::fromString("Ctrl+K"), &deps_.main_window);
    QObject::connect(
        symbol_list_focus_shortcut.release(),
        &QShortcut::activated,
        deps_.ui_components.ui->symbolList,
        [this]() { deps_.ui_components.ui->symbolList->setFocus(); });

    auto buffer_removal_shortcut = std::make_unique<QShortcut>(
        QKeySequence{Qt::Key_Delete}, deps_.ui_components.ui->imageList);
    QObject::connect(
        buffer_removal_shortcut.release(),
        &QShortcut::activated,
        &deps_.main_window,
        [this]() { deps_.event_handler.remove_selected_buffer(); });

    auto go_to_shortcut = std::make_unique<QShortcut>(
        QKeySequence::fromString("Ctrl+L"), &deps_.main_window);
    QObject::connect(go_to_shortcut.release(),
                     &QShortcut::activated,
                     &deps_.main_window,
                     [this]() { deps_.event_handler.toggle_go_to_dialog(); });
    QObject::connect(
        deps_.ui_components.go_to_widget.get(),
        &GoToWidget::go_to_requested,
        &deps_.main_window,
        [this](float x, float y) { deps_.event_handler.go_to_pixel(x, y); });
}

void MainWindowInitializer::initialize_symbol_completer()
{
    deps_.ui_components.symbol_completer =
        std::make_unique<SymbolCompleter>(&deps_.main_window);

    deps_.ui_components.symbol_completer->setCaseSensitivity(
        Qt::CaseSensitivity::CaseInsensitive);
    deps_.ui_components.symbol_completer->setCompletionMode(
        QCompleter::PopupCompletion);
    deps_.ui_components.symbol_completer->setModelSorting(
        QCompleter::CaseInsensitivelySortedModel);

    deps_.ui_components.ui->symbolList->set_completer(
        *deps_.ui_components.symbol_completer);
    QObject::connect(deps_.ui_components.symbol_completer.get(),
                     qOverload<const QString&>(&QCompleter::activated),
                     &deps_.main_window,
                     [this](const QString& str) {
                         deps_.event_handler.symbol_completed(str);
                     });
}

void MainWindowInitializer::initialize_left_pane() const
{
    QObject::connect(deps_.ui_components.ui->imageList,
                     &QListWidget::currentItemChanged,
                     &deps_.main_window,
                     [this](QListWidgetItem* item) {
                         deps_.event_handler.buffer_selected(item);
                     });

    QObject::connect(deps_.ui_components.ui->symbolList,
                     &QLineEdit::editingFinished,
                     &deps_.main_window,
                     [this]() { deps_.event_handler.symbol_selected(); });

    deps_.ui_components.ui->imageList->setContextMenuPolicy(
        Qt::CustomContextMenu);
    QObject::connect(deps_.ui_components.ui->imageList,
                     &QWidget::customContextMenuRequested,
                     &deps_.main_window,
                     [this](const QPoint& pos) {
                         deps_.event_handler.show_context_menu(pos);
                     });
}

void MainWindowInitializer::initialize_auto_contrast_form() const
{
    // Configure auto contrast inputs
    // Note: Qt's setValidator takes ownership, so we use make_unique and
    // release
    deps_.ui_components.ui->ac_c1_min->setValidator(
        std::make_unique<QDoubleValidator>(deps_.ui_components.ui->ac_c1_min)
            .release());
    deps_.ui_components.ui->ac_c2_min->setValidator(
        std::make_unique<QDoubleValidator>(deps_.ui_components.ui->ac_c2_min)
            .release());
    deps_.ui_components.ui->ac_c3_min->setValidator(
        std::make_unique<QDoubleValidator>(deps_.ui_components.ui->ac_c3_min)
            .release());

    deps_.ui_components.ui->ac_c1_max->setValidator(
        std::make_unique<QDoubleValidator>(deps_.ui_components.ui->ac_c1_max)
            .release());
    deps_.ui_components.ui->ac_c2_max->setValidator(
        std::make_unique<QDoubleValidator>(deps_.ui_components.ui->ac_c2_max)
            .release());
    deps_.ui_components.ui->ac_c3_max->setValidator(
        std::make_unique<QDoubleValidator>(deps_.ui_components.ui->ac_c3_max)
            .release());

    QObject::connect(deps_.ui_components.ui->ac_c1_min,
                     &QLineEdit::editingFinished,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_c1_min_update(); });
    QObject::connect(deps_.ui_components.ui->ac_c1_max,
                     &QLineEdit::editingFinished,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_c1_max_update(); });
    QObject::connect(deps_.ui_components.ui->ac_c2_min,
                     &QLineEdit::editingFinished,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_c2_min_update(); });
    QObject::connect(deps_.ui_components.ui->ac_c2_max,
                     &QLineEdit::editingFinished,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_c2_max_update(); });
    QObject::connect(deps_.ui_components.ui->ac_c3_min,
                     &QLineEdit::editingFinished,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_c3_min_update(); });
    QObject::connect(deps_.ui_components.ui->ac_c3_max,
                     &QLineEdit::editingFinished,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_c3_max_update(); });
    QObject::connect(deps_.ui_components.ui->ac_c4_min,
                     &QLineEdit::editingFinished,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_c4_min_update(); });
    QObject::connect(deps_.ui_components.ui->ac_c4_max,
                     &QLineEdit::editingFinished,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_c4_max_update(); });

    QObject::connect(deps_.ui_components.ui->ac_reset_min,
                     &QAbstractButton::clicked,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_min_reset(); });
    QObject::connect(deps_.ui_components.ui->ac_reset_max,
                     &QAbstractButton::clicked,
                     &deps_.main_window,
                     [this]() { deps_.ac_controller.ac_max_reset(); });
}

void MainWindowInitializer::initialize_toolbar() const
{
    QObject::connect(
        deps_.ui_components.ui->acToggle,
        &QToolButton::toggled,
        &deps_.main_window,
        [this](bool is_checked) { deps_.ac_controller.ac_toggle(is_checked); });

    QObject::connect(deps_.ui_components.ui->reposition_buffer,
                     &QAbstractButton::clicked,
                     &deps_.main_window,
                     [this]() { deps_.event_handler.recenter_buffer(); });

    QObject::connect(deps_.ui_components.ui->linkViewsToggle,
                     &QAbstractButton::clicked,
                     &deps_.main_window,
                     [this]() { deps_.event_handler.link_views_toggle(); });

    QObject::connect(deps_.ui_components.ui->rotate_90_cw,
                     &QAbstractButton::clicked,
                     &deps_.main_window,
                     [this]() { deps_.event_handler.rotate_90_cw(); });
    QObject::connect(deps_.ui_components.ui->rotate_90_ccw,
                     &QAbstractButton::clicked,
                     &deps_.main_window,
                     [this]() { deps_.event_handler.rotate_90_ccw(); });
    QObject::connect(
        deps_.ui_components.ui->increase_float_precision,
        &QAbstractButton::clicked,
        &deps_.main_window,
        [this]() { deps_.event_handler.increase_float_precision(); });
    QObject::connect(
        deps_.ui_components.ui->decrease_float_precision,
        &QAbstractButton::clicked,
        &deps_.main_window,
        [this]() { deps_.event_handler.decrease_float_precision(); });
    QObject::connect(deps_.ui_components.ui->go_to_pixel,
                     &QAbstractButton::clicked,
                     &deps_.main_window,
                     [this]() { deps_.event_handler.toggle_go_to_dialog(); });

    deps_.ui_components.ui->increase_float_precision->setEnabled(false);
    deps_.ui_components.ui->decrease_float_precision->setEnabled(false);
}

void MainWindowInitializer::initialize_visualization_pane()
{
    deps_.ui_components.ui->bufferPreview->set_main_window(deps_.main_window);
}

void MainWindowInitializer::initialize_status_bar()
{
    deps_.ui_components.status_bar =
        std::make_unique<QLabel>(&deps_.main_window);
    deps_.ui_components.status_bar->setAlignment(Qt::AlignRight);

    deps_.main_window.statusBar()->addWidget(
        deps_.ui_components.status_bar.get(), 1);
}

void MainWindowInitializer::initialize_go_to_widget()
{
    deps_.ui_components.go_to_widget =
        std::make_unique<GoToWidget>(deps_.ui_components.ui->bufferPreview);
}

void MainWindowInitializer::initialize_networking()
{
    deps_.socket.connectToHost(QString(deps_.host_settings.url.c_str()),
                               deps_.host_settings.port);
    deps_.socket.waitForConnected();
}

void MainWindowInitializer::setFontIcon(QAbstractButton* ui_element,
                                        const wchar_t unicode_id[])
{
    auto icons_font = QFont{};
    icons_font.setFamily("fontello");
    icons_font.setPointSizeF(10.f);

    ui_element->setFont(icons_font);
    ui_element->setText(QString::fromWCharArray(unicode_id));
}

void MainWindowInitializer::setVectorIcon(QLabel* ui_element,
                                          const QString& icon_file_name,
                                          const int width,
                                          const int height)
{
    const auto screen_dpi_scale =
        QGuiApplication::primaryScreen()->devicePixelRatio();

    ui_element->setScaledContents(true);
    ui_element->setMinimumWidth(static_cast<int>(std::round(width)));
    ui_element->setMaximumWidth(static_cast<int>(std::round(width)));
    ui_element->setMinimumHeight(static_cast<int>(std::round(height)));
    ui_element->setMaximumHeight(static_cast<int>(std::round(height)));
    ui_element->setPixmap(
        QIcon(QString(":/resources/icons/%1").arg(icon_file_name))
            .pixmap(QSize(
                static_cast<int>(std::round(width * screen_dpi_scale)),
                static_cast<int>(std::round(height * screen_dpi_scale)))));
    ui_element->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

} // namespace oid
