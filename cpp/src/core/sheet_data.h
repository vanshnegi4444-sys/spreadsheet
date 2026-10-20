#pragma once
#include "cell.h"
#include "cell_range.h"
#include "style.h"
#include "row_col.h"
#include "merge.h"
#include "auto_filter.h"
#include "validation.h"
#include "history.h"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

class SheetData {
public:
    std::string name = "sheet1";
    std::pair<int, int> freeze = {0, 0}; // [freeze_row, freeze_col]
    std::vector<Style> styles;
    Merges merges;
    Rows rows;
    Cols cols;
    Validations validations;
    AutoFilter autoFilter;
    History history;

    // Selection state
    int select_ri = 0;
    int select_ci = 0;
    CellRange selector;
    CellRange autofillRange;
    bool isAutofilling = false;

    // Clipboard state
    CellRange clipboardRange;
    bool clipboardIsCut = false;
    bool hasClipboard = false;

    // Filter/Sort row mapping
    std::set<int> exceptRowSet;
    std::map<int, int> sortedRowMap;   // visual row -> logical row
    std::map<int, int> unsortedRowMap; // logical row -> visual row

    SheetData(const std::string& n = "sheet1");

    int addStyle(const Style& s);
    Style getCellStyle(int ri, int ci) const;
    Style getSelectedCellStyle() const;

    void setSelectedCellAttr(const std::string& property, const std::string& value);
    void setSelectedCellBorder(const BorderStyle& borderStyle, const std::string& mode);

    void setCellText(int ri, int ci, const std::string& text, bool recordHistory = true);
    std::string getCellText(int ri, int ci) const;
    std::string getCellDisplayValue(int ri, int ci);

    bool isSingleSelected() const;
    bool canMerge() const;
    bool canUnmerge() const;
    void merge();
    void unmerge();

    void copy();
    void cut();
    bool paste(const std::string& what = "all");
    bool autofill(const CellRange& targetRange);

    void clearClipboard();

    void insert(const std::string& type, int n = 1); // "row" or "column"
    void deleteSelected(const std::string& type);    // "row" or "column"
    void deleteCellText();

    void setFreeze(int ri, int ci);
    bool freezeIsActive() const { return freeze.first > 0 || freeze.second > 0; }
    float freezeTotalWidth() const;
    float freezeTotalHeight() const;

    void autofilterToggle();
    void resetAutoFilter();

    bool canUndo() const { return history.canUndo(); }
    bool canRedo() const { return history.canRedo(); }
    void undo();
    void redo();

    int mapVisualToLogicalRow(int visualRi) const;
    int mapLogicalToVisualRow(int logicalRi) const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

    // Snapshot current state for undo
    void pushHistoryState();
};

} // namespace xspreadsheet
