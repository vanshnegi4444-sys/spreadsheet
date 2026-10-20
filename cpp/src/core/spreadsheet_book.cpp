#include "spreadsheet_book.h"
#include <fstream>
#include <sstream>

namespace xspreadsheet {

SpreadsheetBook::SpreadsheetBook() {
    addSheet("sheet1");
}

std::shared_ptr<SheetData> SpreadsheetBook::activeSheet() {
    if (sheets.empty()) {
        addSheet("sheet1");
    }
    if (activeSheetIndex < 0 || activeSheetIndex >= static_cast<int>(sheets.size())) {
        activeSheetIndex = 0;
    }
    return sheets[activeSheetIndex];
}

const std::shared_ptr<SheetData> SpreadsheetBook::activeSheet() const {
    if (sheets.empty()) return nullptr;
    int idx = (activeSheetIndex >= 0 && activeSheetIndex < static_cast<int>(sheets.size())) ? activeSheetIndex : 0;
    return sheets[idx];
}

std::shared_ptr<SheetData> SpreadsheetBook::addSheet(const std::string& name) {
    std::string sName = name;
    if (sName.empty()) {
        sName = "sheet" + std::to_string(sheets.size() + 1);
    }
    auto sheet = std::make_shared<SheetData>(sName);
    sheets.push_back(sheet);
    activeSheetIndex = static_cast<int>(sheets.size() - 1);
    return sheet;
}

bool SpreadsheetBook::deleteSheet(int index) {
    if (sheets.size() <= 1 || index < 0 || index >= static_cast<int>(sheets.size())) {
        return false;
    }
    sheets.erase(sheets.begin() + index);
    if (activeSheetIndex >= static_cast<int>(sheets.size())) {
        activeSheetIndex = static_cast<int>(sheets.size() - 1);
    }
    return true;
}

bool SpreadsheetBook::renameSheet(int index, const std::string& newName) {
    if (index < 0 || index >= static_cast<int>(sheets.size()) || newName.empty()) {
        return false;
    }
    sheets[index]->name = newName;
    return true;
}

nlohmann::json SpreadsheetBook::toJson() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& sheet : sheets) {
        arr.push_back(sheet->toJson());
    }
    return arr;
}

void SpreadsheetBook::fromJson(const nlohmann::json& j) {
    sheets.clear();
    if (j.is_array()) {
        for (const auto& sj : j) {
            auto sheet = std::make_shared<SheetData>();
            sheet->fromJson(sj);
            sheets.push_back(sheet);
        }
    } else if (j.is_object()) {
        auto sheet = std::make_shared<SheetData>();
        sheet->fromJson(j);
        sheets.push_back(sheet);
    }
    if (sheets.empty()) {
        addSheet("sheet1");
    }
    activeSheetIndex = 0;
}

#include <filesystem>

bool SpreadsheetBook::saveToFile(const std::string& filepath) const {
    std::string tempPath = filepath + ".tmp";
    {
        std::ofstream out(tempPath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) return false;
        out << toJson().dump(2);
        out.flush();
        if (!out.good()) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tempPath, filepath, ec);
    if (ec) {
        std::filesystem::copy_file(tempPath, filepath, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tempPath, ec);
    }
    return !ec;
}

bool SpreadsheetBook::loadFromFile(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) return false;
    try {
        nlohmann::json j;
        in >> j;
        fromJson(j);
        return true;
    } catch (...) {
        return false;
    }
}

std::string SpreadsheetBook::exportCsv(int sheetIdx) const {
    int idx = (sheetIdx >= 0 && sheetIdx < static_cast<int>(sheets.size())) ? sheetIdx : activeSheetIndex;
    if (idx < 0 || idx >= static_cast<int>(sheets.size())) return "";

    const auto& sheet = sheets[idx];
    auto [maxR, maxC] = sheet->rows.maxCell();
    std::ostringstream ss;

    for (int r = 0; r <= maxR; ++r) {
        for (int c = 0; c <= maxC; ++c) {
            std::string text = sheet->getCellText(r, c);
            bool needQuotes = (text.find(',') != std::string::npos ||
                               text.find('"') != std::string::npos ||
                               text.find('\n') != std::string::npos);
            if (needQuotes) {
                ss << '"';
                for (char ch : text) {
                    if (ch == '"') ss << "\"\"";
                    else ss << ch;
                }
                ss << '"';
            } else {
                ss << text;
            }
            if (c < maxC) ss << ',';
        }
        ss << "\r\n";
    }
    return ss.str();
}

bool SpreadsheetBook::importCsv(const std::string& csvContent, int sheetIdx) {
    int idx = (sheetIdx >= 0 && sheetIdx < static_cast<int>(sheets.size())) ? sheetIdx : activeSheetIndex;
    if (idx < 0 || idx >= static_cast<int>(sheets.size())) return false;

    auto& sheet = sheets[idx];
    sheet->pushHistoryState();

    int row = 0;
    int col = 0;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < csvContent.length(); ++i) {
        char ch = csvContent[i];
        if (inQuotes) {
            if (ch == '"') {
                if (i + 1 < csvContent.length() && csvContent[i + 1] == '"') {
                    current += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current += ch;
            }
        } else {
            if (ch == '"') {
                inQuotes = true;
            } else if (ch == ',') {
                if (row >= sheet->rows.len) sheet->rows.len = row + 50;
                if (col >= sheet->cols.len) sheet->cols.len = col + 26;
                sheet->setCellText(row, col, current, false);
                current.clear();
                ++col;
            } else if (ch == '\n' || ch == '\r') {
                if (ch == '\r' && i + 1 < csvContent.length() && csvContent[i + 1] == '\n') {
                    ++i;
                }
                if (row >= sheet->rows.len) sheet->rows.len = row + 50;
                if (col >= sheet->cols.len) sheet->cols.len = col + 26;
                sheet->setCellText(row, col, current, false);
                current.clear();
                col = 0;
                ++row;
            } else {
                current += ch;
            }
        }
    }
    if (!current.empty() || col > 0) {
        if (row >= sheet->rows.len) sheet->rows.len = row + 50;
        if (col >= sheet->cols.len) sheet->cols.len = col + 26;
        sheet->setCellText(row, col, current, false);
    }
    return true;
}

} // namespace xspreadsheet
