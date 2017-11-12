/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
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

#include <cmath>

#include <QDateTime>
#include <QDebug>
#include <QFontDatabase>
#include <QSettings>
#include <QShortcut>

#include "main_window.h"

#include "ui_main_window.h"


void MainWindow::initialize_settings()
{
    using BufferExpiration = QPair<QString, QDateTime>;

    qRegisterMetaTypeStreamOperators<QList<BufferExpiration>>(
        "QList<QPair<QString, QDateTime>>");

    QSettings settings(QSettings::Format::IniFormat,
                       QSettings::Scope::UserScope,
                       "gdbimagewatch");

    settings.sync();

    // Load maximum framerate
    render_framerate_ =
        settings.value("Rendering/maximum_framerate", 60).value<double>();
    if (render_framerate_ <= 0.f) {
        render_framerate_ = 1.0;
    }

    // Load previous session symbols
    QDateTime now = QDateTime::currentDateTime();
    QList<BufferExpiration> previous_buffers =
        settings.value("PreviousSession/buffers")
            .value<QList<BufferExpiration>>();

    for (const auto& previous_buffer : previous_buffers) {
        if (previous_buffer.second >= now) {
            previous_session_buffers_.insert(
                previous_buffer.first.toStdString());
        }
    }

    // Load window position/size
    settings.beginGroup("MainWindow");
    resize(settings.value("size", size()).toSize());
    move(settings.value("pos", pos()).toPoint());
    settings.endGroup();
}


void MainWindow::initialize_ui_icons()
{
#define SET_FONT_ICON(ui_element, unicode_id) \
    ui_element->setFont(icons_font);          \
    ui_element->setText(unicode_id);

#define SET_VECTOR_ICON(ui_element, icon_file_name, width, height) \
    ui_element->setScaledContents(true);                           \
    ui_element->setMinimumWidth(std::round(width));                \
    ui_element->setMaximumWidth(std::round(width));                \
    ui_element->setMinimumHeight(std::round(height));              \
    ui_element->setMaximumHeight(std::round(height));              \
    ui_element->setPixmap(                                         \
        QIcon(":/resources/icons/" icon_file_name)                 \
            .pixmap(QSize(std::round(width* screen_dpi_scale),     \
                          std::round(height* screen_dpi_scale)))); \
    ui_element->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    if (QFontDatabase::addApplicationFont(":/resources/icons/fontello.ttf") <
        0) {
        qWarning() << "Could not load ionicons font file!";
    }

    qreal screen_dpi_scale = get_screen_dpi_scale();
    QFont icons_font;
    icons_font.setFamily("fontello");
    icons_font.setPointSizeF(10.f);

    SET_FONT_ICON(ui_->acEdit, "\ue803");
    SET_FONT_ICON(ui_->acToggle, "\ue804");
    SET_FONT_ICON(ui_->reposition_buffer, "\ue800");
    SET_FONT_ICON(ui_->linkViewsToggle, "\ue805");
    SET_FONT_ICON(ui_->rotate_90_cw, "\ue801");
    SET_FONT_ICON(ui_->rotate_90_ccw, "\ue802");
    SET_FONT_ICON(ui_->ac_reset_min, "\ue808");
    SET_FONT_ICON(ui_->ac_reset_max, "\ue808");

    SET_FONT_ICON(ui_->label_min, "\ue806");
    SET_FONT_ICON(ui_->label_max, "\ue807");

    SET_VECTOR_ICON(ui_->label_red_min, "label_red_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_red_max, "label_red_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_green_min, "label_green_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_green_max, "label_green_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_blue_min, "label_blue_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_blue_max, "label_blue_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_alpha_min, "label_alpha_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_alpha_max, "label_alpha_channel.svg", 10, 10);

    QLabel* label_min_max = new QLabel(ui_->minMaxEditor);
    SET_VECTOR_ICON(label_min_max, "lower_upper_bound.svg", 12.f, 52.f);
    ui_->gridLayout->addWidget(label_min_max, 0, 0, 2, 1);
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


void MainWindow::initialize_shortcuts()
{
    QShortcut* symbol_list_focus_shortcut_ =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this);
    connect(symbol_list_focus_shortcut_,
            SIGNAL(activated()),
            ui_->symbolList,
            SLOT(setFocus()));

    QShortcut* buffer_removal_shortcut_ =
        new QShortcut(QKeySequence(Qt::Key_Delete), ui_->imageList);
    connect(buffer_removal_shortcut_,
            SIGNAL(activated()),
            this,
            SLOT(remove_selected_buffer()));

    QShortcut* go_to_shortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
    connect(go_to_shortcut,
            SIGNAL(activated()),
            this,
            SLOT(toggle_go_to_dialog()));
    connect(go_to_widget_,
            SIGNAL(go_to_requested(int, int)),
            this,
            SLOT(go_to_pixel(int, int)));
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


void MainWindow::initialize_left_pane()
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


void MainWindow::initialize_auto_contrast_form()
{
    // Configure auto contrast inputs
    ui_->ac_red_min->setValidator(new QDoubleValidator(ui_->ac_red_min));
    ui_->ac_green_min->setValidator(new QDoubleValidator(ui_->ac_green_min));
    ui_->ac_blue_min->setValidator(new QDoubleValidator(ui_->ac_blue_min));

    ui_->ac_red_max->setValidator(new QDoubleValidator(ui_->ac_red_max));
    ui_->ac_green_max->setValidator(new QDoubleValidator(ui_->ac_green_max));
    ui_->ac_blue_max->setValidator(new QDoubleValidator(ui_->ac_blue_max));

    connect(ui_->ac_red_min,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_red_min_update()));
    connect(ui_->ac_red_max,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_red_max_update()));
    connect(ui_->ac_green_min,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_green_min_update()));
    connect(ui_->ac_green_max,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_green_max_update()));
    connect(ui_->ac_blue_min,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_blue_min_update()));
    connect(ui_->ac_blue_max,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_blue_max_update()));
    connect(ui_->ac_alpha_min,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_alpha_min_update()));
    connect(ui_->ac_alpha_max,
            SIGNAL(editingFinished()),
            this,
            SLOT(ac_alpha_max_update()));

    connect(ui_->ac_reset_min, SIGNAL(clicked()), this, SLOT(ac_min_reset()));
    connect(ui_->ac_reset_max, SIGNAL(clicked()), this, SLOT(ac_max_reset()));
}


void MainWindow::initialize_toolbar()
{
    connect(ui_->acToggle, SIGNAL(clicked()), this, SLOT(ac_toggle()));

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
