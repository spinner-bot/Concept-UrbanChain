#pragma once

#include <QDialog>

#include "core/network.h"
#include "ds/dynamic_array.h"

class QTableWidget;

namespace uc {

class TransferDialog : public QDialog {
public:
    explicit TransferDialog(const Network& network, QWidget* parent = nullptr);

    DynamicArray<TransferRecord> records() const;

private:
    DynamicArray<TransferRecord> keys_;
    QTableWidget* table_ = nullptr;
};

}  // namespace uc
