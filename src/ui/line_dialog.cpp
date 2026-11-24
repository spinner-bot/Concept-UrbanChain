#include "ui/line_dialog.h"

#include <QColor>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

#include "core/map.h"

namespace uc {
namespace {

QString stationLabel(const Map& map, Index station) {
    if (station < 0 || station >= map.stationCount()) {
        return "?";
    }
    const Station& value = map.station(station);
    return QString::fromUtf8(value.id.c_str()) + " - " +
           QString::fromUtf8(value.name.c_str());
}

QSpinBox* makeSpin(int minimum, int maximum, int value, QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    return spin;
}

QDoubleSpinBox* makeDoubleSpin(double minimum, double maximum, double value, QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setDecimals(1);
    spin->setValue(value);
    return spin;
}

}  // namespace

LineDialog::LineDialog(const Line& line, const Map& map, QWidget* parent)
    : QDialog(parent), line_(line), map_(map), color_(line.color()) {
    setWindowTitle("Line Properties");
    resize(700, 760);

    auto* layout = new QVBoxLayout(this);

    auto* basicForm = new QFormLayout();
    nameEdit_ = new QLineEdit(QString::fromUtf8(line_.name().c_str()), this);
    basicForm->addRow("Name", nameEdit_);

    colorButton_ = new QPushButton(this);
    colorButton_->setMinimumWidth(120);
    connect(colorButton_, &QPushButton::clicked, this, [this] { chooseColor(); });
    basicForm->addRow("Color", colorButton_);

    dwellSpin_ = makeDoubleSpin(0.0, 3600.0, line_.ops().dwellSec, this);
    basicForm->addRow("Dwell at stops (s)", dwellSpin_);
    layout->addLayout(basicForm);

    layout->addWidget(new QLabel("Stops (top to bottom)", this));
    stopsList_ = new QListWidget(this);
    stopsList_->setMaximumHeight(150);
    for (std::size_t i = 0; i < line_.stations().size(); ++i) {
        auto* item = new QListWidgetItem(stationLabel(map_, line_.stations()[i]));
        item->setData(Qt::UserRole, line_.stations()[i]);
        stopsList_->addItem(item);
    }
    layout->addWidget(stopsList_);
    auto* stopButtons = new QHBoxLayout();
    auto* removeStopButton = new QPushButton("Remove stop", this);
    auto* upStopButton = new QPushButton("Move up", this);
    auto* downStopButton = new QPushButton("Move down", this);
    connect(removeStopButton, &QPushButton::clicked, this, [this] {
        const int row = stopsList_->currentRow();
        if (row < 0) {
            return;
        }
        const DynamicArray<Index> oldStops = currentStops();
        const DynamicArray<double> oldRuns = currentRuns();
        delete stopsList_->takeItem(row);
        rebuildSegmentTable(oldStops, oldRuns);
    });
    connect(upStopButton, &QPushButton::clicked, this, [this] { moveSelectedStop(-1); });
    connect(downStopButton, &QPushButton::clicked, this, [this] { moveSelectedStop(1); });
    stopButtons->addWidget(removeStopButton);
    stopButtons->addWidget(upStopButton);
    stopButtons->addWidget(downStopButton);
    stopButtons->addStretch(1);
    layout->addLayout(stopButtons);

    layout->addWidget(new QLabel("Segment run times (s)", this));
    segmentTable_ = new QTableWidget(0, 3, this);
    segmentTable_->setHorizontalHeaderLabels({"From", "To", "Run (s)"});
    segmentTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    segmentTable_->verticalHeader()->setVisible(false);
    rebuildSegmentTable(line_.stations(), line_.ops().segmentRunSec);
    layout->addWidget(segmentTable_, 1);

    layout->addWidget(new QLabel("Headway periods (seconds since midnight)", this));
    headwayTable_ = new QTableWidget(0, 3, this);
    headwayTable_->setHorizontalHeaderLabels({"Start", "End", "Interval (s)"});
    headwayTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    headwayTable_->verticalHeader()->setVisible(false);
    for (std::size_t i = 0; i < line_.ops().headway.size(); ++i) {
        addHeadwayRow(line_.ops().headway[i]);
    }
    if (line_.ops().headway.empty()) {
        addHeadwayRow(HeadwayPeriod{0, 86400, 300});
    }
    layout->addWidget(headwayTable_);

    auto* headwayButtons = new QHBoxLayout();
    auto* addButton = new QPushButton("Add period", this);
    auto* removeButton = new QPushButton("Remove period", this);
    connect(addButton, &QPushButton::clicked, this, [this] {
        addHeadwayRow(HeadwayPeriod{0, 86400, 300});
    });
    connect(removeButton, &QPushButton::clicked, this, [this] { removeSelectedHeadway(); });
    headwayButtons->addWidget(addButton);
    headwayButtons->addWidget(removeButton);
    headwayButtons->addStretch(1);
    layout->addLayout(headwayButtons);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    updateColorButton();
}

void LineDialog::chooseColor() {
    const QColor initial = QColor::fromRgb(static_cast<QRgb>(color_ | 0xff000000u));
    const QColor chosen = QColorDialog::getColor(initial, this, "Line color");
    if (chosen.isValid()) {
        color_ = static_cast<std::uint32_t>(chosen.rgb()) & 0x00ffffffu;
        updateColorButton();
    }
}

void LineDialog::updateColorButton() {
    const QColor color = QColor::fromRgb(static_cast<QRgb>(color_ | 0xff000000u));
    const int luminance = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
    const QString textColor = luminance > 150 ? "#111827" : "#ffffff";
    colorButton_->setText(color.name().toUpper());
    colorButton_->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid #555;")
                                    .arg(color.name(), textColor));
}

void LineDialog::addHeadwayRow(const HeadwayPeriod& period) {
    const int row = headwayTable_->rowCount();
    headwayTable_->insertRow(row);
    headwayTable_->setCellWidget(row, 0, makeSpin(0, 172800, period.startSec, this));
    headwayTable_->setCellWidget(row, 1, makeSpin(0, 172800, period.endSec, this));
    headwayTable_->setCellWidget(row, 2, makeSpin(1, 86400, period.headwaySec, this));
}

void LineDialog::removeSelectedHeadway() {
    const int row = headwayTable_->currentRow();
    if (row >= 0) {
        headwayTable_->removeRow(row);
    }
}

DynamicArray<Index> LineDialog::currentStops() const {
    DynamicArray<Index> stops;
    for (int row = 0; row < stopsList_->count(); ++row) {
        stops.push_back(stopsList_->item(row)->data(Qt::UserRole).toInt());
    }
    return stops;
}

DynamicArray<double> LineDialog::currentRuns() const {
    DynamicArray<double> runs;
    for (int row = 0; row < segmentTable_->rowCount(); ++row) {
        const auto* spin = qobject_cast<QDoubleSpinBox*>(segmentTable_->cellWidget(row, 2));
        runs.push_back(spin != nullptr ? spin->value() : 120.0);
    }
    return runs;
}

void LineDialog::rebuildSegmentTable(const DynamicArray<Index>& oldStops,
                                     const DynamicArray<double>& oldRuns) {
    const DynamicArray<Index> stops = currentStops();
    segmentTable_->setRowCount(0);
    const int segments = stops.size() >= 2 ? static_cast<int>(stops.size()) - 1 : 0;
    segmentTable_->setRowCount(segments);
    for (int row = 0; row < segments; ++row) {
        const Index from = stops[static_cast<std::size_t>(row)];
        const Index to = stops[static_cast<std::size_t>(row + 1)];
        auto* fromItem = new QTableWidgetItem(stationLabel(map_, from));
        auto* toItem = new QTableWidgetItem(stationLabel(map_, to));
        fromItem->setFlags(fromItem->flags() & ~Qt::ItemIsEditable);
        toItem->setFlags(toItem->flags() & ~Qt::ItemIsEditable);
        segmentTable_->setItem(row, 0, fromItem);
        segmentTable_->setItem(row, 1, toItem);

        double seconds = 120.0;
        for (std::size_t i = 0; i + 1 < oldStops.size(); ++i) {
            const bool forward = oldStops[i] == from && oldStops[i + 1] == to;
            const bool backward = oldStops[i] == to && oldStops[i + 1] == from;
            if ((forward || backward) && i < oldRuns.size()) {
                seconds = oldRuns[i];
                break;
            }
        }
        segmentTable_->setCellWidget(row, 2, makeDoubleSpin(0.0, 36000.0, seconds, this));
    }
}

void LineDialog::moveSelectedStop(int delta) {
    const int row = stopsList_->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= stopsList_->count()) {
        return;
    }
    const DynamicArray<Index> oldStops = currentStops();
    const DynamicArray<double> oldRuns = currentRuns();
    QListWidgetItem* item = stopsList_->takeItem(row);
    stopsList_->insertItem(target, item);
    stopsList_->setCurrentRow(target);
    rebuildSegmentTable(oldStops, oldRuns);
}

Line LineDialog::result() const {
    Line updated = line_;
    updated.setName(nameEdit_->text().trimmed().toUtf8().constData());
    updated.setColor(color_);
    updated.ops().dwellSec = dwellSpin_->value();
    updated.setStations(currentStops());

    updated.ops().segmentRunSec.clear();
    for (int row = 0; row < segmentTable_->rowCount(); ++row) {
        const auto* spin = qobject_cast<QDoubleSpinBox*>(segmentTable_->cellWidget(row, 2));
        updated.ops().segmentRunSec.push_back(spin != nullptr ? spin->value() : 120.0);
    }

    updated.ops().headway.clear();
    for (int row = 0; row < headwayTable_->rowCount(); ++row) {
        const auto* start = qobject_cast<QSpinBox*>(headwayTable_->cellWidget(row, 0));
        const auto* end = qobject_cast<QSpinBox*>(headwayTable_->cellWidget(row, 1));
        const auto* interval = qobject_cast<QSpinBox*>(headwayTable_->cellWidget(row, 2));
        if (start != nullptr && end != nullptr && interval != nullptr) {
            updated.ops().headway.push_back(
                HeadwayPeriod{start->value(), end->value(), interval->value()});
        }
    }
    return updated;
}

}  // namespace uc
