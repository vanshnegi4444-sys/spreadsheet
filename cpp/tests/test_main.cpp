#include <iostream>
#include <cassert>
#include <cmath>
#include <fstream>
#include "core/alphabet.h"
#include "core/cell_range.h"
#include "core/format.h"
#include "core/formula.h"
#include "core/sheet_data.h"
#include "core/spreadsheet_book.h"
#include "core/auto_filter.h"
#include "core/validation.h"
#include "core/history.h"
#include "locale/locale.h"

using namespace xspreadsheet;

void testAlphabet() {
    std::cout << "Running testAlphabet()..." << std::endl;
    assert(stringAt(0) == "A");
    assert(stringAt(25) == "Z");
    assert(stringAt(26) == "AA");
    assert(stringAt(27) == "AB");

    assert(indexAt("A") == 0);
    assert(indexAt("Z") == 25);
    assert(indexAt("AA") == 26);
    assert(indexAt("AB") == 27);

    int x = 0, y = 0;
    assert(expr2xy("A1", x, y) && x == 0 && y == 0);
    assert(expr2xy("B10", x, y) && x == 1 && y == 9);
    assert(expr2xy("AA15", x, y) && x == 26 && y == 14);

    assert(xy2expr(0, 0) == "A1");
    assert(xy2expr(1, 9) == "B10");
    assert(xy2expr(26, 14) == "AA15");

    assert(expr2expr("A1", 1, 2) == "B3");
    std::cout << "testAlphabet() PASSED" << std::endl;
}

void testCellRange() {
    std::cout << "Running testCellRange()..." << std::endl;
    CellRange cr1 = CellRange::valueOf("A1:B3");
    assert(cr1.sri == 0 && cr1.sci == 0 && cr1.eri == 2 && cr1.eci == 1);
    assert(cr1.multiple() == true);
    assert(cr1.rowCount() == 3);
    assert(cr1.colCount() == 2);
    assert(cr1.includes(1, 1) == true);
    assert(cr1.includes(3, 1) == false);

    CellRange cr2 = CellRange::valueOf("B2:C4");
    assert(cr1.intersects(cr2) == true);

    CellRange un = cr1.unionRange(cr2);
    assert(un.sri == 0 && un.sci == 0 && un.eri == 3 && un.eci == 2);

    auto diff = cr1.difference(CellRange::valueOf("A1:A1"));
    assert(!diff.empty());
    std::cout << "testCellRange() PASSED" << std::endl;
}

void testFormat() {
    std::cout << "Running testFormat()..." << std::endl;
    assert(Formatter::formatNumber(1000.12) == "1,000.12");
    assert(Formatter::formatNumber(-1234567.89) == "-1,234,567.89");
    assert(Formatter::format("usd", "50") == "$50.00");
    assert(Formatter::format("rmb", "100") == "￥100.00");
    assert(Formatter::format("percent", "0.25") == "25.00%");
    std::cout << "testFormat() PASSED" << std::endl;
}

void testFormula() {
    std::cout << "Running testFormula()..." << std::endl;
    std::map<std::pair<int, int>, std::string> cells;
    cells[{0, 0}] = "10"; // A1
    cells[{1, 0}] = "20"; // B1
    cells[{0, 1}] = "30"; // A2
    cells[{1, 1}] = "40"; // B2

    auto lookup = [&](int x, int y) -> std::string {
        auto it = cells.find({x, y});
        if (it != cells.end()) return it->second;
        return "";
    };

    assert(FormulaEngine::eval("=1+2*3", lookup) == "7");
    assert(FormulaEngine::eval("=(1+2)*3", lookup) == "9");
    assert(FormulaEngine::eval("=A1+B1", lookup) == "30");
    assert(FormulaEngine::eval("=SUM(A1:B2)", lookup) == "100");
    assert(FormulaEngine::eval("=AVERAGE(A1:B2)", lookup) == "25");
    assert(FormulaEngine::eval("=MAX(A1:B2)", lookup) == "40");
    assert(FormulaEngine::eval("=MIN(A1:B2)", lookup) == "10");
    assert(FormulaEngine::eval("=IF(A1<B1, 100, 200)", lookup) == "100");
    assert(FormulaEngine::eval("=CONCAT(\"Hello\", \" \", \"World\")", lookup) == "Hello World");

    // Cycle detection test
    cells[{2, 0}] = "=D1"; // C1 = D1
    cells[{3, 0}] = "=C1"; // D1 = C1
    std::string cycleRes = FormulaEngine::eval("=C1", lookup);
    assert(cycleRes == "#CIRC!");

    std::cout << "testFormula() PASSED" << std::endl;
}

void testSheetDataAndJSON() {
    std::cout << "Running testSheetDataAndJSON()..." << std::endl;
    SheetData sheet("test_sheet");
    sheet.setCellText(0, 0, "Hello", false);
    sheet.setCellText(0, 1, "World", false);
    sheet.setCellText(1, 0, "=CONCAT(A1, \" \", B1)", false);

    assert(sheet.getCellText(0, 0) == "Hello");
    assert(sheet.getCellDisplayValue(1, 0) == "Hello World");

    // Merges
    sheet.selector.set(3, 3, 4, 4);
    sheet.merge();
    assert(sheet.canUnmerge() == true);
    sheet.unmerge();
    assert(sheet.canUnmerge() == false);

    // Serialization round-trip
    sheet.setFreeze(2, 2);
    nlohmann::json j = sheet.toJson();
    SheetData loaded("restored");
    loaded.fromJson(j);

    assert(loaded.name == "test_sheet");
    assert(loaded.freeze.first == 2 && loaded.freeze.second == 2);
    assert(loaded.getCellText(0, 0) == "Hello");
    assert(loaded.getCellDisplayValue(1, 0) == "Hello World");

    std::cout << "testSheetDataAndJSON() PASSED" << std::endl;
}

void testValidation() {
    std::cout << "Running testValidation()..." << std::endl;
    Validator val;
    val.type = "list";
    val.required = true;
    val.value = "Apple,Banana,Orange";

    assert(val.validate("Banana").first == true);
    assert(val.validate("Grape").first == false);

    val.type = "number";
    val.op = "be";
    val.value = "10";
    val.value2 = "50";
    assert(val.validate("25").first == true);
    assert(val.validate("5").first == false);
    assert(val.validate("100").first == false);

    val.type = "email";
    assert(val.validate("test@example.com").first == true);
    assert(val.validate("invalid-email").first == false);
    std::cout << "testValidation() PASSED" << std::endl;
}

void testHistory() {
    std::cout << "Running testHistory()..." << std::endl;
    History history;
    assert(!history.canUndo());
    assert(!history.canRedo());

    SheetData sheet("sheet1");
    sheet.setCellText(0, 0, "Initial", false);
    history.add(sheet.toJson());

    sheet.setCellText(0, 0, "Modified 1", false);
    assert(history.canUndo());

    nlohmann::json prevSnap;
    assert(history.undo(sheet.toJson(), prevSnap));
    sheet.fromJson(prevSnap);
    assert(sheet.getCellText(0, 0) == "Initial");
    assert(history.canRedo());

    nlohmann::json nextSnap;
    assert(history.redo(sheet.toJson(), nextSnap));
    sheet.fromJson(nextSnap);
    assert(sheet.getCellText(0, 0) == "Modified 1");

    // Test native zero-JSON SheetData undo/redo
    SheetData nativeSheet("native_test");
    nativeSheet.setCellText(0, 0, "Step 1", true);
    nativeSheet.setCellText(0, 0, "Step 2", true);
    assert(nativeSheet.canUndo());
    nativeSheet.undo();
    assert(nativeSheet.getCellText(0, 0) == "Step 1");
    assert(nativeSheet.canRedo());
    nativeSheet.redo();
    assert(nativeSheet.getCellText(0, 0) == "Step 2");

    // Test cell memory pruning
    nativeSheet.setCellText(5, 5, "temporary", false);
    assert(nativeSheet.rows.data.find(5) != nativeSheet.rows.data.end());
    nativeSheet.setCellText(5, 5, "", false);
    assert(nativeSheet.rows.data.find(5) == nativeSheet.rows.data.end()); // row purged!

    std::cout << "testHistory() PASSED" << std::endl;
}

void testAutoFilter() {
    std::cout << "Running testAutoFilter()..." << std::endl;
    SheetData sheet("filter_sheet");
    sheet.setCellText(0, 0, "Fruit", false);
    sheet.setCellText(0, 1, "Price", false);
    sheet.setCellText(1, 0, "Banana", false);
    sheet.setCellText(1, 1, "30", false);
    sheet.setCellText(2, 0, "Apple", false);
    sheet.setCellText(2, 1, "50", false);
    sheet.setCellText(3, 0, "Cherry", false);
    sheet.setCellText(3, 1, "20", false);

    sheet.selector.set(0, 0, 3, 1);
    sheet.autofilterToggle();
    assert(sheet.autoFilter.active() == true);

    // Sort column 0 ascending (Fruit: Apple, Banana, Cherry)
    sheet.autoFilter.setSort(0, "asc");
    sheet.resetAutoFilter();

    assert(sheet.mapVisualToLogicalRow(1) == 2); // Apple was row 2
    assert(sheet.mapVisualToLogicalRow(2) == 1); // Banana was row 1
    assert(sheet.mapVisualToLogicalRow(3) == 3); // Cherry was row 3

    std::cout << "testAutoFilter() PASSED" << std::endl;
}

void testBookAndCSV() {
    std::cout << "Running testBookAndCSV()..." << std::endl;
    SpreadsheetBook book;
    assert(book.sheets.size() == 1);
    book.addSheet("Sheet2");
    assert(book.sheets.size() == 2);

    auto s1 = book.sheets[0];
    s1->setCellText(0, 0, "Name", false);
    s1->setCellText(0, 1, "Age", false);
    s1->setCellText(1, 0, "Alice", false);
    s1->setCellText(1, 1, "30", false);

    std::string csv = book.exportCsv(0);
    assert(csv.find("Name,Age") != std::string::npos);
    assert(csv.find("Alice,30") != std::string::npos);

    // Save and load Workbook
    std::string tempPath = "test_workbook.sheet";
    assert(book.saveToFile(tempPath));

    SpreadsheetBook loaded;
    assert(loaded.loadFromFile(tempPath));
    assert(loaded.sheets.size() == 2);
    assert(loaded.sheets[0]->getCellText(0, 0) == "Name");
    assert(loaded.sheets[0]->getCellText(1, 0) == "Alice");

    std::remove(tempPath.c_str());
    std::cout << "testBookAndCSV() PASSED" << std::endl;
}

void testLocale() {
    std::cout << "Running testLocale()..." << std::endl;
    Locale::setLanguage("en");
    assert(Locale::t("toolbar.undo") == "Undo");
    assert(Locale::t("toolbar.redo") == "Redo");

    Locale::setLanguage("zh-cn");
    assert(Locale::t("toolbar.undo") == "撤销");
    assert(Locale::t("toolbar.redo") == "恢复");

    Locale::setLanguage("de");
    assert(Locale::t("toolbar.undo") == "Rückgängig machen");

    Locale::setLanguage("nl");
    assert(Locale::t("toolbar.undo") == "Ongedaan maken");

    Locale::setLanguage("en");
    std::cout << "testLocale() PASSED" << std::endl;
}

void testPerformanceAndPrecision() {
    std::cout << "Running testPerformanceAndPrecision()..." << std::endl;

    // 1. Precision & Arithmetic verification
    auto lookup = [](int x, int y) -> std::string { return ""; };
    assert(FormulaEngine::eval("=0.1+0.2", lookup) == "0.3");
    assert(FormulaEngine::eval("=1/2", lookup) == "0.5");
    assert(FormulaEngine::eval("=1000*2.5", lookup) == "2500");

    // 2. Select All + Bold: Must NOT allocate 1,000,000 cell objects!
    SheetData sheet("perf_test");
    assert(sheet.rows.data.empty());
    sheet.selector.set(0, 0, sheet.rows.len - 1, sheet.cols.len - 1);
    sheet.setSelectedCellAttr("font-bold", "true");
    // Verify rows.data is still empty (no 1,000,000 dummy cells created!)
    assert(sheet.rows.data.empty());
    // Verify style resolves to bold for any cell in sheet
    assert(sheet.getCellStyle(500, 500).font.bold == true);

    // 3. Formula Caching verification
    sheet.setCellText(0, 0, "=10+20", false);
    assert(sheet.getCellDisplayValue(0, 0) == "30");
    Cell* c = sheet.rows.getCell(0, 0);
    assert(c != nullptr);
    assert(c->evaluatedValue == "30");
    assert(c->isDirty == false);

    std::cout << "testPerformanceAndPrecision() PASSED" << std::endl;
}

void testFormulaPrecision() {
    std::cout << "Running testFormulaPrecision()..." << std::endl;
    auto lookup = [](int x, int y) -> std::string { return ""; };

    // 15 significant digits, like Excel (the old code kept 12)
    assert(FormulaEngine::eval("=1/3", lookup) == "0.333333333333333");
    assert(FormulaEngine::eval("=2/3", lookup) == "0.666666666666667");
    assert(FormulaEngine::eval("=0.1+0.2", lookup) == "0.3");
    assert(FormulaEngine::eval("=1/2", lookup) == "0.5");
    assert(FormulaEngine::eval("=1000*2.5", lookup) == "2500");

    // Scientific notation in formulas
    assert(FormulaEngine::eval("=1e5", lookup) == "100000");
    assert(FormulaEngine::eval("=1E-3", lookup) == "0.001");
    assert(FormulaEngine::eval("=2.5e2", lookup) == "250");
    assert(FormulaEngine::eval("=1e5+1", lookup) == "100001");
    assert(FormulaEngine::eval("=-1.5e-3", lookup) == "-0.0015");

    // Chained rounding: A1 holds 1/3, B1 = A1*3. The error no longer grows
    // the way it did at 12 digits (old result: 0.999999999999). It is still
    // not exactly 1, because the value travels between steps as a rounded
    // string; getting exactly 1 needs the Value-type change (see
    // CHANGES.md, "deliberately not done").
    std::map<std::pair<int, int>, std::string> cells;
    cells[{0, 0}] = "=1/3"; // A1
    auto lookup2 = [&](int x, int y) -> std::string {
        auto it = cells.find({x, y});
        return it != cells.end() ? it->second : "";
    };
    assert(FormulaEngine::eval("=A1*3", lookup2) == "0.999999999999999");

    std::cout << "testFormulaPrecision() PASSED" << std::endl;
}

void testFormulaErrors() {
    std::cout << "Running testFormulaErrors()..." << std::endl;
    std::map<std::pair<int, int>, std::string> cells;
    auto lookup = [&](int x, int y) -> std::string {
        auto it = cells.find({x, y});
        return it != cells.end() ? it->second : "";
    };

    // A cell referring to itself is a cycle, not a deleted reference
    cells[{0, 0}] = "=A1+1"; // A1
    assert(FormulaEngine::eval("=A1", lookup) == "#CIRC!");
    // Mutual cycle
    cells[{2, 0}] = "=D1"; // C1
    cells[{3, 0}] = "=C1"; // D1
    assert(FormulaEngine::eval("=C1", lookup) == "#CIRC!");

    // Text in arithmetic is #VALUE! (Excel), not silent 0 or joining
    assert(FormulaEngine::eval("=\"abc\"-1", lookup) == "#VALUE!");
    assert(FormulaEngine::eval("=\"abc\"*2", lookup) == "#VALUE!");
    assert(FormulaEngine::eval("=\"abc\"+1", lookup) == "#VALUE!");
    assert(FormulaEngine::eval("=\"abc\"/1", lookup) == "#VALUE!");
    assert(FormulaEngine::eval("=\"abc\"/\"def\"", lookup) == "#VALUE!");
    assert(FormulaEngine::eval("=-\"abc\"", lookup) == "#VALUE!");

    // Real division by zero still says so
    assert(FormulaEngine::eval("=1/0", lookup) == "#DIV/0!");

    // Errors pass through instead of being hidden by later steps
    cells[{0, 1}] = "=1/0"; // A2
    assert(FormulaEngine::eval("=A2+1", lookup) == "#DIV/0!");
    assert(FormulaEngine::eval("=A2*5", lookup) == "#DIV/0!");

    std::cout << "testFormulaErrors() PASSED" << std::endl;
}

void testSortPreParsed() {
    std::cout << "Running testSortPreParsed()..." << std::endl;
    SheetData sheet("sort_sheet");
    sheet.setCellText(0, 0, "Item", false);
    sheet.setCellText(0, 1, "Amount", false);
    sheet.setCellText(1, 0, "A", false);
    sheet.setCellText(1, 1, "100", false);
    sheet.setCellText(2, 0, "B", false);
    sheet.setCellText(2, 1, "20", false);
    sheet.setCellText(3, 0, "C", false);
    sheet.setCellText(3, 1, "3", false);
    sheet.setCellText(4, 0, "D", false);
    sheet.setCellText(4, 1, "1000", false);

    sheet.selector.set(0, 0, 4, 1);
    sheet.autofilterToggle();
    sheet.autoFilter.setSort(1, "asc");
    sheet.resetAutoFilter();

    // Numeric order, not alphabetical: 3, 20, 100, 1000
    assert(sheet.mapVisualToLogicalRow(1) == 3);
    assert(sheet.mapVisualToLogicalRow(2) == 2);
    assert(sheet.mapVisualToLogicalRow(3) == 1);
    assert(sheet.mapVisualToLogicalRow(4) == 4);

    // Descending
    sheet.autoFilter.setSort(1, "desc");
    sheet.resetAutoFilter();
    assert(sheet.mapVisualToLogicalRow(1) == 4);
    assert(sheet.mapVisualToLogicalRow(2) == 1);
    assert(sheet.mapVisualToLogicalRow(3) == 2);
    assert(sheet.mapVisualToLogicalRow(4) == 3);

    std::cout << "testSortPreParsed() PASSED" << std::endl;
}

void testCsvSafety() {
    std::cout << "Running testCsvSafety()..." << std::endl;
    SpreadsheetBook book;
    auto s = book.sheets[0];
    s->setCellText(0, 0, "=2+5", false);
    s->setCellText(0, 1, "+cmd", false);
    s->setCellText(0, 2, "-x", false);
    s->setCellText(0, 3, "@x", false);
    s->setCellText(0, 4, "plain", false);
    std::string csv = book.exportCsv(0);
    // Dangerous leading characters are neutralised with a leading quote
    assert(csv.find("'=2+5") != std::string::npos);
    assert(csv.find("'+cmd") != std::string::npos);
    assert(csv.find("'-x") != std::string::npos);
    assert(csv.find("'@x") != std::string::npos);
    // Plain text is untouched
    assert(csv.find(",plain") != std::string::npos);

    // A UTF-8 BOM on import is stripped, not stored inside cell A1
    SpreadsheetBook book2;
    std::string bomCsv = "\xEF\xBB\xBFName,Age\r\nAlice,30\r\n";
    assert(book2.importCsv(bomCsv, 0));
    assert(book2.sheets[0]->getCellText(0, 0) == "Name");
    assert(book2.sheets[0]->getCellText(1, 0) == "Alice");

    // Import grows the grid past the default 1000 rows
    SpreadsheetBook book3;
    std::string big;
    for (int i = 0; i < 1500; ++i) {
        big += "r" + std::to_string(i) + ",1\r\n";
    }
    assert(book3.importCsv(big, 0));
    assert(book3.sheets[0]->rows.len > 1000);
    assert(book3.sheets[0]->getCellText(1499, 0) == "r1499");

    std::cout << "testCsvSafety() PASSED" << std::endl;
}

void testAtomicSaveAndLoadErrors() {
    std::cout << "Running testAtomicSaveAndLoadErrors()..." << std::endl;
    SpreadsheetBook book;
    book.sheets[0]->setCellText(0, 0, "Hello", false);
    std::string path = "test_atomic.sheet";
    std::string tmpPath = path + ".tmp";
    std::remove(tmpPath.c_str());
    assert(book.saveToFile(path));
    // No temp file left behind, and the saved file loads back fine
    std::ifstream probe(tmpPath);
    assert(!probe.is_open());
    SpreadsheetBook loaded;
    assert(loaded.loadFromFile(path));
    assert(loaded.sheets[0]->getCellText(0, 0) == "Hello");
    assert(loaded.lastError().empty());

    // Corrupt file: load fails AND says why
    {
        std::ofstream bad(path, std::ios::trunc);
        bad << "{ this is not json !!!";
    }
    SpreadsheetBook badBook;
    assert(!badBook.loadFromFile(path));
    assert(!badBook.lastError().empty());

    // Missing file: load fails AND says why
    SpreadsheetBook missingBook;
    assert(!missingBook.loadFromFile("no_such_file_xyz.sheet"));
    assert(!missingBook.lastError().empty());

    // Save into a missing directory fails (and reports it)
    SpreadsheetBook book2;
    assert(!book2.saveToFile("no_such_dir_xyz/out.sheet"));
    assert(!book2.lastError().empty());

    std::remove(path.c_str());
    std::remove(tmpPath.c_str());
    std::cout << "testAtomicSaveAndLoadErrors() PASSED" << std::endl;
}

int main() {
    std::cout << "=======================================" << std::endl;
    std::cout << "Running x-spreadsheet C++ Full Test Suite" << std::endl;
    std::cout << "=======================================" << std::endl;

    testAlphabet();
    testCellRange();
    testFormat();
    testFormula();
    testSheetDataAndJSON();
    testValidation();
    testHistory();
    testAutoFilter();
    testBookAndCSV();
    testLocale();
    testPerformanceAndPrecision();
    testFormulaPrecision();
    testFormulaErrors();
    testSortPreParsed();
    testCsvSafety();
    testAtomicSaveAndLoadErrors();

    std::cout << "=======================================" << std::endl;
    std::cout << "ALL 16 TEST SUITES PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "=======================================" << std::endl;
    return 0;
}
