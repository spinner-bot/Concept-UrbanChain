#pragma once

#include <QDialog>

#include <cstdint>

#include "core/line.h"

class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace uc {

class Map;

class LineDialog : public QDialog {
public:
    LineDialog(const Line& line, const Map& map, QWidget* parent = nullptr);

    Line result() const;

private:
    void chooseColor();
    void updateColorButton();
    void addHeadwayRow(const HeadwayPeriod& period);
    void removeSelectedHeadway();
    DynamicArray<Index> currentStops() const;
    DynamicArray<double> currentRuns() const;
    void rebuildSegmentTable(const DynamicArray<Index>& oldStops,
                             const DynamicArray<double>& oldRuns);
    void moveSelectedStop(int delta);

    Line line_;
    const Map& map_;
    std::uint32_t color_ = 0;

    QLineEdit* nameEdit_ = nullptr;
    QPushButton* colorButton_ = nullptr;
    QDoubleSpinBox* dwellSpin_ = nullptr;
    QListWidget* stopsList_ = nullptr;
    QTableWidget* segmentTable_ = nullptr;
    QTableWidget* headwayTable_ = nullptr;
};

}  // namespace uc
