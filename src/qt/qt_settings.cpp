#include "qt_settings.hpp"
#include "ui_qt_settings.h"

#include "qt_settingsmachine.hpp"
#include "qt_settingsdisplay.hpp"
#include "qt_settingsinput.hpp"
#include "qt_settingssound.hpp"
#include "qt_settingsnetwork.hpp"
#include "qt_settingsports.hpp"
#include "qt_settingsstoragecontrollers.hpp"
#include "qt_settingsharddisks.hpp"
#include "qt_settingsfloppycdrom.hpp"
#include "qt_settingsotherremovable.hpp"
#include "qt_settingsotherperipherals.hpp"

extern "C"
{
#include <86box/86box.h>
}

#include <QDebug>
#include <QMessageBox>
#include <QCheckBox>

class SettingsModel : public QAbstractListModel {
public:
    SettingsModel(QObject* parent) : QAbstractListModel(parent) {}

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
private:
    QStringList pages = {
        "Machine",
        "Display",
        "Input Devices",
        "Sound",
        "Network",
        "Ports (COM & LPT)",
        "Storage Controllers",
        "Hard Disks",
        "Floppy & CD-ROM Drives",
        "Other Removable Devices",
        "Other Peripherals",
    };
    QStringList page_icons = {
        "machine",
        "display",
        "input_devices",
        "sound",
        "network",
        "ports",
        "storage_controllers",
        "hard_disk",
        "floppy_and_cdrom_drives",
        "other_removable_devices",
        "other_peripherals",
    };
};

QVariant SettingsModel::data(const QModelIndex &index, int role) const {
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid | QAbstractItemModel::CheckIndexOption::ParentIsInvalid));

    switch (role) {
    case Qt::DisplayRole:
        return pages.at(index.row());
    case Qt::DecorationRole:
        return QIcon(QString(":/settings/win/icons/%1.ico").arg(page_icons[index.row()]));
    default:
        return {};
    }
}

int SettingsModel::rowCount(const QModelIndex &parent) const {
    (void) parent;
    return pages.size();
}

Settings::Settings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Settings)
{
    ui->setupUi(this);

    ui->listView->setModel(new SettingsModel(this));

    machine = new SettingsMachine(this);
    display = new SettingsDisplay(this);
    input = new SettingsInput(this);
    sound = new SettingsSound(this);
    network = new SettingsNetwork(this);
    ports = new SettingsPorts(this);
    storageControllers = new SettingsStorageControllers(this);
    harddisks = new SettingsHarddisks(this);
    floppyCdrom = new SettingsFloppyCDROM(this);
    otherRemovable = new SettingsOtherRemovable(this);
    otherPeripherals = new SettingsOtherPeripherals(this);

    ui->stackedWidget->addWidget(machine);
    ui->stackedWidget->addWidget(display);
    ui->stackedWidget->addWidget(input);
    ui->stackedWidget->addWidget(sound);
    ui->stackedWidget->addWidget(network);
    ui->stackedWidget->addWidget(ports);
    ui->stackedWidget->addWidget(storageControllers);
    ui->stackedWidget->addWidget(harddisks);
    ui->stackedWidget->addWidget(floppyCdrom);
    ui->stackedWidget->addWidget(otherRemovable);
    ui->stackedWidget->addWidget(otherPeripherals);

    connect(machine, &SettingsMachine::currentMachineChanged, display, &SettingsDisplay::onCurrentMachineChanged);
    connect(machine, &SettingsMachine::currentMachineChanged, input, &SettingsInput::onCurrentMachineChanged);
    connect(machine, &SettingsMachine::currentMachineChanged, sound, &SettingsSound::onCurrentMachineChanged);
    connect(machine, &SettingsMachine::currentMachineChanged, network, &SettingsNetwork::onCurrentMachineChanged);
    connect(machine, &SettingsMachine::currentMachineChanged, storageControllers, &SettingsStorageControllers::onCurrentMachineChanged);
    connect(machine, &SettingsMachine::currentMachineChanged, otherPeripherals, &SettingsOtherPeripherals::onCurrentMachineChanged);

    connect(ui->listView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &previous) {
        ui->stackedWidget->setCurrentIndex(current.row());
    });
}

Settings::~Settings()
{
    delete ui;
}

void Settings::save() {
    machine->save();
    display->save();
    input->save();
    sound->save();
    network->save();
    ports->save();
    storageControllers->save();
    harddisks->save();
    floppyCdrom->save();
    otherRemovable->save();
    otherPeripherals->save();
}

void Settings::accept()
{
    if (confirm_save)
    {
        QMessageBox questionbox(QMessageBox::Icon::Question, "86Box", "Do you want to save the settings?\n\nThis will hard reset the emulated machine.", QMessageBox::Save | QMessageBox::Cancel, this);
        QCheckBox *chkbox = new QCheckBox("Do not ask me again");
        questionbox.setCheckBox(chkbox);
        chkbox->setChecked(!confirm_save);
        QObject::connect(chkbox, &QCheckBox::stateChanged, [](int state) {
            confirm_save = (state == Qt::CheckState::Unchecked);
        });
        questionbox.exec();
        if (questionbox.result() == QMessageBox::Cancel) {
            confirm_save = true;
            return;
        }
    }
    QDialog::accept();
}
