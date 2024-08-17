/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2024 OpenImageDebugger contributors
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
#include <iostream>

#include <QDateTime>
#include <QDebug>
#include <QFontDatabase>
#include <QHostAddress>
#include <QSettings>
#include <QShortcut>


#include "ui_main_window.h"

namespace oid
{

void MainWindow::initialize_settings_ui_list_position(
    const QSettings& settings) const
{
    const QVariant variant = settings.value("list_position");
    if (!variant.canConvert<QString>()) {
        return;
    }

    const QString position_str = variant.toString();

    if (position_str == "top" || position_str == "bottom") {
        ui_->splitter->setOrientation(Qt::Vertical);
    }

    if (position_str == "right" || position_str == "bottom") {
        ui_->splitter->insertWidget(-1, ui_->frame_list);
    }

    ui_->splitter->repaint();
}

void MainWindow::initialize_settings_ui_splitter(
    const QSettings& settings) const
{
    const QVariant variant = settings.value("splitter");
    if (!variant.canConvert<QVariantList>()) {
        return;
    }

    QVariantList listVariants = variant.toList();

    QList<int> listSizes;
    for (const QVariant& size : listVariants) {
        listSizes.append(size.toInt());
    }

    ui_->splitter->setSizes(listSizes);
}

void MainWindow::initialize_settings_ui_minmax_compact(
    const QSettings& settings) const
{
    const bool is_minmax_compact = [&]() -> auto {
        const QVariant variant = settings.value("minmax_compact");
        if (!variant.canConvert<bool>()) {
            return false;
        }

        return variant.toBool();
    }();

    if (!is_minmax_compact) {
        return;
    }

    const bool is_minmax_visible = [&]() -> auto {
        const QVariant variant = settings.value("minmax_visible");
        if (!variant.canConvert<bool>()) {
            return true;
        }

        return variant.toBool();
    }();

    if (is_minmax_visible) {
        ui_->gridLayout_toolbar->addWidget(ui_->linkViewsToggle, 0, 0);
        ui_->gridLayout_toolbar->addWidget(ui_->reposition_buffer, 0, 1);
        ui_->gridLayout_toolbar->addWidget(ui_->go_to_pixel, 0, 2);

        ui_->gridLayout_toolbar->addWidget(ui_->rotate_90_ccw, 1, 0);
        ui_->gridLayout_toolbar->addWidget(ui_->rotate_90_cw, 1, 1);
        ui_->gridLayout_toolbar->addWidget(ui_->acToggle, 1, 2);
        ui_->gridLayout_toolbar->addWidget(ui_->shift_precision_left, 1, 3);
        ui_->gridLayout_toolbar->addWidget(ui_->shift_precision_right, 1, 4);
    }

    ui_->horizontalLayout_container_toolbar->addWidget(ui_->minMaxEditor, 2);
    ui_->horizontalLayout_container_toolbar->setStretch(0, 0);
    ui_->horizontalLayout_container_toolbar->setStretch(1, 1);
    ui_->horizontalLayout_container_toolbar->setStretch(2, 0);

    ui_->acEdit->hide();
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
    const QVariant variant = settings.value("colorspace");
    if (!variant.canConvert<QString>()) {
        return;
    }

    const QString colorspace_str = variant.toString();

    if (colorspace_str.size() > 0) {
        name_channel_1_ =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(0));
    }
    if (colorspace_str.size() > 1) {
        name_channel_2_ =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(1));
    }
    if (colorspace_str.size() > 2) {
        name_channel_3_ =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(2));
    }
    if (colorspace_str.size() > 3) {
        name_channel_4_ =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(3));
    }
}

void MainWindow::initialize_settings_ui_minmax_visible(
    const QSettings& settings) const
{
    const QVariant variant = settings.value("minmax_visible");
    if (!variant.canConvert<bool>()) {
        return;
    }

    const bool is_minmax_visible = variant.toBool();
    ui_->acEdit->setChecked(is_minmax_visible);
    ui_->minMaxEditor->setVisible(is_minmax_visible);
}

void MainWindow::initialize_settings_ui_contrast_enabled(
    const QSettings& settings)
{
    const QVariant variant = settings.value("contrast_enabled");
    if (!variant.canConvert<bool>()) {
        return;
    }

    ac_enabled_ = variant.toBool();
    ui_->acToggle->setChecked(ac_enabled_);
    ui_->minMaxEditor->setEnabled(ac_enabled_);
}

void MainWindow::initialize_settings_ui_link_views_enabled(
    const QSettings& settings)
{
    const QVariant variant = settings.value("link_views_enabled");
    if (!variant.canConvert<bool>()) {
        return;
    }

    link_views_enabled_ = variant.toBool();
    ui_->linkViewsToggle->setChecked(link_views_enabled_);
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

    qRegisterMetaTypeStreamOperators<QList<BufferExpiration>>(
        "QList<QPair<QString, QDateTime>>");

    QSettings settings(QSettings::Format::IniFormat,
                       QSettings::Scope::UserScope,
                       "OpenImageDebugger");

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
    const QDateTime now   = QDateTime::currentDateTime();
    auto previous_buffers = settings.value("PreviousSession/buffers")
                                .value<QList<BufferExpiration>>();

    for (const auto& previous_buffer : previous_buffers) {
        if (previous_buffer.second >= now) {
            previous_session_buffers_.insert(
                previous_buffer.first.toStdString());
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
    QFont icons_font;
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
    const qreal screen_dpi_scale = get_screen_dpi_scale();

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

    setFontIcon(ui_->acEdit, L"\ue803");
    setFontIcon(ui_->acToggle, L"\ue804");
    setFontIcon(ui_->reposition_buffer, L"\ue800");
    setFontIcon(ui_->linkViewsToggle, L"\ue805");
    setFontIcon(ui_->rotate_90_cw, L"\ue801");
    setFontIcon(ui_->rotate_90_ccw, L"\ue802");
    setFontIcon(ui_->shift_precision_left, L"\ue806");
    setFontIcon(ui_->shift_precision_right, L"\ue807");
    setFontIcon(ui_->go_to_pixel, L"\uf031");

    setFontIcon(ui_->ac_reset_min, L"\ue808");
    setFontIcon(ui_->ac_reset_max, L"\ue808");

    setVectorIcon(ui_->label_c1_min,
                  QString("label_%1_channel.svg").arg(name_channel_1_),
                  10,
                  10);
    setVectorIcon(ui_->label_c1_max,
                  QString("label_%1_channel.svg").arg(name_channel_1_),
                  10,
                  10);
    setVectorIcon(ui_->label_c2_min,
                  QString("label_%1_channel.svg").arg(name_channel_2_),
                  10,
                  10);
    setVectorIcon(ui_->label_c2_max,
                  QString("label_%1_channel.svg").arg(name_channel_2_),
                  10,
                  10);
    setVectorIcon(ui_->label_c3_min,
                  QString("label_%1_channel.svg").arg(name_channel_3_),
                  10,
                  10);
    setVectorIcon(ui_->label_c3_max,
                  QString("label_%1_channel.svg").arg(name_channel_3_),
                  10,
                  10);
    setVectorIcon(ui_->label_c4_min,
                  QString("label_%1_channel.svg").arg(name_channel_4_),
                  10,
                  10);
    setVectorIcon(ui_->label_c4_max,
                  QString("label_%1_channel.svg").arg(name_channel_4_),
                  10,
                  10);

    setVectorIcon(ui_->label_minmax, "lower_upper_bound.svg", 8, 35);
}


void MainWindow::initialize_timers()
{
    connect(&settings_persist_timer_,
            SIGNAL(timeout()),
            this,
            SLOT(persist_settings()));
    settings_persist_timer_.setSingleShot(true);

    connect(&update_timer_, SIGNAL(timeout()), this, SLOT(loop()));
}


void MainWindow::initialize_ui_signals()
{
    connect(ui_->splitter,
            &QSplitter::splitterMoved,
            this,
            &MainWindow::persist_settings_deferred);

    connect(ui_->acEdit,
            &QAbstractButton::clicked,
            this,
            &MainWindow::persist_settings_deferred);

    connect(ui_->acToggle,
            &QAbstractButton::clicked,
            this,
            &MainWindow::persist_settings_deferred);

    connect(ui_->linkViewsToggle,
            &QAbstractButton::clicked,
            this,
            &MainWindow::persist_settings_deferred);
}

void MainWindow::initialize_shortcuts()
{
    const QShortcut* symbol_list_focus_shortcut_ =
        new QShortcut(QKeySequence::fromString("Ctrl+K"), this);
    connect(symbol_list_focus_shortcut_,
            SIGNAL(activated()),
            ui_->symbolList,
            SLOT(setFocus()));

    const QShortcut* buffer_removal_shortcut_ =
        new QShortcut(QKeySequence(Qt::Key_Delete), ui_->imageList);
    connect(buffer_removal_shortcut_,
            SIGNAL(activated()),
            this,
            SLOT(remove_selected_buffer()));

    const QShortcut* go_to_shortcut =
        new QShortcut(QKeySequence::fromString("Ctrl+L"), this);
    connect(
        go_to_shortcut, SIGNAL(activated()), this, SLOT(toggle_go_to_dialog()));
    connect(go_to_widget_,
            SIGNAL(go_to_requested(float, float)),
            this,
            SLOT(go_to_pixel(float, float)));
}


void MainWindow::initialize_networking()
{
    socket_.connectToHost(QString(host_settings_.url.c_str()),
                          host_settings_.port);
    socket_.waitForConnected();
}


void MainWindow::initialize_symbol_completer()
{
    symbol_completer_ = new SymbolCompleter(this);

    symbol_completer_->setCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
    symbol_completer_->setCompletionMode(QCompleter::PopupCompletion);
    symbol_completer_->setModelSorting(
        QCompleter::CaseInsensitivelySortedModel);

    ui_->symbolList->set_completer(symbol_completer_);
    connect(ui_->symbolList->completer(),
            SIGNAL(activated(QString)),
            this,
            SLOT(symbol_completed(QString)));
}


void MainWindow::initialize_left_pane() const
{
    connect(ui_->imageList,
            SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
            this,
            SLOT(buffer_selected(QListWidgetItem*)));

    connect(ui_->symbolList,
            SIGNAL(editingFinished()),
            this,
            SLOT(symbol_selected()));

    ui_->imageList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui_->imageList,
            SIGNAL(customContextMenuRequested(const QPoint&)),
            this,
            SLOT(show_context_menu(const QPoint&)));
}


void MainWindow::initialize_auto_contrast_form() const
{
    // Configure auto contrast inputs
    ui_->ac_c1_min->setValidator(new QDoubleValidator(ui_->ac_c1_min));
    ui_->ac_c2_min->setValidator(new QDoubleValidator(ui_->ac_c2_min));
    ui_->ac_c3_min->setValidator(new QDoubleValidator(ui_->ac_c3_min));

    ui_->ac_c1_max->setValidator(new QDoubleValidator(ui_->ac_c1_max));
    ui_->ac_c2_max->setValidator(new QDoubleValidator(ui_->ac_c2_max));
    ui_->ac_c3_max->setValidator(new QDoubleValidator(ui_->ac_c3_max));

    connect(ui_->ac_c1_min,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_c1_min_update()));
    connect(ui_->ac_c1_max,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_c1_max_update()));
    connect(ui_->ac_c2_min,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_c2_min_update()));
    connect(ui_->ac_c2_max,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_c2_max_update()));
    connect(ui_->ac_c3_min,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_c3_min_update()));
    connect(ui_->ac_c3_max,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_c3_max_update()));
    connect(ui_->ac_c4_min,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_c4_min_update()));
    connect(ui_->ac_c4_max,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_c4_max_update()));

    connect(ui_->ac_reset_min, SIGNAL(clicked()), this, SLOT(ac_min_reset()));
    connect(ui_->ac_reset_max, SIGNAL(clicked()), this, SLOT(ac_max_reset()));
}


void MainWindow::initialize_toolbar() const
{
    connect(ui_->acToggle, &QToolButton::toggled, this, &MainWindow::ac_toggle);

    connect(ui_->reposition_buffer,
            SIGNAL(clicked()),
            this,
            SLOT(recenter_buffer()));

    connect(ui_->linkViewsToggle,
            SIGNAL(clicked()),
            this,
            SLOT(link_views_toggle()));

    connect(ui_->rotate_90_cw, SIGNAL(clicked()), this, SLOT(rotate_90_cw()));
    connect(ui_->rotate_90_ccw, SIGNAL(clicked()), this, SLOT(rotate_90_ccw()));
    connect(ui_->shift_precision_right,
            SIGNAL(clicked()),
            this,
            SLOT(shift_precision_right()));
    connect(ui_->shift_precision_left,
            SIGNAL(clicked()),
            this,
            SLOT(shift_precision_left()));
    connect(
        ui_->go_to_pixel, SIGNAL(clicked()), this, SLOT(toggle_go_to_dialog()));

    ui_->shift_precision_right->setEnabled(false);
    ui_->shift_precision_left->setEnabled(false);
}


void MainWindow::initialize_visualization_pane()
{
    ui_->bufferPreview->set_main_window(this);
}


void MainWindow::initialize_status_bar()
{
    status_bar_ = new QLabel(this);
    status_bar_->setAlignment(Qt::AlignRight);

    statusBar()->addWidget(status_bar_, 1);
}


void MainWindow::initialize_go_to_widget()
{
    go_to_widget_ = new GoToWidget(ui_->bufferPreview);
}

} // namespace oid
