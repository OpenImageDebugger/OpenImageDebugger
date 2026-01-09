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

#include "main_window.h"

#include <cmath>

#include <memory>

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QFontDatabase>
#include <QHostAddress>
#include <QMetaType>
#include <QSettings>
#include <QShortcut>

#include "ui_main_window.h"

namespace oid
{

void MainWindow::initialize_settings_ui_list_position(
    const QSettings& settings) const
{
    const auto variant = settings.value("list_position");
    if (!variant.canConvert<QString>()) {
        return;
    }

    const auto position_str = variant.toString();

    if (position_str == "top" || position_str == "bottom") {
        ui_components_.ui->splitter->setOrientation(Qt::Vertical);
    }

    if (position_str == "right" || position_str == "bottom") {
        ui_components_.ui->splitter->insertWidget(
            -1, ui_components_.ui->frame_list);
    }

    ui_components_.ui->splitter->repaint();
}

void MainWindow::initialize_settings_ui_splitter(
    const QSettings& settings) const
{
    const auto variant = settings.value("splitter");
    if (!variant.canConvert<QVariantList>()) {
        return;
    }

    const auto listVariants = variant.toList();

    auto listSizes = QList<int>{};
    for (const QVariant& size : listVariants) {
        listSizes.append(size.toInt());
    }

    ui_components_.ui->splitter->setSizes(listSizes);
}

void MainWindow::initialize_settings_ui_minmax_compact(
    const QSettings& settings) const
{
    const auto is_minmax_compact = [&] {
        const auto variant = settings.value("minmax_compact");
        if (!variant.canConvert<bool>()) {
            return false;
        }

        return variant.toBool();
    }();

    if (!is_minmax_compact) {
        return;
    }

    const auto is_minmax_visible = [&] {
        const auto variant = settings.value("minmax_visible");
        if (!variant.canConvert<bool>()) {
            return true;
        }

        return variant.toBool();
    }();

    if (is_minmax_visible) {
        ui_components_.ui->gridLayout_toolbar->addWidget(
            ui_components_.ui->linkViewsToggle, 0, 0);
        ui_components_.ui->gridLayout_toolbar->addWidget(
            ui_components_.ui->reposition_buffer, 0, 1);
        ui_components_.ui->gridLayout_toolbar->addWidget(
            ui_components_.ui->go_to_pixel, 0, 2);

        ui_components_.ui->gridLayout_toolbar->addWidget(
            ui_components_.ui->rotate_90_ccw, 1, 0);
        ui_components_.ui->gridLayout_toolbar->addWidget(
            ui_components_.ui->rotate_90_cw, 1, 1);
        ui_components_.ui->gridLayout_toolbar->addWidget(
            ui_components_.ui->acToggle, 1, 2);
        ui_components_.ui->gridLayout_toolbar->addWidget(
            ui_components_.ui->decrease_float_precision, 1, 3);
        ui_components_.ui->gridLayout_toolbar->addWidget(
            ui_components_.ui->increase_float_precision, 1, 4);
    }

    ui_components_.ui->horizontalLayout_container_toolbar->addWidget(
        ui_components_.ui->minMaxEditor, 2);
    ui_components_.ui->horizontalLayout_container_toolbar->setStretch(0, 0);
    ui_components_.ui->horizontalLayout_container_toolbar->setStretch(1, 1);
    ui_components_.ui->horizontalLayout_container_toolbar->setStretch(2, 0);

    ui_components_.ui->acEdit->hide();
}

QString
MainWindow::initialize_settings_ui_colorspace_channel(const QChar& character)
{
    switch (character.toLatin1()) {
    case 'b':
        return "blue";
    case 'g':
        return "green";
    case 'r':
        return "red";
    case 'a':
        return "alpha";
    default:
        return {};
    }
}

void MainWindow::initialize_settings_ui_colorspace(const QSettings& settings)
{
    const auto variant = settings.value("colorspace");
    if (!variant.canConvert<QString>()) {
        return;
    }

    const auto colorspace_str = variant.toString();

    if (!colorspace_str.isEmpty()) {
        channel_names_.name_channel_1 =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(0));
    }
    if (colorspace_str.size() > 1) {
        channel_names_.name_channel_2 =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(1));
    }
    if (colorspace_str.size() > 2) {
        channel_names_.name_channel_3 =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(2));
    }
    if (colorspace_str.size() > 3) {
        channel_names_.name_channel_4 =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(3));
    }
}

void MainWindow::initialize_settings_ui_minmax_visible(
    const QSettings& settings) const
{
    const auto variant = settings.value("minmax_visible");
    if (!variant.canConvert<bool>()) {
        return;
    }

    const auto is_minmax_visible = variant.toBool();
    ui_components_.ui->acEdit->setChecked(is_minmax_visible);
    ui_components_.ui->minMaxEditor->setVisible(is_minmax_visible);
}

void MainWindow::initialize_settings_ui_contrast_enabled(
    const QSettings& settings)
{
    const auto variant = settings.value("contrast_enabled");
    if (!variant.canConvert<bool>()) {
        return;
    }

    state_.ac_enabled = variant.toBool();
    ui_components_.ui->acToggle->setChecked(state_.ac_enabled);
    ui_components_.ui->minMaxEditor->setEnabled(state_.ac_enabled);
}

void MainWindow::initialize_settings_ui_link_views_enabled(
    const QSettings& settings)
{
    const auto variant = settings.value("link_views_enabled");
    if (!variant.canConvert<bool>()) {
        return;
    }

    state_.link_views_enabled = variant.toBool();
    ui_components_.ui->linkViewsToggle->setChecked(state_.link_views_enabled);
}

void MainWindow::initialize_settings_ui(QSettings& settings)
{
    settings.beginGroup("UI");

    initialize_settings_ui_list_position(settings);
    initialize_settings_ui_splitter(settings);
    initialize_settings_ui_minmax_compact(settings);
    initialize_settings_ui_colorspace(settings);
    initialize_settings_ui_minmax_visible(settings);
    initialize_settings_ui_contrast_enabled(settings);
    initialize_settings_ui_link_views_enabled(settings);

    settings.endGroup();
}

void MainWindow::initialize_settings()
{
    using BufferExpiration = QPair<QString, QDateTime>;

    // Qt 6: Register the metatype for QSettings serialization
    // Including QDataStream provides the stream operators for Qt types
    qRegisterMetaType<QList<BufferExpiration>>();

    auto settings = QSettings{QSettings::Format::IniFormat,
                              QSettings::Scope::UserScope,
                              "OpenImageDebugger"};

    settings.sync();

    // Load maximum framerate
    render_framerate_ =
        settings.value("Rendering/maximum_framerate", 60).value<double>();
    if (render_framerate_ <= 0.f) {
        render_framerate_ = 1.0;
    }

    // Default save suffix: Image
    settings.beginGroup("Export");
    if (settings.contains("default_export_suffix")) {
        default_export_suffix_ =
            settings.value("default_export_suffix").value<QString>();
    } else {
        default_export_suffix_ = "Image File (*.png)";
    }
    settings.endGroup();

    // Load previous session symbols
    const auto now              = QDateTime::currentDateTime();
    const auto previous_buffers = settings.value("PreviousSession/buffers")
                                      .value<QList<BufferExpiration>>();

    for (const auto& [name, time] : previous_buffers) {
        if (time >= now) {
            buffer_data_.previous_session_buffers.insert(name.toStdString());
        }
    }


    // Load window position/size.
    // Window is loaded with a fixed size and restored in timer.
    // This is needed to give application some time to run event loop
    // and redraw all widgets without changing overall geometry.
    settings.beginGroup("MainWindow");
    setFixedSize(settings.value("size", size()).toSize());
    move(settings.value("pos", pos()).toPoint());
    settings.endGroup();


    // Load UI geometry.
    initialize_settings_ui(settings);


    // Restore possibility to resize application in timer.
    // This is needed to give application some time to run event loop
    // and redraw all widgets without changing overall geometry.
    QTimer::singleShot(100, this, [this]() {
        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    });
}


void MainWindow::setFontIcon(QAbstractButton* ui_element,
                             const wchar_t unicode_id[])
{
    auto icons_font = QFont{};
    icons_font.setFamily("fontello");
    icons_font.setPointSizeF(10.f);

    ui_element->setFont(icons_font);
    ui_element->setText(QString::fromWCharArray(unicode_id));
}


void MainWindow::setVectorIcon(QLabel* ui_element,
                               const QString& icon_file_name,
                               const int width,
                               const int height)
{
    const auto screen_dpi_scale = get_screen_dpi_scale();

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


void MainWindow::initialize_ui_icons() const
{
    if (QFontDatabase::addApplicationFont(":/resources/icons/fontello.ttf") <
        0) {
        qWarning() << "Could not load ionicons font file!";
    }

    setFontIcon(ui_components_.ui->acEdit, L"\ue803");
    setFontIcon(ui_components_.ui->acToggle, L"\ue804");
    setFontIcon(ui_components_.ui->reposition_buffer, L"\ue800");
    setFontIcon(ui_components_.ui->linkViewsToggle, L"\ue805");
    setFontIcon(ui_components_.ui->rotate_90_ccw, L"\ue801");
    setFontIcon(ui_components_.ui->rotate_90_cw, L"\ue802");
    setFontIcon(ui_components_.ui->decrease_float_precision, L"\ue806");
    setFontIcon(ui_components_.ui->increase_float_precision, L"\ue807");
    setFontIcon(ui_components_.ui->go_to_pixel, L"\uf031");

    setFontIcon(ui_components_.ui->ac_reset_min, L"\ue808");
    setFontIcon(ui_components_.ui->ac_reset_max, L"\ue808");

    setVectorIcon(
        ui_components_.ui->label_c1_min,
        QString("label_%1_channel.svg").arg(channel_names_.name_channel_1),
        10,
        10);
    setVectorIcon(
        ui_components_.ui->label_c1_max,
        QString("label_%1_channel.svg").arg(channel_names_.name_channel_1),
        10,
        10);
    setVectorIcon(
        ui_components_.ui->label_c2_min,
        QString("label_%1_channel.svg").arg(channel_names_.name_channel_2),
        10,
        10);
    setVectorIcon(
        ui_components_.ui->label_c2_max,
        QString("label_%1_channel.svg").arg(channel_names_.name_channel_2),
        10,
        10);
    setVectorIcon(
        ui_components_.ui->label_c3_min,
        QString("label_%1_channel.svg").arg(channel_names_.name_channel_3),
        10,
        10);
    setVectorIcon(
        ui_components_.ui->label_c3_max,
        QString("label_%1_channel.svg").arg(channel_names_.name_channel_3),
        10,
        10);
    setVectorIcon(
        ui_components_.ui->label_c4_min,
        QString("label_%1_channel.svg").arg(channel_names_.name_channel_4),
        10,
        10);
    setVectorIcon(
        ui_components_.ui->label_c4_max,
        QString("label_%1_channel.svg").arg(channel_names_.name_channel_4),
        10,
        10);

    setVectorIcon(
        ui_components_.ui->label_minmax, "lower_upper_bound.svg", 8, 35);
}


void MainWindow::initialize_timers()
{
    connect(&ui_components_.settings_persist_timer,
            &QTimer::timeout,
            this,
            &MainWindow::persist_settings);
    ui_components_.settings_persist_timer.setSingleShot(true);

    connect(&ui_components_.update_timer,
            &QTimer::timeout,
            this,
            &MainWindow::loop);
}


void MainWindow::initialize_ui_signals() const
{
    connect(ui_components_.ui->splitter,
            &QSplitter::splitterMoved,
            this,
            &MainWindow::persist_settings_deferred);

    connect(ui_components_.ui->acEdit,
            &QAbstractButton::clicked,
            this,
            &MainWindow::persist_settings_deferred);

    connect(ui_components_.ui->acToggle,
            &QAbstractButton::clicked,
            this,
            &MainWindow::persist_settings_deferred);

    connect(ui_components_.ui->linkViewsToggle,
            &QAbstractButton::clicked,
            this,
            &MainWindow::persist_settings_deferred);
}

void MainWindow::initialize_shortcuts()
{
    auto symbol_list_focus_shortcut =
        std::make_unique<QShortcut>(QKeySequence::fromString("Ctrl+K"), this);
    connect(symbol_list_focus_shortcut.release(),
            &QShortcut::activated,
            ui_components_.ui->symbolList,
            [this]() { ui_components_.ui->symbolList->setFocus(); });

    auto buffer_removal_shortcut = std::make_unique<QShortcut>(
        QKeySequence{Qt::Key_Delete}, ui_components_.ui->imageList);
    connect(buffer_removal_shortcut.release(),
            &QShortcut::activated,
            this,
            &MainWindow::remove_selected_buffer);

    auto go_to_shortcut =
        std::make_unique<QShortcut>(QKeySequence::fromString("Ctrl+L"), this);
    connect(go_to_shortcut.release(),
            &QShortcut::activated,
            this,
            &MainWindow::toggle_go_to_dialog);
    connect(ui_components_.go_to_widget.get(),
            &GoToWidget::go_to_requested,
            this,
            &MainWindow::go_to_pixel);
}


void MainWindow::initialize_networking()
{
    socket_.connectToHost(QString(host_settings_.url.c_str()),
                          host_settings_.port);
    socket_.waitForConnected();
}


void MainWindow::initialize_symbol_completer()
{
    ui_components_.symbol_completer = std::make_unique<SymbolCompleter>(this);

    ui_components_.symbol_completer->setCaseSensitivity(
        Qt::CaseSensitivity::CaseInsensitive);
    ui_components_.symbol_completer->setCompletionMode(
        QCompleter::PopupCompletion);
    ui_components_.symbol_completer->setModelSorting(
        QCompleter::CaseInsensitivelySortedModel);

    ui_components_.ui->symbolList->set_completer(
        *ui_components_.symbol_completer);
    connect(ui_components_.symbol_completer.get(),
            qOverload<const QString&>(&QCompleter::activated),
            this,
            &MainWindow::symbol_completed);
}


void MainWindow::initialize_left_pane() const
{
    connect(ui_components_.ui->imageList,
            &QListWidget::currentItemChanged,
            this,
            &MainWindow::buffer_selected);

    connect(ui_components_.ui->symbolList,
            &QLineEdit::editingFinished,
            this,
            &MainWindow::symbol_selected);

    ui_components_.ui->imageList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui_components_.ui->imageList,
            &QWidget::customContextMenuRequested,
            this,
            &MainWindow::show_context_menu);
}


void MainWindow::initialize_auto_contrast_form() const
{
    // Configure auto contrast inputs
    // Note: Qt's setValidator takes ownership, so we use make_unique and
    // release
    ui_components_.ui->ac_c1_min->setValidator(
        std::make_unique<QDoubleValidator>(ui_components_.ui->ac_c1_min)
            .release());
    ui_components_.ui->ac_c2_min->setValidator(
        std::make_unique<QDoubleValidator>(ui_components_.ui->ac_c2_min)
            .release());
    ui_components_.ui->ac_c3_min->setValidator(
        std::make_unique<QDoubleValidator>(ui_components_.ui->ac_c3_min)
            .release());

    ui_components_.ui->ac_c1_max->setValidator(
        std::make_unique<QDoubleValidator>(ui_components_.ui->ac_c1_max)
            .release());
    ui_components_.ui->ac_c2_max->setValidator(
        std::make_unique<QDoubleValidator>(ui_components_.ui->ac_c2_max)
            .release());
    ui_components_.ui->ac_c3_max->setValidator(
        std::make_unique<QDoubleValidator>(ui_components_.ui->ac_c3_max)
            .release());

    connect(ui_components_.ui->ac_c1_min,
            &QLineEdit::editingFinished,
            this,
            &MainWindow::ac_c1_min_update);
    connect(ui_components_.ui->ac_c1_max,
            &QLineEdit::editingFinished,
            this,
            &MainWindow::ac_c1_max_update);
    connect(ui_components_.ui->ac_c2_min,
            &QLineEdit::editingFinished,
            this,
            &MainWindow::ac_c2_min_update);
    connect(ui_components_.ui->ac_c2_max,
            &QLineEdit::editingFinished,
            this,
            &MainWindow::ac_c2_max_update);
    connect(ui_components_.ui->ac_c3_min,
            &QLineEdit::editingFinished,
            this,
            &MainWindow::ac_c3_min_update);
    connect(ui_components_.ui->ac_c3_max,
            &QLineEdit::editingFinished,
            this,
            &MainWindow::ac_c3_max_update);
    connect(ui_components_.ui->ac_c4_min,
            &QLineEdit::editingFinished,
            this,
            &MainWindow::ac_c4_min_update);
    connect(ui_components_.ui->ac_c4_max,
            &QLineEdit::editingFinished,
            this,
            &MainWindow::ac_c4_max_update);

    connect(ui_components_.ui->ac_reset_min,
            &QAbstractButton::clicked,
            this,
            &MainWindow::ac_min_reset);
    connect(ui_components_.ui->ac_reset_max,
            &QAbstractButton::clicked,
            this,
            &MainWindow::ac_max_reset);
}


void MainWindow::initialize_toolbar() const
{
    connect(ui_components_.ui->acToggle,
            &QToolButton::toggled,
            this,
            &MainWindow::ac_toggle);

    connect(ui_components_.ui->reposition_buffer,
            &QAbstractButton::clicked,
            this,
            &MainWindow::recenter_buffer);

    connect(ui_components_.ui->linkViewsToggle,
            &QAbstractButton::clicked,
            this,
            &MainWindow::link_views_toggle);

    connect(ui_components_.ui->rotate_90_cw,
            &QAbstractButton::clicked,
            this,
            &MainWindow::rotate_90_cw);
    connect(ui_components_.ui->rotate_90_ccw,
            &QAbstractButton::clicked,
            this,
            &MainWindow::rotate_90_ccw);
    connect(ui_components_.ui->increase_float_precision,
            &QAbstractButton::clicked,
            this,
            &MainWindow::increase_float_precision);
    connect(ui_components_.ui->decrease_float_precision,
            &QAbstractButton::clicked,
            this,
            &MainWindow::decrease_float_precision);
    connect(ui_components_.ui->go_to_pixel,
            &QAbstractButton::clicked,
            this,
            &MainWindow::toggle_go_to_dialog);

    ui_components_.ui->increase_float_precision->setEnabled(false);
    ui_components_.ui->decrease_float_precision->setEnabled(false);
}


void MainWindow::initialize_visualization_pane()
{
    ui_components_.ui->bufferPreview->set_main_window(*this);
}


void MainWindow::initialize_status_bar()
{
    ui_components_.status_bar = std::make_unique<QLabel>(this);
    ui_components_.status_bar->setAlignment(Qt::AlignRight);

    statusBar()->addWidget(ui_components_.status_bar.get(), 1);
}


void MainWindow::initialize_go_to_widget()
{
    ui_components_.go_to_widget =
        std::make_unique<GoToWidget>(ui_components_.ui->bufferPreview);
}

} // namespace oid
