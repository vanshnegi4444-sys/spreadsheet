#pragma once
#include "sheet_data.h"
#include <vector>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

class SpreadsheetBook {
public:
    std::vector<std::shared_ptr<SheetData>> sheets;
    int activeSheetIndex = 0;

    SpreadsheetBook();

    std::shared_ptr<SheetData> activeSheet();
    const std::shared_ptr<SheetData> activeSheet() const;

    std::shared_ptr<SheetData> addSheet(const std::string& name = "");
    bool deleteSheet(int index);
    bool renameSheet(int index, const std::string& newName);

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

    bool saveToFile(const std::string& filepath) const;
    bool loadFromFile(const std::string& filepath);

    // Why the last save/load failed ("" when the last one worked). The UI
    // can show this instead of failing silently.
    std::string lastError() const { return lastError_; }

    std::string exportCsv(int sheetIdx = -1) const;
    bool importCsv(const std::string& csvContent, int sheetIdx = -1);

private:
    // Mutable so const saveToFile can record a failure for the caller.
    mutable std::string lastError_;
};

} // namespace xspreadsheet
