#pragma once
#include "../core/sheet_data.h"
#include <string>

namespace xspreadsheet {

class DialogPrint {
public:
    bool isOpen = false;
    int paperSizeIdx = 1; // 0: A3, 1: A4, 2: A5, 3: B4, 4: B5
    int orientationIdx = 0; // 0: Landscape, 1: Portrait
    char exportPathBuf[256] = "spreadsheet_export.csv";

    void open() { isOpen = true; }
    void render(SheetData& sheet);
};

} // namespace xspreadsheet
