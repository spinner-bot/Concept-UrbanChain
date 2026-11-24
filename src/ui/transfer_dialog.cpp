#include "ui/transfer_dialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace uc {
namespace {

QString stationLabel(const Map& map, Index station) {
    const Station& value = map.station(station);
    return QString::fromUtf8(value.id.c_str()) + " - " +
           QString::fromUtf8(value.name.c_str());
}

}  // namespace

TransferDialog::TransferDialog(const Network& network, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Transfer Times");
    resize(720, 420);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        "Transfer walk times for shared stations. Each ordered line pair is listed once.",
        this));

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"Station", "From line", "To line", "Seconds"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (Index station = 0; station < network.map().stationCount(); ++station) {
        DynamicArray<int> lines;
        for (std::size_t line = 0; line < network.lines().size(); ++line) {
            if (network.lines()[line].containsStation(station)) {
                lines.push_back(static_cast<int>(line));
            }
        }
        for (std::size_t i = 0; i < lines.size(); ++i) {
            for (std::size_t j = 0; j < lines.size(); ++j) {
                if (i == j) {
                    continue;
                }
                const int fromLine = lines[i];
                const int toLine = lines[j];
                const int row = table_->rowCount();
                table_->insertRow(row);

                auto* stationItem = new QTableWidgetItem(stationLabel(network.map(), station));
                auto* fromItem = new QTableWidgetItem(
                    QString::fromUtf8(network.lines()[static_cast<std::size_t>(fromLine)]
                                          .name()
                                          .c_str()));
                auto* toItem = new QTableWidgetItem(
                    QString::fromUtf8(network.lines()[static_cast<std::size_t>(toLine)]
                                          .name()
                                          .c_str()));
                table_->setItem(row, 0, stationItem);
                table_->setItem(row, 1, fromItem);
                table_->setItem(row, 2, toItem);

                auto* spin = new QDoubleSpinBox(table_);
                spin->setRange(0.0, 3600.0);
                spin->setDecimals(1);
                spin->setValue(network.transferTime(station, fromLine, toLine));
                table_->setCellWidget(row, 3, spin);

                TransferRecord key;
                key.station = station;
                key.fromLine = fromLine;
                key.toLine = toLine;
                key.seconds = 0.0;
                keys_.push_back(key);
            }
        }
    }
    layout->addWidget(table_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

DynamicArray<TransferRecord> TransferDialog::records() const {
    DynamicArray<TransferRecord> result;
    for (std::size_t row = 0; row < keys_.size(); ++row) {
        const auto* spin =
            qobject_cast<QDoubleSpinBox*>(table_->cellWidget(static_cast<int>(row), 3));
        TransferRecord record = keys_[row];
        record.seconds = spin != nullptr ? spin->value() : 120.0;
        result.push_back(record);
    }
    return result;
}

}  // namespace uc
