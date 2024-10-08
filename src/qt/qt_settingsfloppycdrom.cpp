#include "qt_settingsfloppycdrom.hpp"
#include "ui_qt_settingsfloppycdrom.h"

extern "C" {
#include <86box/timer.h>
#include <86box/fdd.h>
#include <86box/cdrom.h>
}

#include <QStandardItemModel>

#include "qt_models_common.hpp"
#include "qt_harddrive_common.hpp"

static void setFloppyType(QAbstractItemModel* model, const QModelIndex& idx, int type) {
    QIcon icon;
    if (type == 0) {
        icon = QIcon(":/settings/win/icons/floppy_disabled.ico");
    } else if (type >= 1 && type <= 6) {
        icon = QIcon(":/settings/win/icons/floppy_525.ico");
    } else {
        icon = QIcon(":/settings/win/icons/floppy_35.ico");
    }

    model->setData(idx, fdd_getname(type));
    model->setData(idx, type, Qt::UserRole);
    model->setData(idx, icon, Qt::DecorationRole);
}

static void setCDROMBus(QAbstractItemModel* model, const QModelIndex& idx, uint8_t bus, uint8_t channel) {
    QIcon icon;
    switch (bus) {
    case CDROM_BUS_DISABLED:
        icon = QIcon(":/settings/win/icons/cdrom_disabled.ico");
        break;
    case CDROM_BUS_ATAPI:
    case CDROM_BUS_SCSI:
        icon = QIcon(":/settings/win/icons/cdrom.ico");
        break;
    }

    auto i = idx.siblingAtColumn(0);
    model->setData(i, Harddrives::BusChannelName(bus, channel));
    model->setData(i, bus, Qt::UserRole);
    model->setData(i, channel, Qt::UserRole + 1);
    model->setData(i, icon, Qt::DecorationRole);
}

static void setCDROMSpeed(QAbstractItemModel* model, const QModelIndex& idx, uint8_t speed) {
    auto i = idx.siblingAtColumn(1);
    model->setData(i, QString("%1x").arg(speed));
    model->setData(i, speed, Qt::UserRole);
}

SettingsFloppyCDROM::SettingsFloppyCDROM(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SettingsFloppyCDROM)
{
    ui->setupUi(this);

    auto* model = ui->comboBoxFloppyType->model();
    int i = 0;
    while (true) {
        QString name = fdd_getname(i);
        if (name.isEmpty()) {
            break;
        }

        Models::AddEntry(model, name, i);
        ++i;
    }

    model = new QStandardItemModel(0, 3, this);
    ui->tableViewFloppy->setModel(model);
    model->setHeaderData(0, Qt::Horizontal, "Type");
    model->setHeaderData(1, Qt::Horizontal, "Turbo");
    model->setHeaderData(2, Qt::Horizontal, "Check BPB");

    model->insertRows(0, FDD_NUM);
    /* Floppy drives category */
    for (int i = 0; i < FDD_NUM; i++) {
        auto idx = model->index(i, 0);
        int type = fdd_get_type(i);
        setFloppyType(model, idx, type);
        model->setData(idx.siblingAtColumn(1), fdd_get_turbo(i) > 0 ? "On" : "Off");
        model->setData(idx.siblingAtColumn(2), fdd_get_check_bpb(i) > 0 ? "On" : "Off");
    }

    ui->tableViewFloppy->resizeColumnsToContents();
    ui->tableViewFloppy->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    connect(ui->tableViewFloppy->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &SettingsFloppyCDROM::onFloppyRowChanged);
    ui->tableViewFloppy->setCurrentIndex(model->index(0, 0));


    Harddrives::populateRemovableBuses(ui->comboBoxBus->model());
    model = ui->comboBoxSpeed->model();
    for (int i = 0; i <= 72; i++) {
        Models::AddEntry(model, QString("%1x").arg(i), i);
    }

    model = new QStandardItemModel(0, 2, this);
    ui->tableViewCDROM->setModel(model);
    model->setHeaderData(0, Qt::Horizontal, "Bus");
    model->setHeaderData(1, Qt::Horizontal, "Speed");
    model->insertRows(0, CDROM_NUM);
    for (int i = 0; i < CDROM_NUM; i++) {
        auto idx = model->index(i, 0);
        setCDROMBus(model, idx, cdrom[i].bus_type, cdrom[i].res);
        setCDROMSpeed(model, idx.siblingAtColumn(1), cdrom[i].speed);
    }
    ui->tableViewCDROM->resizeColumnsToContents();
    ui->tableViewCDROM->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    connect(ui->tableViewCDROM->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &SettingsFloppyCDROM::onCDROMRowChanged);
    ui->tableViewCDROM->setCurrentIndex(model->index(0, 0));
}

SettingsFloppyCDROM::~SettingsFloppyCDROM()
{
    delete ui;
}

void SettingsFloppyCDROM::save() {
    auto* model = ui->tableViewFloppy->model();
    for (int i = 0; i < FDD_NUM; i++) {
        fdd_set_type(i, model->index(i, 0).data(Qt::UserRole).toInt());
        fdd_set_turbo(i, model->index(i, 1).data() == "On" ? 1 : 0);
        fdd_set_check_bpb(i, model->index(i, 2).data() == "On" ? 1 : 0);
    }

    /* Removable devices category */
    model = ui->tableViewCDROM->model();
    for (int i = 0; i < CDROM_NUM; i++) {
	cdrom[i].img_fp = NULL;
        cdrom[i].priv = NULL;
        cdrom[i].ops = NULL;
        cdrom[i].image = NULL;
        cdrom[i].insert = NULL;
        cdrom[i].close = NULL;
        cdrom[i].get_volume = NULL;
        cdrom[i].get_channel = NULL;
        cdrom[i].bus_type = model->index(i, 0).data(Qt::UserRole).toUInt();
        cdrom[i].res = model->index(i, 0).data(Qt::UserRole + 1).toUInt();
        cdrom[i].speed = model->index(i, 1).data(Qt::UserRole).toUInt();
    }
}

void SettingsFloppyCDROM::onFloppyRowChanged(const QModelIndex &current) {
    int type = current.siblingAtColumn(0).data(Qt::UserRole).toInt();
    ui->comboBoxFloppyType->setCurrentIndex(type);
    ui->checkBoxTurboTimings->setChecked(current.siblingAtColumn(1).data() == "On");
    ui->checkBoxCheckBPB->setChecked(current.siblingAtColumn(2).data() == "On");
}

void SettingsFloppyCDROM::onCDROMRowChanged(const QModelIndex &current) {
    uint8_t bus = current.siblingAtColumn(0).data(Qt::UserRole).toUInt();
    uint8_t channel = current.siblingAtColumn(0).data(Qt::UserRole + 1).toUInt();
    uint8_t speed = current.siblingAtColumn(1).data(Qt::UserRole).toUInt();

    ui->comboBoxBus->setCurrentIndex(-1);
    auto* model = ui->comboBoxBus->model();
    auto match = model->match(model->index(0, 0), Qt::UserRole, bus);
    if (! match.isEmpty()) {
        ui->comboBoxBus->setCurrentIndex(match.first().row());
    }

    model = ui->comboBoxChannel->model();
    match = model->match(model->index(0, 0), Qt::UserRole, channel);
    if (! match.isEmpty()) {
        ui->comboBoxChannel->setCurrentIndex(match.first().row());
    }
    ui->comboBoxSpeed->setCurrentIndex(speed);
}

void SettingsFloppyCDROM::on_checkBoxTurboTimings_stateChanged(int arg1) {
    auto idx = ui->tableViewFloppy->selectionModel()->currentIndex();
    ui->tableViewFloppy->model()->setData(idx.siblingAtColumn(1), arg1 == Qt::Checked ? "On" : "Off");
}

void SettingsFloppyCDROM::on_checkBoxCheckBPB_stateChanged(int arg1) {
    auto idx = ui->tableViewFloppy->selectionModel()->currentIndex();
    ui->tableViewFloppy->model()->setData(idx.siblingAtColumn(2), arg1 == Qt::Checked ? "On" : "Off");
}

void SettingsFloppyCDROM::on_comboBoxFloppyType_activated(int index) {
    setFloppyType(ui->tableViewFloppy->model(), ui->tableViewFloppy->selectionModel()->currentIndex(), index);
}

void SettingsFloppyCDROM::on_comboBoxBus_currentIndexChanged(int index) {
    if (index < 0) {
        return;
    }

    int bus = ui->comboBoxBus->currentData().toInt();
    bool enabled = (bus != CDROM_BUS_DISABLED);
    ui->comboBoxChannel->setEnabled(enabled);
    ui->comboBoxSpeed->setEnabled(enabled);
    Harddrives::populateBusChannels(ui->comboBoxChannel->model(), bus);
}

void SettingsFloppyCDROM::on_comboBoxSpeed_activated(int index) {
    auto idx = ui->tableViewCDROM->selectionModel()->currentIndex();
    setCDROMSpeed(ui->tableViewCDROM->model(), idx.siblingAtColumn(1), index);
}


void SettingsFloppyCDROM::on_comboBoxBus_activated(int) {
    setCDROMBus(
        ui->tableViewCDROM->model(),
        ui->tableViewCDROM->selectionModel()->currentIndex(),
        ui->comboBoxBus->currentData().toUInt(),
        ui->comboBoxChannel->currentData().toUInt());
}


void SettingsFloppyCDROM::on_comboBoxChannel_activated(int) {
    setCDROMBus(
        ui->tableViewCDROM->model(),
        ui->tableViewCDROM->selectionModel()->currentIndex(),
        ui->comboBoxBus->currentData().toUInt(),
        ui->comboBoxChannel->currentData().toUInt());
}

