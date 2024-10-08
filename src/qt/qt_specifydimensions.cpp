#include "qt_specifydimensions.h"
#include "ui_qt_specifydimensions.h"

#include "qt_mainwindow.hpp"

#include <QStatusBar>
#include <QMenuBar>

extern "C"
{
#include <86box/86box.h>
#include <86box/plat.h>
#include <86box/ui.h>
#include <86box/video.h>
}

extern MainWindow* main_window;

SpecifyDimensions::SpecifyDimensions(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SpecifyDimensions)
{
    ui->setupUi(this);
    ui->checkBox->setChecked(vid_resize == 2);
    ui->spinBoxWidth->setRange(16, 2048);
    // ui->spinBoxWidth->setValue(main_window->getRenderWidgetSize().width());
    ui->spinBoxHeight->setRange(16, 2048);
    // ui->spinBoxHeight->setValue(main_window->getRenderWidgetSize().height());
}

SpecifyDimensions::~SpecifyDimensions()
{
    delete ui;
}

void SpecifyDimensions::on_SpecifyDimensions_accepted()
{
    if (ui->checkBox->isChecked())
    {
        vid_resize = 2;
        window_remember = 0;
        fixed_size_x = ui->spinBoxWidth->value();
        fixed_size_y = ui->spinBoxHeight->value();
        main_window->setFixedSize(ui->spinBoxWidth->value(), ui->spinBoxHeight->value() + (hide_status_bar ? main_window->statusBar()->height() : 0) + main_window->menuBar()->height());
        emit main_window->updateMenuResizeOptions();
    }
    else
    {
        vid_resize = 0;
        window_remember = 1;
        window_w = ui->spinBoxWidth->value();
        window_h = ui->spinBoxHeight->value();
        main_window->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        emit main_window->resizeContents(ui->spinBoxWidth->value(), ui->spinBoxHeight->value());
        vid_resize = 1;
        emit main_window->updateMenuResizeOptions();
    }
    emit main_window->updateWindowRememberOption();
}

