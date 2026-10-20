#pragma once
#include "../core/spreadsheet_book.h"
#include "dialog_validation.h"
#include "dialog_sort_filter.h"
#include "dialog_print.h"
#include <string>
#include <vector>

struct GLFWwindow;
struct ImDrawList;

namespace xspreadsheet {

class SpreadsheetUI {
public:
    SpreadsheetBook book;

    // Scroll offsets
    float scrollX = 0.0f;
    float scrollY = 0.0f;

    // Scrollbar interaction state
    bool isDraggingVScroll = false;
    float dragVScrollMouseStartY = 0.0f;
    float dragVScrollInitialScrollY = 0.0f;

    bool isDraggingHScroll = false;
    float dragHScrollMouseStartX = 0.0f;
    float dragHScrollInitialScrollX = 0.0f;

    // Bottom bar splitter state (sheet tabs vs horizontal scrollbar)
    float bottomBarSplitWidth = 360.0f;
    bool isDraggingSplitter = false;
    float dragSplitterStartX = 0.0f;
    float dragSplitterStartWidth = 360.0f;

    // Resizing state
    bool isResizingCol = false;
    int resizeColIdx = -1;
    float resizeColStartX = 0.0f;
    float resizeColStartWidth = 0.0f;

    bool isResizingRow = false;
    int resizeRowIdx = -1;
    float resizeRowStartY = 0.0f;
    float resizeRowStartHeight = 0.0f;

    // Selection & Autofill dragging state
    bool isSelectingRange = false;
    bool isDraggingAutofill = false;
    CellRange autofillTargetRange;

    // In-place editing state
    bool isEditingCell = false;
    int editingRi = -1;
    int editingCi = -1;
    char cellEditBuf[1024] = "";
    bool cellEditInitialFocus = false;

    // Name Box state
    char nameBoxBuf[64] = "A1";
    bool nameBoxEditing = false;

    // Sheet tab rename state
    int renamingSheetIdx = -1;
    char renameSheetBuf[64] = "";

    // Excel Ribbon & View State
    std::string activeRibbonTab = "Home";
    bool ribbonCollapsed = false;
    bool showGridlines = true;
    bool showHeadings = true;
    bool showFormulaBar = true;
    float zoomScale = 1.0f;
    int activeViewMode = 0; // 0 = Normal, 1 = Page Layout, 2 = Page Break Preview
    bool autoSaveActive = true;
    float autoSaveTimer = 30.0f;
    std::string lastSaveStatus = "";
    float saveStatusTimer = 0.0f;
    char searchBoxBuf[128] = "";

    // Status bar statistic preferences (Excel customizable status bar)
    bool statShowAvg = true;
    bool statShowCount = true;
    bool statShowNumCount = true;
    bool statShowMin = true;
    bool statShowMax = true;
    bool statShowSum = true;

    // Dialogs
    DialogValidation dialogValidation;
    DialogSortFilter dialogSortFilter;
    DialogPrint dialogPrint;

    // File dialogs
    char currentFilePathBuf[256] = "spreadsheet.json";

    // Hybrid rendering state (Method 2: 60 FPS on scroll, event-driven on cell edit, 0 FPS on idle)
    float scrollActivityTimer = 0.5f;
    bool isFastContinuousNeeded() const;
    bool isEditingCellActive() const { return isEditingCell; }

    SpreadsheetUI();

    void render(GLFWwindow* window);

private:
    void renderTitleBar(GLFWwindow* window);
    void renderRibbonTabs();
    void renderRibbonContent();
    void renderRibbonHome();
    void renderRibbonInsert();
    void renderRibbonPageLayout();
    void renderRibbonFormulas();
    void renderRibbonData();
    void renderRibbonReview();
    void renderRibbonView();
    void renderRibbonHelp();

    void renderFormulaBar();
    void renderGridCanvas();
    void renderVerticalScrollbar(ImDrawList* drawList, float x, float y, float width, float height, float totalContentH, float viewH);
    void renderHorizontalScrollbar(ImDrawList* drawList, float x, float y, float width, float height, float totalContentW, float viewW);
    void renderContextMenu();
    void renderBottomBar();
    void renderStatusBar();

    void scrollToCell(int ri, int ci, float availableWidth, float availableHeight, float headerColW, float headerRowH);
    void startCellEditing(int ri, int ci, const std::string& initialText = "");
    void commitCellEditing();
    void cancelCellEditing();

    void calculateSelectionStats(double& outSum, double& outAvg, int& outCount, int& outNumCount, double& outMin, double& outMax);
};

} // namespace xspreadsheet
