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

    std::string exportCsv(int sheetIdx = -1) const;
    bool importCsv(const std::string& csvContent, int sheetIdx = -1);
};

} // namespace xspreadsheet
