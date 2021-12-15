#include "qt_mainwindow.hpp"
#include "ui_qt_mainwindow.h"

#include "qt_specifydimensions.h"
#include "qt_soundgain.hpp"

extern "C" {
#include <86box/86box.h>
#include <86box/config.h>
#include <86box/keyboard.h>
#include <86box/plat.h>
#include <86box/video.h>
#include <86box/vid_ega.h>
#include <86box/version.h>
#include "qt_sdl.h"
};

#include <QDebug>
#include <QGuiApplication>
#include <QWindow>
#include <QTimer>
#include <QThread>
#include <QKeyEvent>
#include <QMessageBox>
#include <QFocusEvent>
#include <QApplication>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QCheckBox>

#include <array>
#include <unordered_map>

#include "qt_settings.hpp"
#include "qt_machinestatus.hpp"
#include "qt_mediamenu.hpp"

//extern void qt_mouse_capture(int);
//extern "C" void qt_blit(int x, int y, int w, int h);

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    Q_INIT_RESOURCE(qt_resources);
    mm = std::make_shared<MediaMenu>(this);
    MediaMenu::ptr = mm;
    status = std::make_unique<MachineStatus>(this);

    ui->setupUi(this);
    statusBar()->setVisible(!hide_status_bar);

    this->setWindowIcon(QIcon(":/settings/win/icons/86Box-yellow.ico"));

    connect(this, &MainWindow::showMessageForNonQtThread, this, &MainWindow::showMessage_, Qt::BlockingQueuedConnection);

    connect(this, &MainWindow::setTitle, this, [this](const QString& title) {
        setWindowTitle(title);
    });
    connect(this, &MainWindow::getTitleForNonQtThread, this, &MainWindow::getTitle_, Qt::BlockingQueuedConnection);

    connect(this, &MainWindow::updateMenuResizeOptions, [this]() {
        ui->actionResizable_window->setEnabled(vid_resize != 2);
        ui->actionResizable_window->setChecked(vid_resize == 1);
        ui->menuWindow_scale_factor->setEnabled(vid_resize == 0);
    });

    connect(this, &MainWindow::updateWindowRememberOption, [this]() {
        ui->actionRemember_size_and_position->setChecked(window_remember);
    });

    emit updateMenuResizeOptions();

    connect(this, &MainWindow::pollMouse, this, [] {
        sdl_mouse_poll();
    });

    connect(this, &MainWindow::setMouseCapture, this, [](bool state) {
        mouse_capture = state ? 1 : 0;
        sdl_mouse_capture(mouse_capture);
    });

    connect(this, &MainWindow::resizeContents, this, [](int w, int h) {
        sdl_resize(w, h);
    });
    connect(this, &MainWindow::setFullscreen, this, [](bool state) {
        startblit();
        video_fullscreen = state ? 1 : 0;
        sdl_set_fs(video_fullscreen);
        endblit();
    });

    connect(ui->menubar, &QMenuBar::triggered, this, [] {
        config_save();
    });

    connect(this, &MainWindow::updateStatusBarPanes, this, [this] {
        refreshMediaMenu();
    });
    connect(this, &MainWindow::updateStatusBarPanes, this, &MainWindow::refreshMediaMenu);
    connect(this, &MainWindow::updateStatusBarTip, status.get(), &MachineStatus::updateTip);
    connect(this, &MainWindow::updateStatusBarActivity, status.get(), &MachineStatus::setActivity);
    connect(this, &MainWindow::updateStatusBarEmpty, status.get(), &MachineStatus::setEmpty);
    connect(this, &MainWindow::statusBarMessage, status.get(), &MachineStatus::message);

    ui->actionKeyboard_requires_capture->setChecked(kbd_req_capture);
    ui->actionRight_CTRL_is_left_ALT->setChecked(rctrl_is_lalt);
    ui->actionResizable_window->setChecked(vid_resize == 1);
    ui->actionRemember_size_and_position->setChecked(window_remember);
    ui->menuWindow_scale_factor->setEnabled(vid_resize == 0);
    ui->actionHiDPI_scaling->setChecked(dpi_scale);
    ui->actionHide_status_bar->setChecked(hide_status_bar);
    ui->actionUpdate_status_bar_icons->setChecked(update_icons);
    switch (vid_api) {
    case 0:
        sdl_inits();
        break;
    case 1:
        sdl_inith();
        break;
    case 2:
        sdl_initho();
        break;
    }
    switch (scale) {
    case 0:
        ui->action0_5x->setChecked(true);
        break;
    case 1:
        ui->action1x->setChecked(true);
        break;
    case 2:
        ui->action1_5x->setChecked(true);
        break;
    case 3:
        ui->action2x->setChecked(true);
        break;
    }
    switch (video_filter_method) {
    case 0:
        ui->actionNearest->setChecked(true);
        break;
    case 1:
        ui->actionLinear->setChecked(true);
        break;
    }
    switch (video_fullscreen_scale) {
    case FULLSCR_SCALE_FULL:
        ui->actionFullScreen_stretch->setChecked(true);
        break;
    case FULLSCR_SCALE_43:
        ui->actionFullScreen_43->setChecked(true);
        break;
    case FULLSCR_SCALE_KEEPRATIO:
        ui->actionFullScreen_keepRatio->setChecked(true);
        break;
    case FULLSCR_SCALE_INT:
        ui->actionFullScreen_int->setChecked(true);
        break;
    }
    switch (video_grayscale) {
    case 0:
        ui->actionRGB_Color->setChecked(true);
        break;
    case 1:
        ui->actionRGB_Grayscale->setChecked(true);
        break;
    case 2:
        ui->actionAmber_monitor->setChecked(true);
        break;
    case 3:
        ui->actionGreen_monitor->setChecked(true);
        break;
    case 4:
        ui->actionWhite_monitor->setChecked(true);
        break;
    }
    switch (video_graytype) {
    case 0:
        ui->actionBT601_NTSC_PAL->setChecked(true);
        break;
    case 1:
        ui->actionBT709_HDTV->setChecked(true);
        break;
    case 2:
        ui->actionAverage->setChecked(true);
        break;
    }
    if (force_43 > 0) {
        ui->actionForce_4_3_display_ratio->setChecked(true);
    }
    if (enable_overscan > 0) {
        ui->actionCGA_PCjr_Tandy_EGA_S_VGA_overscan->setChecked(true);
    }
    if (vid_cga_contrast > 0) {
        ui->actionChange_contrast_for_monochrome_display->setChecked(true);
    }

    sdl_thread = std::thread([this] {
        sdl_main();
        close();
    });
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (confirm_exit)
    {
        QMessageBox questionbox(QMessageBox::Icon::Question, "86Box", "Are you sure you want to exit 86Box?", QMessageBox::Yes | QMessageBox::No, this);
        QCheckBox *chkbox = new QCheckBox("Do not ask me again");
        questionbox.setCheckBox(chkbox);
        chkbox->setChecked(!confirm_exit);
        QObject::connect(chkbox, &QCheckBox::stateChanged, [](int state) {
            confirm_exit = (state == Qt::CheckState::Unchecked);
        });
        questionbox.exec();
        if (questionbox.result() == QMessageBox::No) {
            confirm_exit = true;
            event->ignore();
            return;
        }
        config_save();
    }
    if (window_remember) {
//        window_w = ui->stackedWidget->width();
//        window_h = ui->stackedWidget->height();
//        if (!QApplication::platformName().contains("wayland")) {
//            window_x = this->geometry().x();
//            window_y = this->geometry().y();
//        }
    }
    event->accept();
}

MainWindow::~MainWindow() {
    sdl_quit();
    delete ui;
    sdl_thread.join();
    sdl_close();
}

//void MainWindow::showEvent(QShowEvent *event) {
//    if (window_remember && !QApplication::platformName().contains("wayland")) {
//        setGeometry(window_x, window_y, window_w, window_h);
//    }
//    if (vid_resize == 2) {
//        setFixedSize(fixed_size_x, fixed_size_y + this->menuBar()->height() + this->statusBar()->height());
//        scrnsz_x = fixed_size_x;
//        scrnsz_y = fixed_size_y;
//    }
//    else if (window_remember) {
//        emit resizeContents(window_w, window_h);
//        scrnsz_x = window_w;
//        scrnsz_y = window_h;
//    }
//}

void MainWindow::on_actionKeyboard_requires_capture_triggered() {
    kbd_req_capture ^= 1;
}

void MainWindow::on_actionRight_CTRL_is_left_ALT_triggered() {
    rctrl_is_lalt ^= 1;
}

void MainWindow::on_actionHard_Reset_triggered() {
    pc_reset_hard();
}

void MainWindow::on_actionCtrl_Alt_Del_triggered() {
    pc_send_cad();
}

void MainWindow::on_actionCtrl_Alt_Esc_triggered() {
    pc_send_cae();
}

void MainWindow::on_actionPause_triggered() {
    plat_pause(dopause ^ 1);
}

void MainWindow::on_actionExit_triggered() {
    close();
}

void MainWindow::on_actionSettings_triggered() {
    int currentPause = dopause;
    plat_pause(1);
    Settings settings(this);
    settings.setModal(true);
    settings.setWindowModality(Qt::WindowModal);
    settings.exec();

    switch (settings.result()) {
    case QDialog::Accepted:
        /*
        pc_reset_hard_close();
        settings.save();
        config_changed = 2;
        pc_reset_hard_init();
        */
        settings.save();
        config_changed = 2;
        pc_reset_hard();

        break;
    case QDialog::Rejected:
        break;
    }
    plat_pause(currentPause);
}

void MainWindow::on_actionFullscreen_triggered() {
    if (video_fullscreen > 0) {
        video_fullscreen = 0;
    } else {
        video_fullscreen = 1;
    }

    sdl_set_fs(video_fullscreen);
}

void MainWindow::getTitle_(wchar_t *title)
{
    this->windowTitle().toWCharArray(title);
}

void MainWindow::getTitle(wchar_t *title)
{
    if (QThread::currentThread() == this->thread()) {
        getTitle_(title);
    } else {
        emit getTitleForNonQtThread(title);
    }
}

void MainWindow::refreshMediaMenu() {
    mm->refresh(ui->menuMedia);
    status->refresh(ui->statusbar);
}

void MainWindow::showMessage(const QString& header, const QString& message) {
    if (QThread::currentThread() == this->thread()) {
        showMessage_(header, message);
    } else {
        emit showMessageForNonQtThread(header, message);
    }
}

void MainWindow::showMessage_(const QString &header, const QString &message) {
    QMessageBox box(QMessageBox::Warning, header, message, QMessageBox::NoButton, this);
    box.setTextFormat(Qt::TextFormat::RichText);
    box.exec();
}

//QSize MainWindow::getRenderWidgetSize()
//{
//    return ui->stackedWidget->size();
//}

void MainWindow::on_actionSoftware_Renderer_triggered() {
    startblit();
    sdl_close();
    sdl_inits();
    vid_api = 0;
    endblit();
}

void MainWindow::on_actionHardware_Renderer_OpenGL_triggered() {
    startblit();
    sdl_close();
    sdl_inith();
    vid_api = 1;
    endblit();
}

void MainWindow::on_actionHardware_Renderer_OpenGL_ES_triggered() {
    startblit();
    sdl_close();
    sdl_initho();
    vid_api = 2;
    endblit();
}

void MainWindow::on_actionResizable_window_triggered(bool checked) {
    if (checked) {
        vid_resize = 1;
        setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    } else {
        vid_resize = 0;
    }
    ui->menuWindow_scale_factor->setEnabled(! checked);
    emit resizeContents(scrnsz_x, scrnsz_y);
}

static void
video_toggle_option(QAction* action, int *val)
{
    startblit();
    *val ^= 1;
    video_copy = (video_grayscale || invert_display) ? video_transform_copy : memcpy;
    action->setChecked(*val > 0 ? true : false);
    endblit();
    config_save();
    device_force_redraw();
}

void MainWindow::on_actionInverted_VGA_monitor_triggered() {
    video_toggle_option(ui->actionInverted_VGA_monitor, &invert_display);
}

static void update_scaled_checkboxes(Ui::MainWindow* ui, QAction* selected) {
    ui->action0_5x->setChecked(ui->action0_5x == selected);
    ui->action1x->setChecked(ui->action1x == selected);
    ui->action1_5x->setChecked(ui->action1_5x == selected);
    ui->action2x->setChecked(ui->action2x == selected);

    reset_screen_size();
    device_force_redraw();
    video_force_resize_set(1);
    doresize = 1;
    config_save();
}

void MainWindow::on_action0_5x_triggered() {
    scale = 0;
    update_scaled_checkboxes(ui, ui->action0_5x);
}

void MainWindow::on_action1x_triggered() {
    scale = 1;
    update_scaled_checkboxes(ui, ui->action1x);
}

void MainWindow::on_action1_5x_triggered() {
    scale = 2;
    update_scaled_checkboxes(ui, ui->action1_5x);
}

void MainWindow::on_action2x_triggered() {
    scale = 3;
    update_scaled_checkboxes(ui, ui->action2x);
}

void MainWindow::on_actionNearest_triggered() {
    video_filter_method = 0;
    ui->actionLinear->setChecked(false);
}

void MainWindow::on_actionLinear_triggered() {
    video_filter_method = 1;
    ui->actionNearest->setChecked(false);
}

static void update_fullscreen_scale_checkboxes(Ui::MainWindow* ui, QAction* selected) {
    ui->actionFullScreen_stretch->setChecked(ui->actionFullScreen_stretch == selected);
    ui->actionFullScreen_43->setChecked(ui->actionFullScreen_43 == selected);
    ui->actionFullScreen_keepRatio->setChecked(ui->actionFullScreen_keepRatio == selected);
    ui->actionFullScreen_int->setChecked(ui->actionFullScreen_int == selected);

    if (video_fullscreen > 0) {
        sdl_resize(0, 0);
    }

    device_force_redraw();
    config_save();
}

void MainWindow::on_actionFullScreen_stretch_triggered() {
    video_fullscreen_scale = FULLSCR_SCALE_FULL;
    update_fullscreen_scale_checkboxes(ui, ui->actionFullScreen_stretch);
}

void MainWindow::on_actionFullScreen_43_triggered() {
    video_fullscreen_scale = FULLSCR_SCALE_43;
    update_fullscreen_scale_checkboxes(ui, ui->actionFullScreen_43);
}

void MainWindow::on_actionFullScreen_keepRatio_triggered() {
    video_fullscreen_scale = FULLSCR_SCALE_KEEPRATIO;
    update_fullscreen_scale_checkboxes(ui, ui->actionFullScreen_keepRatio);
}

void MainWindow::on_actionFullScreen_int_triggered() {
    video_fullscreen_scale = FULLSCR_SCALE_INT;
    update_fullscreen_scale_checkboxes(ui, ui->actionFullScreen_int);
}

static void update_greyscale_checkboxes(Ui::MainWindow* ui, QAction* selected, int value) {
    ui->actionRGB_Color->setChecked(ui->actionRGB_Color == selected);
    ui->actionRGB_Grayscale->setChecked(ui->actionRGB_Grayscale == selected);
    ui->actionAmber_monitor->setChecked(ui->actionAmber_monitor == selected);
    ui->actionGreen_monitor->setChecked(ui->actionGreen_monitor == selected);
    ui->actionWhite_monitor->setChecked(ui->actionWhite_monitor == selected);

    startblit();
    video_grayscale = value;
    video_copy = (video_grayscale || invert_display) ? video_transform_copy : memcpy;
    endblit();
    device_force_redraw();
    config_save();
}

void MainWindow::on_actionRGB_Color_triggered() {
    update_greyscale_checkboxes(ui, ui->actionRGB_Color, 0);
}

void MainWindow::on_actionRGB_Grayscale_triggered() {
    update_greyscale_checkboxes(ui, ui->actionRGB_Grayscale, 1);
}

void MainWindow::on_actionAmber_monitor_triggered() {
    update_greyscale_checkboxes(ui, ui->actionAmber_monitor, 2);
}

void MainWindow::on_actionGreen_monitor_triggered() {
    update_greyscale_checkboxes(ui, ui->actionGreen_monitor, 3);
}

void MainWindow::on_actionWhite_monitor_triggered() {
    update_greyscale_checkboxes(ui, ui->actionWhite_monitor, 4);
}

static void update_greyscale_type_checkboxes(Ui::MainWindow* ui, QAction* selected, int value) {
    ui->actionBT601_NTSC_PAL->setChecked(ui->actionBT601_NTSC_PAL == selected);
    ui->actionBT709_HDTV->setChecked(ui->actionBT709_HDTV == selected);
    ui->actionAverage->setChecked(ui->actionAverage == selected);

    video_graytype = value;
    device_force_redraw();
    config_save();
}

void MainWindow::on_actionBT601_NTSC_PAL_triggered() {
    update_greyscale_type_checkboxes(ui, ui->actionBT601_NTSC_PAL, 0);
}

void MainWindow::on_actionBT709_HDTV_triggered() {
    update_greyscale_type_checkboxes(ui, ui->actionBT709_HDTV, 1);
}

void MainWindow::on_actionAverage_triggered() {
    update_greyscale_type_checkboxes(ui, ui->actionAverage, 2);
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QApplication::aboutQt();
}

void MainWindow::on_actionAbout_86Box_triggered()
{
    QMessageBox msgBox;
    msgBox.setTextFormat(Qt::RichText);
    QString githash;
#ifdef EMU_GIT_HASH
    githash = QString(" [%1]").arg(EMU_GIT_HASH);
#endif
    msgBox.setText(QString("<b>86Box v%1%2</b>").arg(EMU_VERSION_FULL, githash));
    msgBox.setInformativeText(R"(
An emulator of old computers

Authors: Sarah Walker, Miran Grca, Fred N. van Kempen (waltje), SA1988, Tiseno100, reenigne, leilei, JohnElliott, greatpsycho, and others.

Released under the GNU General Public License version 2 or later. See LICENSE for more information.
)");
    msgBox.setWindowTitle("About 86Box");
    msgBox.addButton("OK", QMessageBox::ButtonRole::AcceptRole);
    auto webSiteButton = msgBox.addButton("86box.net", QMessageBox::ButtonRole::HelpRole);
    webSiteButton->connect(webSiteButton, &QPushButton::released, []()
    {
        QDesktopServices::openUrl(QUrl("https://86box.net/"));
    });
    msgBox.setIconPixmap(QIcon(":/settings/win/icons/86Box-yellow.ico").pixmap(32, 32));
    msgBox.exec();
}

void MainWindow::on_actionDocumentation_triggered()
{
     QDesktopServices::openUrl(QUrl("https://86box.readthedocs.io"));
}

void MainWindow::on_actionCGA_PCjr_Tandy_EGA_S_VGA_overscan_triggered() {
    update_overscan = 1;
    video_toggle_option(ui->actionCGA_PCjr_Tandy_EGA_S_VGA_overscan, &enable_overscan);
}

void MainWindow::on_actionChange_contrast_for_monochrome_display_triggered() {
    vid_cga_contrast ^= 1;
    cgapal_rebuild();
    config_save();
}

void MainWindow::on_actionForce_4_3_display_ratio_triggered() {
    video_toggle_option(ui->actionForce_4_3_display_ratio, &force_43);
    video_force_resize_set(1);
}

void MainWindow::on_actionRemember_size_and_position_triggered()
{
    window_remember ^= 1;
//    window_w = ui->stackedWidget->width();
//    window_h = ui->stackedWidget->height();
//    if (!QApplication::platformName().contains("wayland")) {
//        window_x = geometry().x();
//        window_y = geometry().y();
//    }
    ui->actionRemember_size_and_position->setChecked(window_remember);
}

void MainWindow::on_actionSpecify_dimensions_triggered()
{
    SpecifyDimensions dialog(this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.exec();
}

void MainWindow::on_actionHiDPI_scaling_triggered()
{
    dpi_scale ^= 1;
    ui->actionHiDPI_scaling->setChecked(dpi_scale);
    emit resizeContents(scrnsz_x, scrnsz_y);
}

void MainWindow::on_actionHide_status_bar_triggered()
{
    hide_status_bar ^= 1;
    ui->actionHide_status_bar->setChecked(hide_status_bar);
    statusBar()->setVisible(!hide_status_bar);
    if (vid_resize >= 2) setFixedSize(fixed_size_x, fixed_size_y + menuBar()->height() + (hide_status_bar ? 0 : statusBar()->height()));
    else {
        int vid_resize_orig = vid_resize;
        vid_resize = 0;
        emit resizeContents(scrnsz_x, scrnsz_y);
        vid_resize = vid_resize_orig;
    }
}

void MainWindow::on_actionUpdate_status_bar_icons_triggered()
{
    update_icons ^= 1;
    ui->actionUpdate_status_bar_icons->setChecked(update_icons);
}

void MainWindow::on_actionTake_screenshot_triggered()
{
    startblit();
    screenshots++;
    endblit();
    device_force_redraw();
}

void MainWindow::on_actionSound_gain_triggered()
{
    SoundGain gain(this);
    gain.exec();
}
