#include "spreadsheet_ui.h"
#include "../core/alphabet.h"
#include "../core/formula.h"
#include "../core/format.h"
#include "../locale/locale.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cfloat>

namespace xspreadsheet {

static const float SCROLLBAR_SIZE = 16.0f;
static const float TITLE_BAR_HEIGHT = 30.0f;
static const float RIBBON_TABS_HEIGHT = 28.0f;
static const float RIBBON_BODY_HEIGHT = 74.0f;
static const float FORMULA_BAR_HEIGHT = 28.0f;
static const float BOTTOM_BAR_HEIGHT = 26.0f;
static const float STATUS_BAR_HEIGHT = 22.0f;

// Helper function to parse hex colors like "#ffffff" or "#4b89ff"
static ImU32 parseHexColor(const std::string& hex, ImU32 defaultColor = IM_COL32(255, 255, 255, 255)) {
    if (hex.empty()) return defaultColor;
    std::string s = hex;
    if (s[0] == '#') s = s.substr(1);

    unsigned int r = 255, g = 255, b = 255, a = 255;
    if (s.length() == 6) {
        std::stringstream ss;
        ss << std::hex << s;
        unsigned int val;
        if (ss >> val) {
            r = (val >> 16) & 0xFF;
            g = (val >> 8) & 0xFF;
            b = val & 0xFF;
            return IM_COL32(r, g, b, a);
        }
    } else if (s.length() == 8) {
        std::stringstream ss;
        ss << std::hex << s;
        unsigned int val;
        if (ss >> val) {
            r = (val >> 24) & 0xFF;
            g = (val >> 16) & 0xFF;
            b = (val >> 8) & 0xFF;
            a = val & 0xFF;
            return IM_COL32(r, g, b, a);
        }
    }
    return defaultColor;
}

SpreadsheetUI::SpreadsheetUI() {
    auto sheet = book.activeSheet();

    // Populate sample data
    sheet->setCellText(1, 0, "Quarterly Report", false);
    sheet->setCellText(2, 0, "Item", false);
    sheet->setCellText(2, 1, "Q1", false);
    sheet->setCellText(2, 2, "Q2", false);
    sheet->setCellText(2, 3, "Total", false);

    sheet->setCellText(3, 0, "Hardware", false);
    sheet->setCellText(3, 1, "1200", false);
    sheet->setCellText(3, 2, "1450", false);
    sheet->setCellText(3, 3, "=SUM(B4:C4)", false);

    sheet->setCellText(4, 0, "Software", false);
    sheet->setCellText(4, 1, "850", false);
    sheet->setCellText(4, 2, "920", false);
    sheet->setCellText(4, 3, "=SUM(B5:C5)", false);

    sheet->setCellText(5, 0, "Services", false);
    sheet->setCellText(5, 1, "430", false);
    sheet->setCellText(5, 2, "610", false);
    sheet->setCellText(5, 3, "=SUM(B6:C6)", false);

    // Header styling
    sheet->selector.set(1, 0, 1, 3);
    sheet->merge();
    sheet->setSelectedCellAttr("font-bold", "true");
    sheet->setSelectedCellAttr("font-size", "14");

    sheet->selector.set(2, 0, 2, 3);
    sheet->setSelectedCellAttr("font-bold", "true");
    sheet->setSelectedCellAttr("bgcolor", "#f2f4f8");

    BorderStyle bs;
    bs.style = "thin";
    bs.color = "#b0b8c4";
    sheet->selector.set(2, 0, 5, 3);
    sheet->setSelectedCellBorder(bs, "all");

    sheet->selector.set(0, 0, 0, 0);
}

void SpreadsheetUI::render(GLFWwindow* window) {
    float dt = ImGui::GetIO().DeltaTime;
    if (scrollActivityTimer > 0.0f) {
        scrollActivityTimer = std::max(0.0f, scrollActivityTimer - dt);
    }
    if (saveStatusTimer > 0.0f) {
        saveStatusTimer -= dt;
    }
    if (autoSaveActive) {
        autoSaveTimer -= dt;
        if (autoSaveTimer <= 0.0f) {
            autoSaveTimer = 30.0f;
            if (book.saveToFile(currentFilePathBuf)) {
                lastSaveStatus = "AutoSaved";
                saveStatusTimer = 3.0f;
            }
        }
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags hostWindowFlags = ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus |
                                       ImGuiWindowFlags_NoNavFocus |
                                       ImGuiWindowFlags_NoScrollbar |
                                       ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("SpreadsheetMainWindow", nullptr, hostWindowFlags);
    ImGui::PopStyleVar(3);

    // 1. Excel Top Title Bar with Quick Access Toolbar
    renderTitleBar(window);

    // 2. Excel Ribbon Tabs
    renderRibbonTabs();

    // 3. Excel Ribbon Commands Area
    if (!ribbonCollapsed) {
        renderRibbonContent();
    }

    // 4. Excel Formula Bar with Name Box & fx
    if (showFormulaBar) {
        renderFormulaBar();
    }

    // 5. Excel Spreadsheet Grid with Vertical Scrollbar
    renderGridCanvas();

    // 6. Excel Bottom Bar with Sheet Navigation, Tabs, Splitter & Horizontal Scrollbar
    renderBottomBar();

    // 7. Excel Status Bar with Ready, Stats, View Modes & Zoom
    renderStatusBar();

    // Dialogs / Modals
    auto sheet = book.activeSheet();
    dialogValidation.render(*sheet);
    dialogSortFilter.render(*sheet);
    dialogPrint.render(*sheet);

    ImGui::End();
}

void SpreadsheetUI::renderTitleBar(GLFWwindow* window) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.98f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 3.0f));
    ImGui::BeginChild("ExcelTitleBar", ImVec2(0, TITLE_BAR_HEIGHT), false, ImGuiWindowFlags_NoScrollbar);

    auto sheet = book.activeSheet();

    // AutoSave Toggle
    if (autoSaveActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.35f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.85f, 0.88f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    }
    std::string autoSaveLabel = autoSaveActive ? "AutoSave: On##AutoSaveBtn" : "AutoSave: Off##AutoSaveBtn";
    if (ImGui::Button(autoSaveLabel.c_str())) {
        autoSaveActive = !autoSaveActive;
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle AutoSave (Every 30s)");

    ImGui::SameLine();

    // Quick Access Save
    if (ImGui::Button("Save##QuickSaveBtn")) {
        if (book.saveToFile(currentFilePathBuf)) {
            lastSaveStatus = "Saved successfully";
            saveStatusTimer = 3.0f;
        } else {
            lastSaveStatus = "Save failed!";
            saveStatusTimer = 5.0f;
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save (Ctrl+S)");

    ImGui::SameLine();

    // Quick Access Undo
    bool canUndo = sheet->canUndo();
    if (!canUndo) ImGui::BeginDisabled();
    if (ImGui::Button("Undo##QuickUndoBtn")) {
        sheet->undo();
    }
    if (!canUndo) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo (Ctrl+Z)");

    ImGui::SameLine();

    // Quick Access Redo
    bool canRedo = sheet->canRedo();
    if (!canRedo) ImGui::BeginDisabled();
    if (ImGui::Button("Redo##QuickRedoBtn")) {
        sheet->redo();
    }
    if (!canRedo) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Redo (Ctrl+Y)");

    ImGui::SameLine(0, 16);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0, 16);

    // Document Title
    std::string docTitle = currentFilePathBuf;
    if (docTitle.empty()) docTitle = "Book1";
    size_t lastSlash = docTitle.find_last_of("/\\");
    if (lastSlash != std::string::npos) docTitle = docTitle.substr(lastSlash + 1);
    std::string titleStr = docTitle + " - Excel";

    float titleWidth = ImGui::CalcTextSize(titleStr.c_str()).x;
    float centerPos = (ImGui::GetWindowWidth() - titleWidth) * 0.5f;
    if (centerPos > ImGui::GetCursorPosX()) {
        ImGui::SameLine(centerPos);
    }
    ImGui::TextColored(ImVec4(0.2f, 0.2f, 0.25f, 1.0f), "%s", titleStr.c_str());

    // Search Box on right side
    float searchWidth = 180.0f;
    float rightControlsWidth = searchWidth + 24.0f;
    if (ImGui::GetWindowWidth() > centerPos + titleWidth + rightControlsWidth) {
        ImGui::SameLine(ImGui::GetWindowWidth() - rightControlsWidth);
        ImGui::SetNextItemWidth(searchWidth);
        ImGui::InputTextWithHint("##ExcelSearchBox", "Search (Alt+Q)", searchBoxBuf, sizeof(searchBoxBuf));
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void SpreadsheetUI::renderRibbonTabs() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.94f, 0.94f, 0.96f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));
    ImGui::BeginChild("ExcelRibbonTabsBar", ImVec2(0, RIBBON_TABS_HEIGHT), false, ImGuiWindowFlags_NoScrollbar);

    // File Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.35f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button("File##FileMenuBtn", ImVec2(52, 22))) {
        ImGui::OpenPopup("FileMenuPopup");
    }
    ImGui::PopStyleColor(3);

    if (ImGui::BeginPopup("FileMenuPopup")) {
        if (ImGui::MenuItem("New Workbook", "Ctrl+N")) {
            book = SpreadsheetBook();
        }
        if (ImGui::MenuItem("Open...", "Ctrl+O")) {
            ImGui::OpenPopup("OpenFilePopup");
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            book.saveToFile(currentFilePathBuf);
        }
        if (ImGui::MenuItem("Save As...")) {
            ImGui::OpenPopup("SaveAsPopup");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Export to CSV...")) {
            dialogPrint.open();
        }
        if (ImGui::MenuItem("Print Preview...", "Ctrl+P")) {
            dialogPrint.open();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Close")) {
            // reset book
            book = SpreadsheetBook();
        }
        ImGui::EndPopup();
    }

    // Open File & Save As Popups
    if (ImGui::BeginPopup("OpenFilePopup")) {
        ImGui::Text("Enter file path:");
        ImGui::InputText("##OpenFilePath", currentFilePathBuf, sizeof(currentFilePathBuf));
        if (ImGui::Button("Load JSON", ImVec2(100, 0))) {
            book.loadFromFile(currentFilePathBuf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("SaveAsPopup")) {
        ImGui::Text("Save to file path:");
        ImGui::InputText("##SaveFilePath", currentFilePathBuf, sizeof(currentFilePathBuf));
        if (ImGui::Button("Save JSON", ImVec2(100, 0))) {
            book.saveToFile(currentFilePathBuf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Standard Excel Tabs
    const char* tabs[] = { "Home", "Insert", "Page Layout", "Formulas", "Data", "Review", "View", "Help" };
    for (int i = 0; i < 8; ++i) {
        bool isActive = (activeRibbonTab == tabs[i]);
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.12f, 0.40f, 0.85f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.94f, 0.94f, 0.96f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.90f, 0.94f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        }

        if (ImGui::Button(tabs[i], ImVec2(0, 22))) {
            activeRibbonTab = tabs[i];
            if (ribbonCollapsed) ribbonCollapsed = false;
        }

        // Draw active bottom accent line
        if (isActive) {
            ImVec2 minP = ImGui::GetItemRectMin();
            ImVec2 maxP = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(minP.x + 2, maxP.y - 1),
                ImVec2(maxP.x - 2, maxP.y - 1),
                IM_COL32(30, 100, 215, 255),
                2.5f
            );
        }

        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    }

    // Ribbon Pin / Collapse Button on Far Right
    float availW = ImGui::GetContentRegionAvail().x;
    if (availW > 30.0f) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 32.0f);
        if (ImGui::Button(ribbonCollapsed ? " v ##ToggleRibbon" : " ^ ##ToggleRibbon", ImVec2(24, 22))) {
            ribbonCollapsed = !ribbonCollapsed;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(ribbonCollapsed ? "Show Ribbon" : "Collapse Ribbon (Ctrl+F1)");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void SpreadsheetUI::renderRibbonContent() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
    ImGui::BeginChild("ExcelRibbonBody", ImVec2(0, RIBBON_BODY_HEIGHT), false, ImGuiWindowFlags_NoScrollbar);

    if (activeRibbonTab == "Home") {
        renderRibbonHome();
    } else if (activeRibbonTab == "Insert") {
        renderRibbonInsert();
    } else if (activeRibbonTab == "Page Layout") {
        renderRibbonPageLayout();
    } else if (activeRibbonTab == "Formulas") {
        renderRibbonFormulas();
    } else if (activeRibbonTab == "Data") {
        renderRibbonData();
    } else if (activeRibbonTab == "Review") {
        renderRibbonReview();
    } else if (activeRibbonTab == "View") {
        renderRibbonView();
    } else if (activeRibbonTab == "Help") {
        renderRibbonHelp();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void SpreadsheetUI::renderRibbonHome() {
    auto sheet = book.activeSheet();
    Style curStyle = sheet->getSelectedCellStyle();

    // Group 1: Clipboard
    ImGui::BeginGroup();
    if (ImGui::Button("Paste##RbnPaste", ImVec2(44, 46))) {
        sheet->paste("all");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Paste (Ctrl+V)");

    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ImGui::Button("Cut##RbnCut", ImVec2(48, 22))) {
        sheet->cut();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cut (Ctrl+X)");

    if (ImGui::Button("Copy##RbnCopy", ImVec2(48, 22))) {
        sheet->copy();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy (Ctrl+C)");
    ImGui::EndGroup();

    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Clipboard");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Group 2: Font
    ImGui::BeginGroup();
    // Row 1: Font Family & Size
    ImGui::SetNextItemWidth(100);
    const char* fontNames[] = { "Calibri", "Arial", "Segoe UI", "Comic Sans MS", "Courier New", "Verdana", "Tahoma" };
    int curFontIdx = 0;
    for (int i = 0; i < 7; ++i) {
        if (curStyle.font.name == fontNames[i]) { curFontIdx = i; break; }
    }
    if (ImGui::Combo("##FontFamilyCombo", &curFontIdx, fontNames, 7)) {
        sheet->setSelectedCellAttr("font-name", fontNames[curFontIdx]);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    const char* fontSizes[] = { "8", "9", "10", "11", "12", "14", "16", "18", "20", "24", "28", "36" };
    int curSizeIdx = 3; // 11
    int curPt = static_cast<int>(std::round(curStyle.font.size));
    for (int i = 0; i < 12; ++i) {
        if (curPt == std::stoi(fontSizes[i])) { curSizeIdx = i; break; }
    }
    if (ImGui::Combo("##FontSizeCombo", &curSizeIdx, fontSizes, 12)) {
        sheet->setSelectedCellAttr("font-size", fontSizes[curSizeIdx]);
    }

    ImGui::SameLine();
    if (ImGui::Button("A^##IncFont", ImVec2(24, 22))) {
        int nextPt = std::min(36, curPt + 2);
        sheet->setSelectedCellAttr("font-size", std::to_string(nextPt));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Increase Font Size");

    ImGui::SameLine();
    if (ImGui::Button("Av##DecFont", ImVec2(24, 22))) {
        int nextPt = std::max(8, curPt - 2);
        sheet->setSelectedCellAttr("font-size", std::to_string(nextPt));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Decrease Font Size");

    // Row 2: B, I, U, S, Borders, Fill Color, Font Color
    if (curStyle.font.bold) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.85f, 0.6f));
    if (ImGui::Button(" B ##BoldBtn", ImVec2(24, 22))) {
        sheet->setSelectedCellAttr("font-bold", curStyle.font.bold ? "false" : "true");
    }
    if (curStyle.font.bold) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bold (Ctrl+B)");

    ImGui::SameLine();
    if (curStyle.font.italic) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.85f, 0.6f));
    if (ImGui::Button(" I ##ItalicBtn", ImVec2(24, 22))) {
        sheet->setSelectedCellAttr("font-italic", curStyle.font.italic ? "false" : "true");
    }
    if (curStyle.font.italic) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Italic (Ctrl+I)");

    ImGui::SameLine();
    if (curStyle.underline) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.85f, 0.6f));
    if (ImGui::Button(" U ##UnderlineBtn", ImVec2(24, 22))) {
        sheet->setSelectedCellAttr("underline", curStyle.underline ? "false" : "true");
    }
    if (curStyle.underline) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Underline (Ctrl+U)");

    ImGui::SameLine();
    if (curStyle.strike) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.85f, 0.6f));
    if (ImGui::Button("ab##StrikeBtn", ImVec2(24, 22))) {
        sheet->setSelectedCellAttr("strike", curStyle.strike ? "false" : "true");
    }
    if (curStyle.strike) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Strikethrough");

    ImGui::SameLine();
    if (ImGui::Button("田##BordersBtn", ImVec2(24, 22))) {
        ImGui::OpenPopup("BordersPopup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Borders");
    if (ImGui::BeginPopup("BordersPopup")) {
        BorderStyle bs;
        bs.style = "thin";
        bs.color = "#000000";
        if (ImGui::Selectable("Bottom Border")) { sheet->setSelectedCellBorder(bs, "bottom"); ImGui::CloseCurrentPopup(); }
        if (ImGui::Selectable("Top Border")) { sheet->setSelectedCellBorder(bs, "top"); ImGui::CloseCurrentPopup(); }
        if (ImGui::Selectable("Left Border")) { sheet->setSelectedCellBorder(bs, "left"); ImGui::CloseCurrentPopup(); }
        if (ImGui::Selectable("Right Border")) { sheet->setSelectedCellBorder(bs, "right"); ImGui::CloseCurrentPopup(); }
        ImGui::Separator();
        if (ImGui::Selectable("No Border")) { sheet->setSelectedCellBorder(bs, "none"); ImGui::CloseCurrentPopup(); }
        if (ImGui::Selectable("All Borders")) { sheet->setSelectedCellBorder(bs, "all"); ImGui::CloseCurrentPopup(); }
        if (ImGui::Selectable("Outside Borders")) { sheet->setSelectedCellBorder(bs, "outside"); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Fill##FillBtn", ImVec2(34, 22))) {
        ImGui::OpenPopup("FillColorPopup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fill Color");
    if (ImGui::BeginPopup("FillColorPopup")) {
        const char* fillPalette[] = {
            "#ffffff", "#f2f2f2", "#d9d9d9", "#bfbfbf", "#a6a6a6", "#7f7f7f", "#595959", "#000000",
            "#f4cccc", "#fce5cd", "#fff2cc", "#d9ead3", "#d0e0e3", "#cfe2f3", "#d9d2e9", "#ead1dc",
            "#ea9999", "#f9cb9c", "#ffe599", "#b6d7a8", "#a2c4c9", "#9fc5e8", "#b4a7d6", "#d5a6bd"
        };
        for (int i = 0; i < 24; ++i) {
            ImU32 col = parseHexColor(fillPalette[i]);
            ImGui::PushID(i + 100);
            if (ImGui::ColorButton("##FillColBtn", ImColor(col), ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18))) {
                sheet->setSelectedCellAttr("bgcolor", fillPalette[i]);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            if ((i + 1) % 8 != 0) ImGui::SameLine();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(" A ##TextColorBtn", ImVec2(24, 22))) {
        ImGui::OpenPopup("TextColorPopup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Font Color");
    if (ImGui::BeginPopup("TextColorPopup")) {
        const char* colorPalette[] = {
            "#000000", "#434343", "#666666", "#999999", "#b7b7b7", "#cccccc", "#d9d9d9", "#ffffff",
            "#980000", "#ff0000", "#ff9900", "#ffff00", "#00ff00", "#00ffff", "#4a86e8", "#0000ff",
            "#9900ff", "#ff00ff", "#e6b8af", "#f4cccc", "#fce5cd", "#fff2cc", "#d9ead3", "#d0e0e3"
        };
        for (int i = 0; i < 24; ++i) {
            ImU32 col = parseHexColor(colorPalette[i]);
            ImGui::PushID(i);
            if (ImGui::ColorButton("##TxtColBtn", ImColor(col), ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18))) {
                sheet->setSelectedCellAttr("color", colorPalette[i]);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            if ((i + 1) % 8 != 0) ImGui::SameLine();
        }
        ImGui::EndPopup();
    }

    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Font");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Group 3: Alignment
    ImGui::BeginGroup();
    // Row 1: Top, Middle, Bottom align + Wrap Text
    if (ImGui::Button("Top##TopAlignBtn", ImVec2(28, 22))) {
        sheet->setSelectedCellAttr("valign", "top");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Top Align");

    ImGui::SameLine();
    if (ImGui::Button("Mid##MidAlignBtn", ImVec2(28, 22))) {
        sheet->setSelectedCellAttr("valign", "middle");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Middle Align");

    ImGui::SameLine();
    if (ImGui::Button("Bot##BotAlignBtn", ImVec2(28, 22))) {
        sheet->setSelectedCellAttr("valign", "bottom");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bottom Align");

    ImGui::SameLine();
    if (ImGui::Button("Wrap Text##WrapTextBtn", ImVec2(68, 22))) {
        sheet->setSelectedCellAttr("textwrap", curStyle.textwrap ? "false" : "true");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Wrap Text");

    // Row 2: Left, Center, Right align + Merge & Center
    if (ImGui::Button("Left##LeftAlignBtn", ImVec2(28, 22))) {
        sheet->setSelectedCellAttr("align", "left");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Align Left");

    ImGui::SameLine();
    if (ImGui::Button("Ctr##CenterAlignBtn", ImVec2(28, 22))) {
        sheet->setSelectedCellAttr("align", "center");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Center");

    ImGui::SameLine();
    if (ImGui::Button("Rgt##RightAlignBtn", ImVec2(28, 22))) {
        sheet->setSelectedCellAttr("align", "right");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Align Right");

    ImGui::SameLine();
    bool isMerged = sheet->canUnmerge();
    if (isMerged) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.85f, 0.6f));
    if (ImGui::Button("Merge & Center##MergeCenterBtn", ImVec2(98, 22))) {
        if (isMerged) sheet->unmerge();
        else sheet->merge();
    }
    if (isMerged) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Merge & Center");

    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Alignment");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Group 4: Number Formats
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(100);
    const char* formats[] = { "normal", "text", "number", "percent", "rmb", "usd", "eur", "date", "time", "datetime" };
    const char* formatLabels[] = { "General", "Plain Text", "Number", "Percentage", "Currency (￥)", "Currency ($)", "Currency (€)", "Short Date", "Time", "Date Time" };
    int curFmtIdx = 0;
    for (int i = 0; i < 10; ++i) {
        if (curStyle.format == formats[i]) { curFmtIdx = i; break; }
    }
    if (ImGui::Combo("##NumFormatCombo", &curFmtIdx, formatLabels, 10)) {
        sheet->setSelectedCellAttr("format", formats[curFmtIdx]);
    }

    // Number quick buttons: $, %, ,, .0, .00
    if (ImGui::Button("$##FmtUSD", ImVec2(20, 22))) {
        sheet->setSelectedCellAttr("format", "usd");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Currency ($)");

    ImGui::SameLine();
    if (ImGui::Button("%##FmtPct", ImVec2(20, 22))) {
        sheet->setSelectedCellAttr("format", "percent");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Percent (%)");

    ImGui::SameLine();
    if (ImGui::Button(",##FmtComma", ImVec2(20, 22))) {
        sheet->setSelectedCellAttr("format", "number");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Comma Style");

    ImGui::SameLine();
    if (ImGui::Button(".00##IncDec", ImVec2(24, 22))) {
        sheet->setSelectedCellAttr("format", "number");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Increase Decimals");

    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Number");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Group 5: Cells (Insert, Delete, Format)
    ImGui::BeginGroup();
    if (ImGui::Button("Insert##CellsInsertBtn", ImVec2(48, 22))) {
        ImGui::OpenPopup("InsertCellsPopup");
    }
    if (ImGui::BeginPopup("InsertCellsPopup")) {
        if (ImGui::MenuItem("Insert Sheet Rows")) sheet->insert("row", 1);
        if (ImGui::MenuItem("Insert Sheet Columns")) sheet->insert("column", 1);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete##CellsDeleteBtn", ImVec2(48, 22))) {
        ImGui::OpenPopup("DeleteCellsPopup");
    }
    if (ImGui::BeginPopup("DeleteCellsPopup")) {
        if (ImGui::MenuItem("Delete Sheet Rows")) sheet->deleteSelected("row");
        if (ImGui::MenuItem("Delete Sheet Columns")) sheet->deleteSelected("column");
        if (ImGui::MenuItem("Clear Cell Text")) sheet->deleteCellText();
        ImGui::EndPopup();
    }

    if (ImGui::Button("Format##CellsFmtBtn", ImVec2(100, 22))) {
        ImGui::OpenPopup("FormatCellsPopup");
    }
    if (ImGui::BeginPopup("FormatCellsPopup")) {
        if (ImGui::MenuItem("AutoFit Row Height")) {
            sheet->rows.setHeight(sheet->select_ri, sheet->rows.default_height);
        }
        if (ImGui::MenuItem("AutoFit Column Width")) {
            sheet->cols.setWidth(sheet->select_ci, sheet->cols.default_width);
        }
        ImGui::EndPopup();
    }

    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Cells");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Group 6: Editing (AutoSum, Clear, Sort & Filter)
    ImGui::BeginGroup();
    if (ImGui::Button("Σ AutoSum##AutoSumBtn", ImVec2(76, 22))) {
        sheet->setSelectedCellAttr("formula", "SUM");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("AutoSum (Alt+=)");

    ImGui::SameLine();
    if (ImGui::Button("Clear##ClearBtn", ImVec2(44, 22))) {
        ImGui::OpenPopup("ClearPopup");
    }
    if (ImGui::BeginPopup("ClearPopup")) {
        if (ImGui::MenuItem("Clear All")) {
            sheet->deleteCellText();
            sheet->setSelectedCellAttr("bgcolor", "#ffffff");
            BorderStyle bs; bs.style = "none";
            sheet->setSelectedCellBorder(bs, "none");
        }
        if (ImGui::MenuItem("Clear Formats")) {
            sheet->setSelectedCellAttr("bgcolor", "#ffffff");
            sheet->setSelectedCellAttr("font-bold", "false");
            sheet->setSelectedCellAttr("font-italic", "false");
        }
        if (ImGui::MenuItem("Clear Contents")) {
            sheet->deleteCellText();
        }
        ImGui::EndPopup();
    }

    bool filterActive = sheet->autoFilter.active();
    if (filterActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.85f, 0.6f));
    if (ImGui::Button("Sort & Filter##SortFilterBtn", ImVec2(80, 22))) {
        ImGui::OpenPopup("SortFilterPopup");
    }
    if (filterActive) ImGui::PopStyleColor();
    if (ImGui::BeginPopup("SortFilterPopup")) {
        if (ImGui::MenuItem("Toggle Filter", nullptr, filterActive)) {
            sheet->autofilterToggle();
        }
        if (ImGui::MenuItem("Sort A to Z")) {
            dialogSortFilter.open(*sheet, sheet->select_ci);
        }
        if (ImGui::MenuItem("Sort Z to A")) {
            dialogSortFilter.open(*sheet, sheet->select_ci);
        }
        ImGui::EndPopup();
    }

    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Editing");
    ImGui::EndGroup();
}

void SpreadsheetUI::renderRibbonInsert() {
    auto sheet = book.activeSheet();

    ImGui::BeginGroup();
    if (ImGui::Button("PivotTable##InsertPT", ImVec2(68, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Table##InsertTbl", ImVec2(48, 44))) {}
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Tables");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("Pictures##InsertPic", ImVec2(60, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Shapes##InsertShape", ImVec2(54, 44))) {}
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Illustrations");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("Bar Chart##InsertBar", ImVec2(62, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Line Chart##InsertLine", ImVec2(66, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Pie Chart##InsertPie", ImVec2(62, 44))) {}
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Charts");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("Link##InsertLink", ImVec2(44, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Text Box##InsertText", ImVec2(60, 44))) {}
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Text & Links");
    ImGui::EndGroup();
}

void SpreadsheetUI::renderRibbonPageLayout() {
    ImGui::BeginGroup();
    if (ImGui::Button("Margins##PgMargins", ImVec2(56, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Orientation##PgOrient", ImVec2(76, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Size##PgSize", ImVec2(44, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Print Area##PgPrintArea", ImVec2(68, 44))) {
        dialogPrint.open();
    }
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Page Setup");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::Checkbox("Gridlines: View", &showGridlines);
    ImGui::Checkbox("Headings: View", &showHeadings);
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Sheet Options");
    ImGui::EndGroup();
}

void SpreadsheetUI::renderRibbonFormulas() {
    auto sheet = book.activeSheet();

    ImGui::BeginGroup();
    if (ImGui::Button("fx Insert Function", ImVec2(110, 44))) {
        ImGui::OpenPopup("InsertFuncPopup");
    }
    if (ImGui::BeginPopup("InsertFuncPopup")) {
        for (const auto& f : FormulaEngine::getBaseFormulas()) {
            if (ImGui::Selectable(f.key.c_str())) {
                sheet->setSelectedCellAttr("formula", f.key);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("AutoSum##FormulasAutoSum", ImVec2(64, 44))) {
        sheet->setSelectedCellAttr("formula", "SUM");
    }

    ImGui::SameLine();
    if (ImGui::Button("Financial##FormulasFin", ImVec2(64, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Logical##FormulasLog", ImVec2(56, 44))) {
        sheet->setSelectedCellAttr("formula", "IF");
    }
    ImGui::SameLine();
    if (ImGui::Button("Text##FormulasTxt", ImVec2(48, 44))) {
        sheet->setSelectedCellAttr("formula", "CONCAT");
    }
    ImGui::SameLine();
    if (ImGui::Button("Math & Trig##FormulasMath", ImVec2(80, 44))) {}

    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Function Library");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("Calculate Now (F9)##CalcNowBtn", ImVec2(116, 44))) {
        // Trigger sheet re-evaluation
        sheet->rows.len = sheet->rows.len;
    }
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Calculation");
    ImGui::EndGroup();
}

void SpreadsheetUI::renderRibbonData() {
    auto sheet = book.activeSheet();

    ImGui::BeginGroup();
    if (ImGui::Button("From CSV##DataFromCSV", ImVec2(70, 44))) {
        ImGui::OpenPopup("OpenFilePopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh All##DataRefresh", ImVec2(74, 44))) {}
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Queries & Connections");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("Sort A-Z##DataSortAZ", ImVec2(60, 44))) {
        dialogSortFilter.open(*sheet, sheet->select_ci);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sort Z-A##DataSortZA", ImVec2(60, 44))) {
        dialogSortFilter.open(*sheet, sheet->select_ci);
    }
    ImGui::SameLine();
    bool fActive = sheet->autoFilter.active();
    if (fActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.85f, 0.6f));
    if (ImGui::Button("Filter##DataFilterBtn", ImVec2(52, 44))) {
        sheet->autofilterToggle();
    }
    if (fActive) ImGui::PopStyleColor();

    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Sort & Filter");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("Data Validation##DataValBtn", ImVec2(96, 44))) {
        dialogValidation.open(*sheet);
    }
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Data Tools");
    ImGui::EndGroup();
}

void SpreadsheetUI::renderRibbonReview() {
    ImGui::BeginGroup();
    if (ImGui::Button("Spelling##RevSpell", ImVec2(60, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Thesaurus##RevThes", ImVec2(68, 44))) {}
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Proofing");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("New Comment##RevComment", ImVec2(88, 44))) {}
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Comments");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("Protect Sheet##RevProtect", ImVec2(84, 44))) {}
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Protect");
    ImGui::EndGroup();
}

void SpreadsheetUI::renderRibbonView() {
    auto sheet = book.activeSheet();

    ImGui::BeginGroup();
    if (ImGui::Button("Normal##ViewNorm", ImVec2(52, 44))) { activeViewMode = 0; }
    ImGui::SameLine();
    if (ImGui::Button("Page Break##ViewPB", ImVec2(74, 44))) { activeViewMode = 2; }
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Workbook Views");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::Checkbox("Formula Bar", &showFormulaBar);
    ImGui::Checkbox("Headings", &showHeadings);
    ImGui::Checkbox("Gridlines", &showGridlines);
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Show");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button("100% Zoom##View100", ImVec2(68, 44))) { zoomScale = 1.0f; }
    ImGui::SameLine();
    if (ImGui::Button("Zoom to Sel##ViewZoomSel", ImVec2(78, 44))) { zoomScale = 1.25f; }
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Zoom");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::BeginGroup();
    bool freezeActive = sheet->freezeIsActive();
    if (freezeActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.85f, 0.6f));
    if (ImGui::Button("Freeze Panes##ViewFreeze", ImVec2(88, 44))) {
        if (freezeActive) sheet->setFreeze(0, 0);
        else sheet->setFreeze(sheet->select_ri, sheet->select_ci);
    }
    if (freezeActive) ImGui::PopStyleColor();
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Window");
    ImGui::EndGroup();
}

void SpreadsheetUI::renderRibbonHelp() {
    ImGui::BeginGroup();
    if (ImGui::Button("Help##HelpBtn", ImVec2(48, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Feedback##HelpFeedback", ImVec2(62, 44))) {}
    ImGui::SameLine();
    if (ImGui::Button("Shortcuts##HelpShortcuts", ImVec2(66, 44))) {
        ImGui::OpenPopup("ShortcutsPopup");
    }
    if (ImGui::BeginPopup("ShortcutsPopup")) {
        ImGui::Text("Excel Keyboard Shortcuts:");
        ImGui::Separator();
        ImGui::BulletText("F2: Edit Active Cell");
        ImGui::BulletText("Enter: Commit Edit & Move Down");
        ImGui::BulletText("Tab: Commit Edit & Move Right");
        ImGui::BulletText("Shift+Enter / Shift+Tab: Move Up / Left");
        ImGui::BulletText("Ctrl+Z / Ctrl+Y: Undo / Redo");
        ImGui::BulletText("Ctrl+C / Ctrl+X / Ctrl+V: Copy / Cut / Paste");
        ImGui::BulletText("Ctrl+B / Ctrl+I / Ctrl+U: Bold / Italic / Underline");
        ImGui::BulletText("Ctrl+A: Select All");
        ImGui::BulletText("Del / Backspace: Clear Contents");
        ImGui::BulletText("Home / Ctrl+Home: Column A / Cell A1");
        ImGui::EndPopup();
    }
    ImGui::SetCursorPosY(52);
    ImGui::TextDisabled("Help & Support");
    ImGui::EndGroup();
}

void SpreadsheetUI::renderFormulaBar() {
    auto sheet = book.activeSheet();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.97f, 0.97f, 0.98f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
    ImGui::BeginChild("ExcelFormulaBarZone", ImVec2(0, FORMULA_BAR_HEIGHT), false, ImGuiWindowFlags_NoScrollbar);

    // 1. Name Box (shows cell reference or selection size, allows jumping)
    if (!nameBoxEditing) {
        if (sheet->selector.multiple()) {
            int rows = sheet->selector.rowCount();
            int cols = sheet->selector.colCount();
            snprintf(nameBoxBuf, sizeof(nameBoxBuf), "%dR x %dC", rows, cols);
        } else {
            std::string ref = xy2expr(sheet->select_ci, sheet->select_ri);
            snprintf(nameBoxBuf, sizeof(nameBoxBuf), "%s", ref.c_str());
        }
    }

    ImGui::SetNextItemWidth(74.0f);
    if (ImGui::InputText("##NameBoxInput", nameBoxBuf, sizeof(nameBoxBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string target = nameBoxBuf;
        CellRange range = CellRange::valueOf(target);
        if (range.sri >= 0 && range.sci >= 0) {
            sheet->select_ri = range.sri;
            sheet->select_ci = range.sci;
            sheet->selector = range;
        }
        nameBoxEditing = false;
    }
    if (ImGui::IsItemActive()) {
        nameBoxEditing = true;
    } else {
        nameBoxEditing = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name Box");

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // 2. Action Buttons: Cancel [X], Enter [Check], fx
    bool editingActive = isEditingCell;

    if (!editingActive) ImGui::BeginDisabled();
    if (ImGui::Button(" X ##FormulaCancelBtn", ImVec2(22, 22))) {
        cancelCellEditing();
    }
    if (!editingActive) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel (Esc)");

    ImGui::SameLine();
    if (!editingActive) ImGui::BeginDisabled();
    if (ImGui::Button(" V ##FormulaEnterBtn", ImVec2(22, 22))) {
        commitCellEditing();
    }
    if (!editingActive) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enter (Commit)");

    ImGui::SameLine();
    if (ImGui::Button("fx##FormulaInsertFuncBtn", ImVec2(26, 22))) {
        ImGui::OpenPopup("InsertFuncPopupBar");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Insert Function");
    if (ImGui::BeginPopup("InsertFuncPopupBar")) {
        for (const auto& f : FormulaEngine::getBaseFormulas()) {
            if (ImGui::Selectable(f.key.c_str())) {
                startCellEditing(sheet->select_ri, sheet->select_ci, "=" + f.key + "()");
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // 3. Formula Input Field
    std::string curText = sheet->getCellText(sheet->select_ri, sheet->select_ci);
    static char formulaBarBuf[1024];
    if (!isEditingCell) {
        strncpy(formulaBarBuf, curText.c_str(), sizeof(formulaBarBuf) - 1);
        formulaBarBuf[sizeof(formulaBarBuf) - 1] = '\0';
    }

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##FormulaBarLiveInput", formulaBarBuf, sizeof(formulaBarBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        sheet->setCellText(sheet->select_ri, sheet->select_ci, formulaBarBuf, true);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        sheet->setCellText(sheet->select_ri, sheet->select_ci, formulaBarBuf, true);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void SpreadsheetUI::startCellEditing(int ri, int ci, const std::string& initialText) {
    auto sheet = book.activeSheet();
    isEditingCell = true;
    editingRi = ri;
    editingCi = ci;

    std::string text = initialText.empty() ? sheet->getCellText(ri, ci) : initialText;
    strncpy(cellEditBuf, text.c_str(), sizeof(cellEditBuf) - 1);
    cellEditBuf[sizeof(cellEditBuf) - 1] = '\0';
    cellEditInitialFocus = true;
}

void SpreadsheetUI::commitCellEditing() {
    if (!isEditingCell) return;
    auto sheet = book.activeSheet();
    sheet->setCellText(editingRi, editingCi, cellEditBuf, true);
    isEditingCell = false;
    editingRi = -1;
    editingCi = -1;
}

void SpreadsheetUI::cancelCellEditing() {
    isEditingCell = false;
    editingRi = -1;
    editingCi = -1;
}

void SpreadsheetUI::scrollToCell(int ri, int ci, float availableWidth, float availableHeight, float headerColW, float headerRowH) {
    auto sheet = book.activeSheet();

    float targetLeft = 0.0f;
    for (int c = 0; c < ci; ++c) {
        targetLeft += sheet->cols.getWidth(c) * zoomScale;
    }
    float targetRight = targetLeft + sheet->cols.getWidth(ci) * zoomScale;

    float targetTop = 0.0f;
    for (int r = 0; r < ri; ++r) {
        targetTop += sheet->rows.getHeight(r) * zoomScale;
    }
    float targetBottom = targetTop + sheet->rows.getHeight(ri) * zoomScale;

    float viewW = availableWidth - headerColW - SCROLLBAR_SIZE;
    float viewH = availableHeight - headerRowH;

    if (targetLeft < scrollX) {
        scrollX = targetLeft;
    } else if (targetRight > scrollX + viewW) {
        scrollX = targetRight - viewW;
    }

    if (targetTop < scrollY) {
        scrollY = targetTop;
    } else if (targetBottom > scrollY + viewH) {
        scrollY = targetBottom - viewH;
    }

    scrollX = std::max(0.0f, scrollX);
    scrollY = std::max(0.0f, scrollY);
}

void SpreadsheetUI::renderGridCanvas() {
    auto sheet = book.activeSheet();

    // Dimensions
    float totalAvailW = ImGui::GetContentRegionAvail().x;
    float totalAvailH = ImGui::GetContentRegionAvail().y - BOTTOM_BAR_HEIGHT - STATUS_BAR_HEIGHT;

    ImGui::BeginChild("ExcelGridArea", ImVec2(totalAvailW, totalAvailH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool isMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool isMouseDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    bool isRightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    float gridViewW = totalAvailW - SCROLLBAR_SIZE;
    float gridViewH = totalAvailH;

    float headerColW = showHeadings ? (sheet->cols.indexWidth) : 0.0f;
    float headerRowH = showHeadings ? (sheet->rows.default_height) : 0.0f;

    float totalContentW = sheet->cols.totalWidth() * zoomScale;
    float totalContentH = sheet->rows.totalHeight() * zoomScale;

    float maxScrollX = std::max(0.0f, totalContentW - (gridViewW - headerColW) + 200.0f);
    float maxScrollY = std::max(0.0f, totalContentH - (gridViewH - headerRowH) + 200.0f);

    // Mouse wheel scrolling
    float wheel = ImGui::GetIO().MouseWheel;
    float wheelH = ImGui::GetIO().MouseWheelH;
    if (wheel != 0.0f || wheelH != 0.0f) {
        scrollActivityTimer = 0.25f; // keep 60 FPS active during and right after scroll
    }
    if (ImGui::IsWindowHovered()) {
        if (ImGui::GetIO().KeyShift) {
            scrollX = std::clamp(scrollX - wheel * 40.0f, 0.0f, maxScrollX);
        } else {
            scrollY = std::clamp(scrollY - wheel * 40.0f, 0.0f, maxScrollY);
            scrollX = std::clamp(scrollX - wheelH * 40.0f, 0.0f, maxScrollX);
        }
    }

    if (isDraggingVScroll || isDraggingHScroll || isDraggingSplitter || isResizingCol || isResizingRow || isSelectingRange || isDraggingAutofill) {
        scrollActivityTimer = 0.15f;
    }

    // Grid Background
    drawList->AddRectFilled(origin, ImVec2(origin.x + gridViewW, origin.y + gridViewH), IM_COL32(255, 255, 255, 255));

    // Handle Column Resizing drag
    if (isResizingCol) {
        if (isMouseDown) {
            float delta = (mousePos.x - resizeColStartX) / zoomScale;
            float newWidth = std::max(sheet->cols.minWidth, resizeColStartWidth + delta);
            sheet->cols.setWidth(resizeColIdx, newWidth);
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        } else {
            isResizingCol = false;
            resizeColIdx = -1;
        }
    }

    // Handle Row Resizing drag
    if (isResizingRow) {
        if (isMouseDown) {
            float delta = (mousePos.y - resizeRowStartY) / zoomScale;
            float newHeight = std::max(16.0f, resizeRowStartHeight + delta);
            sheet->rows.setHeight(resizeRowIdx, newHeight);
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        } else {
            isResizingRow = false;
            resizeRowIdx = -1;
        }
    }

    // Column positions calculation
    std::vector<float> colX(sheet->cols.len + 1);
    float curX = origin.x + headerColW;
    for (int c = 0; c < sheet->cols.len; ++c) {
        colX[c] = curX;
        float w = sheet->cols.getWidth(c) * zoomScale;
        curX += w;
    }
    colX[sheet->cols.len] = curX;

    // Adjust scroll for non-frozen columns
    for (int c = sheet->freeze.second; c <= sheet->cols.len; ++c) {
        colX[c] -= scrollX;
    }

    // Row positions calculation
    std::vector<float> rowY(sheet->rows.len + 1);
    float curY = origin.y + headerRowH;
    for (int r = 0; r < sheet->rows.len; ++r) {
        rowY[r] = curY;
        float h = sheet->rows.getHeight(r) * zoomScale;
        curY += h;
    }
    rowY[sheet->rows.len] = curY;

    // Adjust scroll for non-frozen rows
    for (int r = sheet->freeze.first; r <= sheet->rows.len; ++r) {
        rowY[r] -= scrollY;
    }

    // Clip rect for grid cells
    ImVec4 gridClip(origin.x + headerColW, origin.y + headerRowH,
                    origin.x + gridViewW, origin.y + gridViewH);
    drawList->PushClipRect(ImVec2(gridClip.x, gridClip.y), ImVec2(gridClip.z, gridClip.w), true);

    // Visible range calculation
    int minVisibleC = 0, maxVisibleC = sheet->cols.len - 1;
    int minVisibleR = 0, maxVisibleR = sheet->rows.len - 1;

    for (int c = 0; c < sheet->cols.len; ++c) {
        if (colX[c + 1] >= origin.x + headerColW) { minVisibleC = c; break; }
    }
    for (int c = minVisibleC; c < sheet->cols.len; ++c) {
        if (colX[c] > origin.x + gridViewW) { maxVisibleC = c; break; }
    }
    for (int r = 0; r < sheet->rows.len; ++r) {
        if (rowY[r + 1] >= origin.y + headerRowH) { minVisibleR = r; break; }
    }
    for (int r = minVisibleR; r < sheet->rows.len; ++r) {
        if (rowY[r] > origin.y + gridViewH) { maxVisibleR = r; break; }
    }

    // Render cells in visible range
    for (int r = minVisibleR; r <= maxVisibleR; ++r) {
        int logR = sheet->mapVisualToLogicalRow(r);
        for (int c = minVisibleC; c <= maxVisibleC; ++c) {
            float x1 = colX[c];
            float y1 = rowY[r];
            float x2 = colX[c + 1];
            float y2 = rowY[r + 1];

            // Merged cell check
            auto mergeOpt = sheet->merges.getFirstIncludes(logR, c);
            if (mergeOpt) {
                if (mergeOpt->sri != logR || mergeOpt->sci != c) {
                    continue; // subordinate cell of merge, master renders it
                }
                int endVisR = sheet->mapLogicalToVisualRow(mergeOpt->eri);
                x2 = colX[mergeOpt->eci + 1];
                y2 = rowY[endVisR + 1];
            }

            Style style = sheet->getCellStyle(logR, c);
            ImU32 bgCol = parseHexColor(style.bgcolor, IM_COL32(255, 255, 255, 255));
            if (bgCol != IM_COL32(255, 255, 255, 255)) {
                drawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), bgCol);
            }

            // Excel gridlines
            if (showGridlines) {
                drawList->AddLine(ImVec2(x1, y2), ImVec2(x2, y2), IM_COL32(218, 220, 224, 255));
                drawList->AddLine(ImVec2(x2, y1), ImVec2(x2, y2), IM_COL32(218, 220, 224, 255));
            }

            // Custom Borders
            if (style.border.hasAny()) {
                if (style.border.top) {
                    drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y1), parseHexColor(style.border.top->color, IM_COL32(0,0,0,255)), 1.5f);
                }
                if (style.border.bottom) {
                    drawList->AddLine(ImVec2(x1, y2), ImVec2(x2, y2), parseHexColor(style.border.bottom->color, IM_COL32(0,0,0,255)), 1.5f);
                }
                if (style.border.left) {
                    drawList->AddLine(ImVec2(x1, y1), ImVec2(x1, y2), parseHexColor(style.border.left->color, IM_COL32(0,0,0,255)), 1.5f);
                }
                if (style.border.right) {
                    drawList->AddLine(ImVec2(x2, y1), ImVec2(x2, y2), parseHexColor(style.border.right->color, IM_COL32(0,0,0,255)), 1.5f);
                }
            }

            // Cell text
            std::string disp = sheet->getCellDisplayValue(logR, c);
            if (!disp.empty()) {
                ImVec2 textSize = ImGui::CalcTextSize(disp.c_str());
                float tx = x1 + 4.0f;
                float ty = y1 + (y2 - y1 - textSize.y) * 0.5f;

                if (style.align == "center") {
                    tx = x1 + (x2 - x1 - textSize.x) * 0.5f;
                } else if (style.align == "right") {
                    tx = x2 - textSize.x - 4.0f;
                }

                if (style.valign == "top") {
                    ty = y1 + 2.0f;
                } else if (style.valign == "bottom") {
                    ty = y2 - textSize.y - 2.0f;
                }

                ImU32 textCol = parseHexColor(style.color, IM_COL32(20, 20, 20, 255));
                drawList->AddText(ImVec2(tx, ty), textCol, disp.c_str());

                if (style.strike) {
                    float sy = ty + textSize.y * 0.5f;
                    drawList->AddLine(ImVec2(tx, sy), ImVec2(tx + textSize.x, sy), textCol, 1.0f);
                }
                if (style.underline) {
                    float uy = ty + textSize.y + 1.0f;
                    drawList->AddLine(ImVec2(tx, uy), ImVec2(tx + textSize.x, uy), textCol, 1.0f);
                }
            }

            // Data validation error marker
            if (!sheet->validations.getError(logR, c).empty()) {
                drawList->AddTriangleFilled(
                    ImVec2(x2 - 7.0f, y1),
                    ImVec2(x2, y1),
                    ImVec2(x2, y1 + 7.0f),
                    IM_COL32(220, 30, 30, 240)
                );
            }
        }
    }

    // Excel Selection Box & Autofill Handle
    {
        int selStartVisR = sheet->mapLogicalToVisualRow(sheet->selector.sri);
        int selEndVisR = sheet->mapLogicalToVisualRow(sheet->selector.eri);
        int minSelR = std::min(selStartVisR, selEndVisR);
        int maxSelR = std::max(selStartVisR, selEndVisR);

        float sx1 = colX[sheet->selector.sci];
        float sy1 = rowY[minSelR];
        float sx2 = colX[sheet->selector.eci + 1];
        float sy2 = rowY[maxSelR + 1];

        // Excel selection tint (active anchor cell stays clear)
        if (sheet->selector.multiple()) {
            drawList->AddRectFilled(ImVec2(sx1, sy1), ImVec2(sx2, sy2), IM_COL32(30, 100, 215, 28));

            // Clear active cell
            int actVisR = sheet->mapLogicalToVisualRow(sheet->select_ri);
            float ax1 = colX[sheet->select_ci];
            float ay1 = rowY[actVisR];
            float ax2 = colX[sheet->select_ci + 1];
            float ay2 = rowY[actVisR + 1];
            drawList->AddRectFilled(ImVec2(ax1, ay1), ImVec2(ax2, ay2), IM_COL32(255, 255, 255, 255));
        }

        // Excel 2px signature selection border
        drawList->AddRect(ImVec2(sx1, sy1), ImVec2(sx2, sy2), IM_COL32(30, 100, 215, 255), 0.0f, 0, 2.0f);

        // Autofill Handle square (6x6) at bottom right
        float hSize = 3.0f;
        ImVec2 handleCenter(sx2, sy2);
        ImVec2 handleMin(handleCenter.x - hSize, handleCenter.y - hSize);
        ImVec2 handleMax(handleCenter.x + hSize, handleCenter.y + hSize);

        drawList->AddRectFilled(handleMin, handleMax, IM_COL32(30, 100, 215, 255));
        drawList->AddRect(handleMin, handleMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f);

        // Handle Hover & Drag
        if (mousePos.x >= handleMin.x - 3 && mousePos.x <= handleMax.x + 3 &&
            mousePos.y >= handleMin.y - 3 && mousePos.y <= handleMax.y + 3) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            if (isMouseClicked) {
                isDraggingAutofill = true;
                autofillTargetRange = sheet->selector;
            }
        }
    }

    // Autofill Dragging Preview
    if (isDraggingAutofill) {
        if (isMouseDown) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            int hoverC = -1, hoverR = -1;
            for (int c = 0; c < sheet->cols.len; ++c) {
                if (mousePos.x >= colX[c] && mousePos.x < colX[c + 1]) { hoverC = c; break; }
            }
            for (int r = 0; r < sheet->rows.len; ++r) {
                if (mousePos.y >= rowY[r] && mousePos.y < rowY[r + 1]) { hoverR = r; break; }
            }
            if (hoverC >= 0 && hoverR >= 0) {
                int logR = sheet->mapVisualToLogicalRow(hoverR);
                autofillTargetRange = sheet->selector;
                if (hoverR > sheet->selector.eri) autofillTargetRange.eri = logR;
                if (hoverC > sheet->selector.eci) autofillTargetRange.eci = hoverC;
                if (hoverR < sheet->selector.sri) autofillTargetRange.sri = logR;
                if (hoverC < sheet->selector.sci) autofillTargetRange.sci = hoverC;

                float ax1 = colX[autofillTargetRange.sci];
                float ay1 = rowY[autofillTargetRange.sri];
                float ax2 = colX[autofillTargetRange.eci + 1];
                float ay2 = rowY[autofillTargetRange.eri + 1];
                drawList->AddRect(ImVec2(ax1, ay1), ImVec2(ax2, ay2), IM_COL32(30, 100, 215, 200), 0.0f, 0, 1.5f);
            }
        } else {
            isDraggingAutofill = false;
            sheet->autofill(autofillTargetRange);
        }
    }

    // Clipboard Marching Box
    if (sheet->hasClipboard) {
        float cx1 = colX[sheet->clipboardRange.sci];
        float cy1 = rowY[sheet->clipboardRange.sri];
        float cx2 = colX[sheet->clipboardRange.eci + 1];
        float cy2 = rowY[sheet->clipboardRange.eri + 1];
        drawList->AddRect(ImVec2(cx1, cy1), ImVec2(cx2, cy2), IM_COL32(40, 160, 60, 240), 0.0f, 0, 2.0f);
    }

    // Freeze dividing line
    if (sheet->freeze.first > 0) {
        float fy = rowY[sheet->freeze.first];
        drawList->AddLine(ImVec2(origin.x + headerColW, fy), ImVec2(origin.x + gridViewW, fy), IM_COL32(30, 100, 215, 200), 2.0f);
    }
    if (sheet->freeze.second > 0) {
        float fx = colX[sheet->freeze.second];
        drawList->AddLine(ImVec2(fx, origin.y + headerRowH), ImVec2(fx, origin.y + gridViewH), IM_COL32(30, 100, 215, 200), 2.0f);
    }

    drawList->PopClipRect();

    // Column Headers (A, B, C...)
    if (showHeadings) {
        drawList->PushClipRect(ImVec2(origin.x + headerColW, origin.y), ImVec2(origin.x + gridViewW, origin.y + headerRowH), true);
        for (int c = minVisibleC; c <= maxVisibleC; ++c) {
            float x1 = colX[c];
            float x2 = colX[c + 1];
            bool isColSelected = (c >= sheet->selector.sci && c <= sheet->selector.eci);

            ImU32 hdrBg = isColSelected ? IM_COL32(224, 230, 240, 255) : IM_COL32(245, 246, 248, 255);
            drawList->AddRectFilled(ImVec2(x1, origin.y), ImVec2(x2, origin.y + headerRowH), hdrBg);
            drawList->AddLine(ImVec2(x2, origin.y), ImVec2(x2, origin.y + headerRowH), IM_COL32(218, 220, 224, 255));
            drawList->AddLine(ImVec2(x1, origin.y + headerRowH), ImVec2(x2, origin.y + headerRowH), IM_COL32(200, 202, 208, 255));

            // Selection indicator on column header bottom edge
            if (isColSelected) {
                drawList->AddLine(ImVec2(x1, origin.y + headerRowH - 1), ImVec2(x2, origin.y + headerRowH - 1), IM_COL32(30, 100, 215, 255), 2.0f);
            }

            // Column Letter
            std::string colName = stringAt(c);
            ImVec2 txtSz = ImGui::CalcTextSize(colName.c_str());
            drawList->AddText(ImVec2(x1 + (x2 - x1 - txtSz.x) * 0.5f, origin.y + (headerRowH - txtSz.y) * 0.5f),
                              isColSelected ? IM_COL32(20, 60, 140, 255) : IM_COL32(70, 72, 75, 255), colName.c_str());

            // AutoFilter Dropdown Icon
            if (sheet->autoFilter.active() && sheet->autoFilter.hrange().includes(sheet->autoFilter.range().sri, c)) {
                float btnX = x2 - 14.0f;
                float btnY = origin.y + headerRowH - 14.0f;
                drawList->AddTriangleFilled(
                    ImVec2(btnX, btnY),
                    ImVec2(btnX + 8.0f, btnY),
                    ImVec2(btnX + 4.0f, btnY + 6.0f),
                    IM_COL32(50, 50, 50, 200)
                );
                if (isMouseClicked && mousePos.x >= btnX - 4 && mousePos.x <= btnX + 12 &&
                    mousePos.y >= btnY - 4 && mousePos.y <= btnY + 12) {
                    dialogSortFilter.open(*sheet, c);
                }
            }

            // Click header to select column
            if (isMouseClicked && mousePos.y >= origin.y && mousePos.y <= origin.y + headerRowH &&
                mousePos.x >= x1 && mousePos.x < x2 - 4.0f) {
                sheet->select_ci = c;
                sheet->select_ri = 0;
                sheet->selector.set(0, c, sheet->rows.len - 1, c);
            }

            // Column Resizing Handle
            if (mousePos.x >= x2 - 4.0f && mousePos.x <= x2 + 4.0f &&
                mousePos.y >= origin.y && mousePos.y <= origin.y + headerRowH) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                if (isMouseClicked) {
                    isResizingCol = true;
                    resizeColIdx = c;
                    resizeColStartX = mousePos.x;
                    resizeColStartWidth = sheet->cols.getWidth(c);
                }
            }
        }
        drawList->PopClipRect();
    }

    // Row Headers (1, 2, 3...)
    if (showHeadings) {
        drawList->PushClipRect(ImVec2(origin.x, origin.y + headerRowH), ImVec2(origin.x + headerColW, origin.y + gridViewH), true);
        for (int r = minVisibleR; r <= maxVisibleR; ++r) {
            float y1 = rowY[r];
            float y2 = rowY[r + 1];
            int logR = sheet->mapVisualToLogicalRow(r);
            bool isRowSelected = (logR >= sheet->selector.sri && logR <= sheet->selector.eri);

            ImU32 hdrBg = isRowSelected ? IM_COL32(224, 230, 240, 255) : IM_COL32(245, 246, 248, 255);
            drawList->AddRectFilled(ImVec2(origin.x, y1), ImVec2(origin.x + headerColW, y2), hdrBg);
            drawList->AddLine(ImVec2(origin.x, y2), ImVec2(origin.x + headerColW, y2), IM_COL32(218, 220, 224, 255));
            drawList->AddLine(ImVec2(origin.x + headerColW, y1), ImVec2(origin.x + headerColW, y2), IM_COL32(200, 202, 208, 255));

            // Selection indicator on row header right edge
            if (isRowSelected) {
                drawList->AddLine(ImVec2(origin.x + headerColW - 1, y1), ImVec2(origin.x + headerColW - 1, y2), IM_COL32(30, 100, 215, 255), 2.0f);
            }

            // Row Number
            std::string rowName = std::to_string(logR + 1);
            ImVec2 txtSz = ImGui::CalcTextSize(rowName.c_str());
            drawList->AddText(ImVec2(origin.x + (headerColW - txtSz.x) * 0.5f, y1 + (y2 - y1 - txtSz.y) * 0.5f),
                              isRowSelected ? IM_COL32(20, 60, 140, 255) : IM_COL32(70, 72, 75, 255), rowName.c_str());

            // Click header to select row
            if (isMouseClicked && mousePos.x >= origin.x && mousePos.x <= origin.x + headerColW &&
                mousePos.y >= y1 && mousePos.y < y2 - 4.0f) {
                sheet->select_ri = logR;
                sheet->select_ci = 0;
                sheet->selector.set(logR, 0, logR, sheet->cols.len - 1);
            }

            // Row Resizing Handle
            if (mousePos.y >= y2 - 4.0f && mousePos.y <= y2 + 4.0f &&
                mousePos.x >= origin.x && mousePos.x <= origin.x + headerColW) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                if (isMouseClicked) {
                    isResizingRow = true;
                    resizeRowIdx = logR;
                    resizeRowStartY = mousePos.y;
                    resizeRowStartHeight = sheet->rows.getHeight(logR);
                }
            }
        }
        drawList->PopClipRect();
    }

    // Top-Left Select All Corner Box
    if (showHeadings) {
        drawList->AddRectFilled(origin, ImVec2(origin.x + headerColW, origin.y + headerRowH), IM_COL32(240, 242, 245, 255));
        drawList->AddLine(ImVec2(origin.x, origin.y + headerRowH), ImVec2(origin.x + headerColW, origin.y + headerRowH), IM_COL32(200, 202, 208, 255));
        drawList->AddLine(ImVec2(origin.x + headerColW, origin.y), ImVec2(origin.x + headerColW, origin.y + headerRowH), IM_COL32(200, 202, 208, 255));

        // Excel triangle in corner
        drawList->AddTriangleFilled(
            ImVec2(origin.x + headerColW - 6.0f, origin.y + headerRowH - 2.0f),
            ImVec2(origin.x + headerColW - 2.0f, origin.y + headerRowH - 6.0f),
            ImVec2(origin.x + headerColW - 2.0f, origin.y + headerRowH - 2.0f),
            IM_COL32(140, 145, 155, 255)
        );

        if (isMouseClicked && mousePos.x >= origin.x && mousePos.x <= origin.x + headerColW &&
            mousePos.y >= origin.y && mousePos.y <= origin.y + headerRowH) {
            sheet->selector.set(0, 0, sheet->rows.len - 1, sheet->cols.len - 1);
        }
    }

    // Grid Cell Clicking & Drag Selecting
    bool insideGrid = mousePos.x >= origin.x + headerColW && mousePos.x <= origin.x + gridViewW &&
                      mousePos.y >= origin.y + headerRowH && mousePos.y <= origin.y + gridViewH;

    if (insideGrid && !isResizingCol && !isResizingRow && !isDraggingAutofill) {
        int cellC = -1, cellR = -1;
        for (int c = minVisibleC; c <= maxVisibleC; ++c) {
            if (mousePos.x >= colX[c] && mousePos.x < colX[c + 1]) { cellC = c; break; }
        }
        for (int r = minVisibleR; r <= maxVisibleR; ++r) {
            if (mousePos.y >= rowY[r] && mousePos.y < rowY[r + 1]) { cellR = r; break; }
        }

        if (cellC >= 0 && cellR >= 0) {
            int logR = sheet->mapVisualToLogicalRow(cellR);

            if (isMouseDoubleClicked && !isEditingCell) {
                startCellEditing(logR, cellC);
            } else if (isMouseClicked) {
                if (isEditingCell) {
                    commitCellEditing();
                }
                sheet->select_ri = logR;
                sheet->select_ci = cellC;
                sheet->selector.set(logR, cellC, logR, cellC);
                isSelectingRange = true;
            } else if (isMouseDown && isSelectingRange) {
                sheet->selector.eri = logR;
                sheet->selector.eci = cellC;
                if (sheet->selector.sri > sheet->selector.eri) std::swap(sheet->selector.sri, sheet->selector.eri);
                if (sheet->selector.sci > sheet->selector.eci) std::swap(sheet->selector.sci, sheet->selector.eci);
            }

            if (isRightClicked) {
                if (!sheet->selector.includes(logR, cellC)) {
                    sheet->select_ri = logR;
                    sheet->select_ci = cellC;
                    sheet->selector.set(logR, cellC, logR, cellC);
                }
                ImGui::OpenPopup("GridContextMenu");
            }
        }
    }

    if (!isMouseDown) {
        isSelectingRange = false;
    }

    // Context Menu
    renderContextMenu();

    // Inline In-place Cell Editor
    if (isEditingCell) {
        int visR = sheet->mapLogicalToVisualRow(editingRi);
        float ex1 = colX[editingCi];
        float ey1 = rowY[visR];
        float ex2 = colX[editingCi + 1];
        float ey2 = rowY[visR + 1];

        ImGui::SetCursorScreenPos(ImVec2(ex1, ey1));
        ImGui::SetNextItemWidth(ex2 - ex1);

        if (cellEditInitialFocus) {
            ImGui::SetKeyboardFocusHere();
            cellEditInitialFocus = false;
        }

        ImGuiInputTextFlags editFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
        if (ImGui::InputText("##InPlaceCellEditor", cellEditBuf, sizeof(cellEditBuf), editFlags)) {
            commitCellEditing();
            if (sheet->select_ri < sheet->rows.len - 1) {
                sheet->select_ri++;
                sheet->selector.set(sheet->select_ri, sheet->select_ci, sheet->select_ri, sheet->select_ci);
                scrollToCell(sheet->select_ri, sheet->select_ci, gridViewW, gridViewH, headerColW, headerRowH);
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            cancelCellEditing();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            commitCellEditing();
            if (sheet->select_ci < sheet->cols.len - 1) {
                sheet->select_ci++;
                sheet->selector.set(sheet->select_ri, sheet->select_ci, sheet->select_ri, sheet->select_ci);
                scrollToCell(sheet->select_ri, sheet->select_ci, gridViewW, gridViewH, headerColW, headerRowH);
            }
        }
    } else {
        // Keyboard Navigation, Full Shortcuts & Type-to-Edit
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            bool ctrl = ImGui::GetIO().KeyCtrl;
            if (ctrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
                    sheet->undo();
                } else if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
                    sheet->redo();
                } else if (ImGui::IsKeyPressed(ImGuiKey_C)) {
                    sheet->copy();
                } else if (ImGui::IsKeyPressed(ImGuiKey_X)) {
                    sheet->cut();
                } else if (ImGui::IsKeyPressed(ImGuiKey_V)) {
                    if (ImGui::GetIO().KeyShift) {
                        sheet->paste("text");
                    } else if (ImGui::GetIO().KeyAlt) {
                        sheet->paste("format");
                    } else {
                        sheet->paste("all");
                    }
                } else if (ImGui::IsKeyPressed(ImGuiKey_S)) {
                    if (book.saveToFile(currentFilePathBuf)) {
                        lastSaveStatus = "Saved successfully";
                        saveStatusTimer = 3.0f;
                    }
                } else if (ImGui::IsKeyPressed(ImGuiKey_A)) {
                    sheet->selector.set(0, 0, sheet->rows.len - 1, sheet->cols.len - 1);
                } else if (ImGui::IsKeyPressed(ImGuiKey_B)) {
                    Style s = sheet->getSelectedCellStyle();
                    sheet->setSelectedCellAttr("font-bold", s.font.bold ? "false" : "true");
                } else if (ImGui::IsKeyPressed(ImGuiKey_I)) {
                    Style s = sheet->getSelectedCellStyle();
                    sheet->setSelectedCellAttr("font-italic", s.font.italic ? "false" : "true");
                } else if (ImGui::IsKeyPressed(ImGuiKey_U)) {
                    Style s = sheet->getSelectedCellStyle();
                    sheet->setSelectedCellAttr("underline", s.underline ? "false" : "true");
                }
            } else {
                // Type-to-edit: typing printable characters immediately begins cell editing
                if (!ImGui::GetIO().InputQueueCharacters.empty()) {
                    std::string typed;
                    for (int n = 0; n < ImGui::GetIO().InputQueueCharacters.Size; ++n) {
                        ImWchar c = ImGui::GetIO().InputQueueCharacters[n];
                        if (c >= 32 && c < 127) {
                            typed += static_cast<char>(c);
                        }
                    }
                    if (!typed.empty()) {
                        ImGui::GetIO().InputQueueCharacters.clear();
                        startCellEditing(sheet->select_ri, sheet->select_ci, typed);
                    }
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_F2)) {
                startCellEditing(sheet->select_ri, sheet->select_ci);
            } else if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                sheet->deleteCellText();
            } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                if (ImGui::GetIO().KeyShift) {
                    if (sheet->selector.eci > sheet->selector.sci) sheet->selector.eci--;
                } else {
                    if (sheet->select_ci > 0) {
                        sheet->select_ci--;
                        sheet->selector.set(sheet->select_ri, sheet->select_ci, sheet->select_ri, sheet->select_ci);
                        scrollToCell(sheet->select_ri, sheet->select_ci, gridViewW, gridViewH, headerColW, headerRowH);
                    }
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                if (ImGui::GetIO().KeyShift) {
                    if (sheet->selector.eci < sheet->cols.len - 1) sheet->selector.eci++;
                } else {
                    if (sheet->select_ci < sheet->cols.len - 1) {
                        sheet->select_ci++;
                        sheet->selector.set(sheet->select_ri, sheet->select_ci, sheet->select_ri, sheet->select_ci);
                        scrollToCell(sheet->select_ri, sheet->select_ci, gridViewW, gridViewH, headerColW, headerRowH);
                    }
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                if (ImGui::GetIO().KeyShift) {
                    if (sheet->selector.eri > sheet->selector.sri) sheet->selector.eri--;
                } else {
                    if (sheet->select_ri > 0) {
                        sheet->select_ri--;
                        sheet->selector.set(sheet->select_ri, sheet->select_ci, sheet->select_ri, sheet->select_ci);
                        scrollToCell(sheet->select_ri, sheet->select_ci, gridViewW, gridViewH, headerColW, headerRowH);
                    }
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                if (ImGui::GetIO().KeyShift) {
                    if (sheet->selector.eri < sheet->rows.len - 1) sheet->selector.eri++;
                } else {
                    if (sheet->select_ri < sheet->rows.len - 1) {
                        sheet->select_ri++;
                        sheet->selector.set(sheet->select_ri, sheet->select_ci, sheet->select_ri, sheet->select_ci);
                        scrollToCell(sheet->select_ri, sheet->select_ci, gridViewW, gridViewH, headerColW, headerRowH);
                    }
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
                if (ImGui::GetIO().KeyCtrl) {
                    sheet->select_ri = 0;
                    sheet->select_ci = 0;
                } else {
                    sheet->select_ci = 0;
                }
                sheet->selector.set(sheet->select_ri, sheet->select_ci, sheet->select_ri, sheet->select_ci);
                scrollToCell(sheet->select_ri, sheet->select_ci, gridViewW, gridViewH, headerColW, headerRowH);
            }
        }
    }

    // 7. Render Vertical Scrollbar on Right
    renderVerticalScrollbar(drawList, origin.x + gridViewW, origin.y, SCROLLBAR_SIZE, gridViewH, totalContentH, gridViewH - headerRowH);

    ImGui::EndChild();
}

void SpreadsheetUI::renderVerticalScrollbar(ImDrawList* drawList, float x, float y, float width, float height, float totalContentH, float viewH) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool isMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    float btnH = width; // 16px square button
    float trackY1 = y + btnH;
    float trackY2 = y + height - btnH;
    float trackH = trackY2 - trackY1;

    // Track Background
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), IM_COL32(242, 243, 246, 255));
    drawList->AddLine(ImVec2(x, y), ImVec2(x, y + height), IM_COL32(215, 218, 224, 255));

    // Top Arrow Button [▲]
    ImVec2 topBtnMin(x, y);
    ImVec2 topBtnMax(x + width, y + btnH);
    bool topBtnHovered = (mousePos.x >= topBtnMin.x && mousePos.x <= topBtnMax.x && mousePos.y >= topBtnMin.y && mousePos.y <= topBtnMax.y);
    if (topBtnHovered) {
        drawList->AddRectFilled(topBtnMin, topBtnMax, IM_COL32(215, 220, 230, 255));
        if (isMouseDown) {
            scrollY = std::max(0.0f, scrollY - 25.0f * zoomScale);
        }
    }
    // Arrow Triangle UP
    drawList->AddTriangleFilled(
        ImVec2(x + width * 0.5f, y + 4.0f),
        ImVec2(x + 3.0f, y + btnH - 4.0f),
        ImVec2(x + width - 3.0f, y + btnH - 4.0f),
        IM_COL32(80, 85, 95, 255)
    );

    // Bottom Arrow Button [▼]
    ImVec2 botBtnMin(x, y + height - btnH);
    ImVec2 botBtnMax(x + width, y + height);
    bool botBtnHovered = (mousePos.x >= botBtnMin.x && mousePos.x <= botBtnMax.x && mousePos.y >= botBtnMin.y && mousePos.y <= botBtnMax.y);
    float maxScrollY = std::max(0.0f, totalContentH - viewH + 200.0f);
    if (botBtnHovered) {
        drawList->AddRectFilled(botBtnMin, botBtnMax, IM_COL32(215, 220, 230, 255));
        if (isMouseDown) {
            scrollY = std::min(maxScrollY, scrollY + 25.0f * zoomScale);
        }
    }
    // Arrow Triangle DOWN
    drawList->AddTriangleFilled(
        ImVec2(x + 3.0f, y + height - btnH + 4.0f),
        ImVec2(x + width - 3.0f, y + height - btnH + 4.0f),
        ImVec2(x + width * 0.5f, y + height - 4.0f),
        IM_COL32(80, 85, 95, 255)
    );

    if (trackH <= 10.0f) return;

    // Thumb sizing & positioning
    float thumbRatio = std::min(1.0f, viewH / (totalContentH + 200.0f));
    float thumbH = std::max(22.0f, trackH * thumbRatio);
    float scrollRatio = (maxScrollY > 0.0f) ? std::clamp(scrollY / maxScrollY, 0.0f, 1.0f) : 0.0f;
    float thumbY = trackY1 + scrollRatio * (trackH - thumbH);

    ImVec2 thumbMin(x + 2.0f, thumbY);
    ImVec2 thumbMax(x + width - 2.0f, thumbY + thumbH);
    bool thumbHovered = (mousePos.x >= x && mousePos.x <= x + width && mousePos.y >= thumbY && mousePos.y <= thumbY + thumbH);

    // Drag interaction
    if (thumbHovered && isMouseClicked) {
        isDraggingVScroll = true;
        dragVScrollMouseStartY = mousePos.y;
        dragVScrollInitialScrollY = scrollY;
    }

    if (isDraggingVScroll) {
        if (isMouseDown) {
            float deltaY = mousePos.y - dragVScrollMouseStartY;
            float scrollDelta = (trackH > thumbH) ? (deltaY * (maxScrollY / (trackH - thumbH))) : 0.0f;
            scrollY = std::clamp(dragVScrollInitialScrollY + scrollDelta, 0.0f, maxScrollY);
        } else {
            isDraggingVScroll = false;
        }
    } else if (isMouseClicked && mousePos.x >= x && mousePos.x <= x + width) {
        // Track click (Page Up / Page Down)
        if (mousePos.y >= trackY1 && mousePos.y < thumbY) {
            scrollY = std::max(0.0f, scrollY - viewH);
        } else if (mousePos.y > thumbY + thumbH && mousePos.y <= trackY2) {
            scrollY = std::min(maxScrollY, scrollY + viewH);
        }
    }

    // Render Thumb
    ImU32 thumbCol = IM_COL32(195, 198, 205, 255);
    if (isDraggingVScroll) {
        thumbCol = IM_COL32(130, 135, 145, 255);
    } else if (thumbHovered) {
        thumbCol = IM_COL32(165, 170, 180, 255);
    }
    drawList->AddRectFilled(thumbMin, thumbMax, thumbCol, 2.0f);
}

void SpreadsheetUI::renderHorizontalScrollbar(ImDrawList* drawList, float x, float y, float width, float height, float totalContentW, float viewW) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool isMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    float btnW = height; // square button matching height
    float trackX1 = x + btnW;
    float trackX2 = x + width - btnW;
    float trackW = trackX2 - trackX1;

    // Track Background
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), IM_COL32(242, 243, 246, 255));
    drawList->AddLine(ImVec2(x, y), ImVec2(x + width, y), IM_COL32(215, 218, 224, 255));

    // Left Arrow Button [◄]
    ImVec2 leftBtnMin(x, y);
    ImVec2 leftBtnMax(x + btnW, y + height);
    bool leftBtnHovered = (mousePos.x >= leftBtnMin.x && mousePos.x <= leftBtnMax.x && mousePos.y >= leftBtnMin.y && mousePos.y <= leftBtnMax.y);
    if (leftBtnHovered) {
        drawList->AddRectFilled(leftBtnMin, leftBtnMax, IM_COL32(215, 220, 230, 255));
        if (isMouseDown) {
            scrollX = std::max(0.0f, scrollX - 50.0f * zoomScale);
        }
    }
    // Arrow Triangle LEFT
    drawList->AddTriangleFilled(
        ImVec2(x + 4.0f, y + height * 0.5f),
        ImVec2(x + btnW - 4.0f, y + 3.0f),
        ImVec2(x + btnW - 4.0f, y + height - 3.0f),
        IM_COL32(80, 85, 95, 255)
    );

    // Right Arrow Button [►]
    ImVec2 rightBtnMin(x + width - btnW, y);
    ImVec2 rightBtnMax(x + width, y + height);
    bool rightBtnHovered = (mousePos.x >= rightBtnMin.x && mousePos.x <= rightBtnMax.x && mousePos.y >= rightBtnMin.y && mousePos.y <= rightBtnMax.y);
    float maxScrollX = std::max(0.0f, totalContentW - viewW + 200.0f);
    if (rightBtnHovered) {
        drawList->AddRectFilled(rightBtnMin, rightBtnMax, IM_COL32(215, 220, 230, 255));
        if (isMouseDown) {
            scrollX = std::min(maxScrollX, scrollX + 50.0f * zoomScale);
        }
    }
    // Arrow Triangle RIGHT
    drawList->AddTriangleFilled(
        ImVec2(x + width - 4.0f, y + height * 0.5f),
        ImVec2(x + width - btnW + 4.0f, y + height - 3.0f),
        ImVec2(x + width - btnW + 4.0f, y + 3.0f),
        IM_COL32(80, 85, 95, 255)
    );

    if (trackW <= 10.0f) return;

    // Thumb sizing & positioning
    float thumbRatio = std::min(1.0f, viewW / (totalContentW + 200.0f));
    float thumbW = std::max(22.0f, trackW * thumbRatio);
    float scrollRatio = (maxScrollX > 0.0f) ? std::clamp(scrollX / maxScrollX, 0.0f, 1.0f) : 0.0f;
    float thumbX = trackX1 + scrollRatio * (trackW - thumbW);

    ImVec2 thumbMin(thumbX, y + 2.0f);
    ImVec2 thumbMax(thumbX + thumbW, y + height - 2.0f);
    bool thumbHovered = (mousePos.x >= thumbX && mousePos.x <= thumbX + thumbW && mousePos.y >= y && mousePos.y <= y + height);

    // Drag interaction
    if (thumbHovered && isMouseClicked) {
        isDraggingHScroll = true;
        dragHScrollMouseStartX = mousePos.x;
        dragHScrollInitialScrollX = scrollX;
    }

    if (isDraggingHScroll) {
        if (isMouseDown) {
            float deltaX = mousePos.x - dragHScrollMouseStartX;
            float scrollDelta = (trackW > thumbW) ? (deltaX * (maxScrollX / (trackW - thumbW))) : 0.0f;
            scrollX = std::clamp(dragHScrollInitialScrollX + scrollDelta, 0.0f, maxScrollX);
        } else {
            isDraggingHScroll = false;
        }
    } else if (isMouseClicked && mousePos.y >= y && mousePos.y <= y + height) {
        // Track click (Page Left / Page Right)
        if (mousePos.x >= trackX1 && mousePos.x < thumbX) {
            scrollX = std::max(0.0f, scrollX - viewW);
        } else if (mousePos.x > thumbX + thumbW && mousePos.x <= trackX2) {
            scrollX = std::min(maxScrollX, scrollX + viewW);
        }
    }

    // Render Thumb
    ImU32 thumbCol = IM_COL32(195, 198, 205, 255);
    if (isDraggingHScroll) {
        thumbCol = IM_COL32(130, 135, 145, 255);
    } else if (thumbHovered) {
        thumbCol = IM_COL32(165, 170, 180, 255);
    }
    drawList->AddRectFilled(thumbMin, thumbMax, thumbCol, 2.0f);
}

void SpreadsheetUI::renderBottomBar() {
    float availW = ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 barPos = ImGui::GetCursorScreenPos();
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool isMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.98f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));
    ImGui::BeginChild("ExcelBottomBar", ImVec2(0, BOTTOM_BAR_HEIGHT), false, ImGuiWindowFlags_NoScrollbar);

    // Top border line
    drawList->AddLine(barPos, ImVec2(barPos.x + availW, barPos.y), IM_COL32(215, 218, 224, 255));

    // Clamp splitter width
    bottomBarSplitWidth = std::clamp(bottomBarSplitWidth, 180.0f, availW - 120.0f);

    // 1. Sheet Navigation Buttons: |<, <, >, >|
    if (ImGui::Button("|<##FirstSheet", ImVec2(20, 22))) {
        book.activeSheetIndex = 0;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("First Sheet");

    ImGui::SameLine();
    if (ImGui::Button("<##PrevSheet", ImVec2(20, 22))) {
        if (book.activeSheetIndex > 0) book.activeSheetIndex--;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Previous Sheet");

    ImGui::SameLine();
    if (ImGui::Button(">##NextSheet", ImVec2(20, 22))) {
        if (book.activeSheetIndex < static_cast<int>(book.sheets.size()) - 1) book.activeSheetIndex++;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Next Sheet");

    ImGui::SameLine();
    if (ImGui::Button(">|##LastSheet", ImVec2(20, 22))) {
        book.activeSheetIndex = static_cast<int>(book.sheets.size()) - 1;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Last Sheet");

    ImGui::SameLine();

    // Plus (+) Add Sheet Button
    if (ImGui::Button("+##AddSheet", ImVec2(22, 22))) {
        book.addSheet();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("New Sheet");

    ImGui::SameLine();

    // 2. Sheet Tabs
    for (size_t i = 0; i < book.sheets.size(); ++i) {
        const auto& s = book.sheets[i];
        bool isActive = (book.activeSheetIndex == static_cast<int>(i));

        ImGui::PushID(static_cast<int>(i));
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.12f, 0.40f, 0.85f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.92f, 0.93f, 0.95f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.90f, 0.94f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        }

        if (renamingSheetIdx == static_cast<int>(i)) {
            ImGui::SetNextItemWidth(80);
            if (ImGui::InputText("##RenameSheetInput", renameSheetBuf, sizeof(renameSheetBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                book.renameSheet(static_cast<int>(i), renameSheetBuf);
                renamingSheetIdx = -1;
            }
            if (ImGui::IsItemDeactivated()) {
                book.renameSheet(static_cast<int>(i), renameSheetBuf);
                renamingSheetIdx = -1;
            }
        } else {
            if (ImGui::Button(s->name.c_str(), ImVec2(0, 22))) {
                book.activeSheetIndex = static_cast<int>(i);
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                renamingSheetIdx = static_cast<int>(i);
                strncpy(renameSheetBuf, s->name.c_str(), sizeof(renameSheetBuf) - 1);
            }
            if (ImGui::BeginPopupContextItem("SheetTabContext")) {
                if (ImGui::MenuItem("Rename")) {
                    renamingSheetIdx = static_cast<int>(i);
                    strncpy(renameSheetBuf, s->name.c_str(), sizeof(renameSheetBuf) - 1);
                }
                if (ImGui::MenuItem("Delete Sheet", nullptr, false, book.sheets.size() > 1)) {
                    book.deleteSheet(static_cast<int>(i));
                }
                ImGui::EndPopup();
            }
        }

        // Active tab bottom line indicator
        if (isActive) {
            ImVec2 tMin = ImGui::GetItemRectMin();
            ImVec2 tMax = ImGui::GetItemRectMax();
            drawList->AddLine(
                ImVec2(tMin.x, tMax.y - 1),
                ImVec2(tMax.x, tMax.y - 1),
                IM_COL32(30, 100, 215, 255),
                2.5f
            );
        }

        ImGui::PopStyleColor(3);
        ImGui::PopID();
        ImGui::SameLine();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // 3. Draggable Splitter Handle between Sheet Tabs and Horizontal Scrollbar
    float splitterX = barPos.x + bottomBarSplitWidth;
    float splitterW = 8.0f;
    ImVec2 splitMin(splitterX, barPos.y);
    ImVec2 splitMax(splitterX + splitterW, barPos.y + BOTTOM_BAR_HEIGHT);

    bool splitterHovered = (mousePos.x >= splitMin.x && mousePos.x <= splitMax.x && mousePos.y >= splitMin.y && mousePos.y <= splitMax.y);
    if (splitterHovered || isDraggingSplitter) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (splitterHovered && isMouseClicked) {
        isDraggingSplitter = true;
        dragSplitterStartX = mousePos.x;
        dragSplitterStartWidth = bottomBarSplitWidth;
    }
    if (isDraggingSplitter) {
        if (isMouseDown) {
            bottomBarSplitWidth = dragSplitterStartWidth + (mousePos.x - dragSplitterStartX);
        } else {
            isDraggingSplitter = false;
        }
    }

    // Draw Splitter dots (::)
    drawList->AddRectFilled(splitMin, splitMax, IM_COL32(235, 237, 240, 255));
    drawList->AddCircleFilled(ImVec2(splitterX + 4.0f, barPos.y + 8.0f), 1.5f, IM_COL32(130, 135, 145, 255));
    drawList->AddCircleFilled(ImVec2(splitterX + 4.0f, barPos.y + 13.0f), 1.5f, IM_COL32(130, 135, 145, 255));
    drawList->AddCircleFilled(ImVec2(splitterX + 4.0f, barPos.y + 18.0f), 1.5f, IM_COL32(130, 135, 145, 255));

    // 4. Horizontal Scrollbar to the right of the splitter
    float hScrollStartX = splitterX + splitterW;
    float hScrollW = (barPos.x + availW - SCROLLBAR_SIZE) - hScrollStartX;
    if (hScrollW > 20.0f) {
        auto sheet = book.activeSheet();
        float totalContentW = sheet->cols.totalWidth() * zoomScale;
        float viewW = (availW - SCROLLBAR_SIZE - sheet->cols.indexWidth);
        renderHorizontalScrollbar(drawList, hScrollStartX, barPos.y, hScrollW, BOTTOM_BAR_HEIGHT, totalContentW, viewW);
    }

    // 5. Corner Box at intersection of Horizontal & Vertical scrollbars
    float cornerX = barPos.x + availW - SCROLLBAR_SIZE;
    drawList->AddRectFilled(ImVec2(cornerX, barPos.y), ImVec2(cornerX + SCROLLBAR_SIZE, barPos.y + BOTTOM_BAR_HEIGHT), IM_COL32(242, 243, 246, 255));
    drawList->AddLine(ImVec2(cornerX, barPos.y), ImVec2(cornerX, barPos.y + BOTTOM_BAR_HEIGHT), IM_COL32(215, 218, 224, 255));
    drawList->AddLine(ImVec2(cornerX, barPos.y), ImVec2(cornerX + SCROLLBAR_SIZE, barPos.y), IM_COL32(215, 218, 224, 255));
}

void SpreadsheetUI::calculateSelectionStats(double& outSum, double& outAvg, int& outCount, int& outNumCount, double& outMin, double& outMax) {
    auto sheet = book.activeSheet();
    outSum = 0;
    outAvg = 0;
    outCount = 0;
    outNumCount = 0;
    outMin = DBL_MAX;
    outMax = -DBL_MAX;

    int sri = sheet->selector.sri;
    int eri = sheet->selector.eri;
    int sci = sheet->selector.sci;
    int eci = sheet->selector.eci;

    // Fast sparse traversal over populated cells
    auto ritLow = sheet->rows.data.lower_bound(sri);
    auto ritHigh = sheet->rows.data.upper_bound(eri);
    for (auto rit = ritLow; rit != ritHigh; ++rit) {
        for (const auto& cpair : rit->second.cells) {
            int ci = cpair.first;
            if (ci >= sci && ci <= eci) {
                const std::string& text = cpair.second.text;
                if (!text.empty()) {
                    outCount++;
                    char* end = nullptr;
                    double v = std::strtod(text.c_str(), &end);
                    if (end != text.c_str() && *end == '\0') {
                        outSum += v;
                        outNumCount++;
                        if (v < outMin) outMin = v;
                        if (v > outMax) outMax = v;
                    }
                }
            }
        }
    }

    if (outNumCount > 0) {
        outAvg = outSum / outNumCount;
    } else {
        outMin = 0;
        outMax = 0;
    }
}

void SpreadsheetUI::renderStatusBar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.94f, 0.94f, 0.96f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
    ImGui::BeginChild("ExcelStatusBarZone", ImVec2(0, STATUS_BAR_HEIGHT), false, ImGuiWindowFlags_NoScrollbar);

    // 1. Ready Indicator & Hybrid Render Mode
    if (isEditingCell) {
        ImGui::TextColored(ImVec4(0.1f, 0.45f, 0.95f, 1.0f), "Edit");
        ImGui::SameLine();
        ImGui::TextDisabled("| Event-Driven (Pixel updates)");
    } else if (isFastContinuousNeeded()) {
        ImGui::TextColored(ImVec4(0.1f, 0.65f, 0.25f, 1.0f), "Scroll");
        ImGui::SameLine();
        ImGui::TextDisabled("| 60 FPS (GPU Full-Catch)");
    } else {
        ImGui::TextDisabled("Ready | Hybrid 0 FPS (Idle)");
    }

    if (!lastSaveStatus.empty() && saveStatusTimer > 0.0f) {
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.15f, 0.70f, 0.35f, 1.0f), "%s", lastSaveStatus.c_str());
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // 2. Selection Statistics (Excel status bar)
    double sum = 0, avg = 0, minVal = 0, maxVal = 0;
    int count = 0, numCount = 0;
    calculateSelectionStats(sum, avg, count, numCount, minVal, maxVal);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    bool first = true;

    if (numCount > 1 && statShowAvg) {
        ss << "Average: " << avg;
        first = false;
    }
    if (count > 1 && statShowCount) {
        if (!first) ss << "   ";
        ss << "Count: " << count;
        first = false;
    }
    if (numCount > 1 && statShowNumCount) {
        if (!first) ss << "   ";
        ss << "Numerical Count: " << numCount;
        first = false;
    }
    if (numCount > 1 && statShowMin) {
        if (!first) ss << "   ";
        ss << "Min: " << minVal;
        first = false;
    }
    if (numCount > 1 && statShowMax) {
        if (!first) ss << "   ";
        ss << "Max: " << maxVal;
        first = false;
    }
    if (numCount > 1 && statShowSum) {
        if (!first) ss << "   ";
        ss << "Sum: " << sum;
    }

    std::string statsStr = ss.str();
    if (!statsStr.empty()) {
        ImGui::Text("%s", statsStr.c_str());
    }

    // Right-click context popup to customize status bar
    if (ImGui::BeginPopupContextWindow("StatusBarCustomContext")) {
        ImGui::Text("Customize Status Bar");
        ImGui::Separator();
        ImGui::Checkbox("Average", &statShowAvg);
        ImGui::Checkbox("Count", &statShowCount);
        ImGui::Checkbox("Numerical Count", &statShowNumCount);
        ImGui::Checkbox("Min", &statShowMin);
        ImGui::Checkbox("Max", &statShowMax);
        ImGui::Checkbox("Sum", &statShowSum);
        ImGui::EndPopup();
    }

    // 3. Right side: View shortcuts (Normal, Page Layout, Page Break) and Zoom
    float rightWidth = 230.0f;
    if (ImGui::GetWindowWidth() > rightWidth + 100.0f) {
        ImGui::SameLine(ImGui::GetWindowWidth() - rightWidth);

        // View mode icons
        if (ImGui::Button("田##ViewNormBtn", ImVec2(18, 18))) { activeViewMode = 0; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Normal View");

        ImGui::SameLine();
        if (ImGui::Button("🗎##ViewLayoutBtn", ImVec2(18, 18))) { activeViewMode = 1; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Page Layout View");

        ImGui::SameLine();
        if (ImGui::Button("⊞##ViewPageBreakBtn", ImVec2(18, 18))) { activeViewMode = 2; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Page Break Preview");

        ImGui::SameLine(0, 10);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0, 8);

        // Zoom Out [-]
        if (ImGui::Button("-##ZoomOutBtn", ImVec2(18, 18))) {
            zoomScale = std::max(0.5f, zoomScale - 0.1f);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zoom Out");

        ImGui::SameLine();
        // Zoom Slider (50% to 200%)
        ImGui::SetNextItemWidth(70.0f);
        ImGui::SliderFloat("##ZoomSlider", &zoomScale, 0.5f, 2.0f, "");

        ImGui::SameLine();
        // Zoom In [+]
        if (ImGui::Button("+##ZoomInBtn", ImVec2(18, 18))) {
            zoomScale = std::min(2.0f, zoomScale + 0.1f);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zoom In");

        ImGui::SameLine();
        // Zoom Percentage label button (e.g. 100%), click to reset
        char zoomBuf[16];
        snprintf(zoomBuf, sizeof(zoomBuf), "%d%%", static_cast<int>(std::round(zoomScale * 100.0f)));
        if (ImGui::Button(zoomBuf, ImVec2(40, 18))) {
            zoomScale = 1.0f;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to reset Zoom to 100%");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void SpreadsheetUI::renderContextMenu() {
    auto sheet = book.activeSheet();

    if (ImGui::BeginPopup("GridContextMenu")) {
        if (ImGui::MenuItem(Locale::t("contextmenu.copy").c_str(), "Ctrl+C")) {
            sheet->copy();
        }
        if (ImGui::MenuItem(Locale::t("contextmenu.cut").c_str(), "Ctrl+X")) {
            sheet->cut();
        }
        if (ImGui::MenuItem(Locale::t("contextmenu.paste").c_str(), "Ctrl+V", false, sheet->hasClipboard)) {
            sheet->paste("all");
        }
        if (ImGui::MenuItem(Locale::t("contextmenu.pasteValue").c_str(), "Ctrl+Shift+V", false, sheet->hasClipboard)) {
            sheet->paste("text");
        }
        if (ImGui::MenuItem(Locale::t("contextmenu.pasteFormat").c_str(), "Ctrl+Alt+V", false, sheet->hasClipboard)) {
            sheet->paste("format");
        }

        ImGui::Separator();
        if (ImGui::MenuItem(Locale::t("contextmenu.insertRow").c_str())) {
            sheet->insert("row", 1);
        }
        if (ImGui::MenuItem(Locale::t("contextmenu.insertColumn").c_str())) {
            sheet->insert("column", 1);
        }
        if (ImGui::MenuItem(Locale::t("contextmenu.deleteRow").c_str())) {
            sheet->deleteSelected("row");
        }
        if (ImGui::MenuItem(Locale::t("contextmenu.deleteColumn").c_str())) {
            sheet->deleteSelected("column");
        }
        if (ImGui::MenuItem(Locale::t("contextmenu.deleteCellText").c_str())) {
            sheet->deleteCellText();
        }

        ImGui::Separator();
        if (ImGui::MenuItem(Locale::t("contextmenu.validation").c_str())) {
            dialogValidation.open(*sheet);
        }

        ImGui::Separator();
        bool isEditable = true;
        const Cell* c = sheet->rows.getCell(sheet->select_ri, sheet->select_ci);
        if (c) isEditable = c->editable;

        if (ImGui::MenuItem(isEditable ? Locale::t("contextmenu.cellnoneditable").c_str() : Locale::t("contextmenu.celleditable").c_str())) {
            Cell& cell = sheet->rows.getCellOrNew(sheet->select_ri, sheet->select_ci);
            cell.editable = !isEditable;
        }

        ImGui::EndPopup();
    }
}

bool SpreadsheetUI::isFastContinuousNeeded() const {
    return (scrollActivityTimer > 0.0f) ||
           isDraggingVScroll || isDraggingHScroll ||
           isDraggingSplitter || isResizingCol || isResizingRow ||
           isSelectingRange || isDraggingAutofill;
}

} // namespace xspreadsheet
