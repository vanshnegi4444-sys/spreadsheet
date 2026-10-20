#pragma once
#include "../core/sheet_data.h"
#include <string>
#include <vector>
#include <map>

namespace xspreadsheet {

class DialogSortFilter {
public:
    bool isOpen = false;
    int targetCi = -1;
    std::map<std::string, int> valueCounts;
    std::map<std::string, bool> checkedValues;
    char searchBuf[64] = "";

    void open(SheetData& sheet, int ci);
    void render(SheetData& sheet);
};

} // namespace xspreadsheet
