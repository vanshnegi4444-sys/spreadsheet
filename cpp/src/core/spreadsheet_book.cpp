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
        if (!out.is_open()) {
            lastError_ = "Cannot open temporary file for writing: " + tempPath;
            return false;
        }
        out << toJson().dump(2);
        out.flush();
        if (!out.good()) {
            lastError_ = "Failed to write workbook data (disk full?)";
            std::error_code ec2;
            std::filesystem::remove(tempPath, ec2);
            return false;
        }
    }
    // Atomic on the same filesystem: the old file stays intact until the
    // new one is complete, then the rename swaps it in one step.
    std::error_code ec;
    std::filesystem::rename(tempPath, filepath, ec);
    if (ec) {
        // Rename can fail across filesystems; fall back to copy + delete.
        ec.clear();
        std::filesystem::copy_file(tempPath, filepath,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            std::error_code removeEc;
            std::filesystem::remove(tempPath, removeEc);
        }
    }
    if (ec) {
        lastError_ = "Failed to save file: " + filepath + " (" + ec.message() + ")";
        return false;
    }
    lastError_.clear();
    return true;
}

bool SpreadsheetBook::loadFromFile(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        lastError_ = "Cannot open file: " + filepath;
        return false;
    }
    try {
        nlohmann::json j;
        in >> j;
        fromJson(j);
        lastError_.clear();
        return true;
    } catch (const nlohmann::json::exception& e) {
        // e.what() carries the byte position, e.g. "parse error at line
        // 1, column 5" — enough for the UI to tell the user what is wrong.
        lastError_ = std::string("Invalid workbook file: ") + e.what();
        return false;
    } catch (...) {
        lastError_ = "Unknown error while loading file: " + filepath;
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
            // CSV injection: a cell starting with =, +, - or @ runs as a
            // formula when the file is opened in Excel. Prefixing one
            // single quote makes Excel treat it as plain text instead.
            if (!text.empty() && (text[0] == '=' || text[0] == '+' ||
                                  text[0] == '-' || text[0] == '@')) {
                text = "'" + text;
            }
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

    // Strip a UTF-8 byte-order mark if present, so it doesn't end up
    // inside cell A1 (Excel writes one when it saves a CSV as UTF-8).
    std::string body = csvContent;
    if (body.size() >= 3 &&
        static_cast<unsigned char>(body[0]) == 0xEF &&
        static_cast<unsigned char>(body[1]) == 0xBB &&
        static_cast<unsigned char>(body[2]) == 0xBF) {
        body.erase(0, 3);
    }

    int row = 0;
    int col = 0;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < body.length(); ++i) {
        char ch = body[i];
        if (inQuotes) {
            if (ch == '"') {
                if (i + 1 < body.length() && body[i + 1] == '"') {
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
                if (ch == '\r' && i + 1 < body.length() && body[i + 1] == '\n') {
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
