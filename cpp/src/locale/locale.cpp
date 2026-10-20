#include "locale.h"

namespace xspreadsheet {

static std::string s_currentLang = "en";
static bool s_initialized = false;
static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> s_strings;

void Locale::init() {
    if (s_initialized) return;
    s_initialized = true;

    // English
    auto& en = s_strings["en"];
    en["toolbar.undo"] = "Undo";
    en["toolbar.redo"] = "Redo";
    en["toolbar.print"] = "Print";
    en["toolbar.paintformat"] = "Paint format";
    en["toolbar.clearformat"] = "Clear format";
    en["toolbar.format"] = "Format";
    en["toolbar.fontName"] = "Font";
    en["toolbar.fontSize"] = "Font size";
    en["toolbar.fontBold"] = "Font bold";
    en["toolbar.fontItalic"] = "Font italic";
    en["toolbar.underline"] = "Underline";
    en["toolbar.strike"] = "Strike";
    en["toolbar.color"] = "Text color";
    en["toolbar.bgcolor"] = "Fill color";
    en["toolbar.border"] = "Borders";
    en["toolbar.merge"] = "Merge cells";
    en["toolbar.align"] = "Horizontal align";
    en["toolbar.valign"] = "Vertical align";
    en["toolbar.textwrap"] = "Text wrapping";
    en["toolbar.freeze"] = "Freeze cell";
    en["toolbar.autofilter"] = "Filter";
    en["toolbar.formula"] = "Functions";
    en["toolbar.more"] = "More";

    en["contextmenu.copy"] = "Copy";
    en["contextmenu.cut"] = "Cut";
    en["contextmenu.paste"] = "Paste";
    en["contextmenu.pasteValue"] = "Paste values only";
    en["contextmenu.pasteFormat"] = "Paste format only";
    en["contextmenu.hide"] = "Hide";
    en["contextmenu.insertRow"] = "Insert row";
    en["contextmenu.insertColumn"] = "Insert column";
    en["contextmenu.deleteSheet"] = "Delete sheet";
    en["contextmenu.deleteRow"] = "Delete row";
    en["contextmenu.deleteColumn"] = "Delete column";
    en["contextmenu.deleteCell"] = "Delete cell";
    en["contextmenu.deleteCellText"] = "Delete cell text";
    en["contextmenu.validation"] = "Data validations";
    en["contextmenu.cellprintable"] = "Enable export";
    en["contextmenu.cellnonprintable"] = "Disable export";
    en["contextmenu.celleditable"] = "Enable editing";
    en["contextmenu.cellnoneditable"] = "Disable editing";

    en["print.size"] = "Paper size";
    en["print.orientation"] = "Page orientation";

    en["format.normal"] = "Normal";
    en["format.text"] = "Plain Text";
    en["format.number"] = "Number";
    en["format.percent"] = "Percent";
    en["format.rmb"] = "RMB";
    en["format.usd"] = "USD";
    en["format.eur"] = "EUR";
    en["format.date"] = "Date";
    en["format.time"] = "Time";
    en["format.datetime"] = "Date time";
    en["format.duration"] = "Duration";

    en["formula.sum"] = "Sum";
    en["formula.average"] = "Average";
    en["formula.max"] = "Max";
    en["formula.min"] = "Min";
    en["formula._if"] = "IF";
    en["formula.and"] = "AND";
    en["formula.or"] = "OR";
    en["formula.concat"] = "Concat";

    en["validation.required"] = "it must be required";
    en["validation.notMatch"] = "it not match its validation rule";
    en["validation.between"] = "it is between min and max";
    en["validation.notBetween"] = "it is not between min and max";
    en["validation.notIn"] = "it is not in list";
    en["validation.equal"] = "it equal to target";
    en["validation.notEqual"] = "it not equal to target";
    en["validation.lessThan"] = "it less than target";
    en["validation.lessThanEqual"] = "it less than or equal to target";
    en["validation.greaterThan"] = "it greater than target";
    en["validation.greaterThanEqual"] = "it greater than or equal to target";

    en["sort.desc"] = "Sort Z -> A";
    en["sort.asc"] = "Sort A -> Z";
    en["filter.empty"] = "(Blanks)";

    // Chinese (zh-cn)
    auto& zh = s_strings["zh-cn"];
    zh["toolbar.undo"] = "撤销";
    zh["toolbar.redo"] = "恢复";
    zh["toolbar.print"] = "打印";
    zh["toolbar.paintformat"] = "格式刷";
    zh["toolbar.clearformat"] = "清除格式";
    zh["toolbar.format"] = "数据格式";
    zh["toolbar.fontName"] = "字体";
    zh["toolbar.fontSize"] = "字号";
    zh["toolbar.fontBold"] = "加粗";
    zh["toolbar.fontItalic"] = "倾斜";
    zh["toolbar.underline"] = "下划线";
    zh["toolbar.strike"] = "删除线";
    zh["toolbar.color"] = "字体颜色";
    zh["toolbar.bgcolor"] = "填充颜色";
    zh["toolbar.border"] = "边框";
    zh["toolbar.merge"] = "合并单元格";
    zh["toolbar.align"] = "水平对齐";
    zh["toolbar.valign"] = "垂直对齐";
    zh["toolbar.textwrap"] = "自动换行";
    zh["toolbar.freeze"] = "冻结";
    zh["toolbar.autofilter"] = "自动筛选";
    zh["toolbar.formula"] = "函数";
    zh["toolbar.more"] = "更多";

    zh["contextmenu.copy"] = "复制";
    zh["contextmenu.cut"] = "剪切";
    zh["contextmenu.paste"] = "粘贴";
    zh["contextmenu.pasteValue"] = "粘贴数据";
    zh["contextmenu.pasteFormat"] = "粘贴格式";
    zh["contextmenu.hide"] = "隐藏";
    zh["contextmenu.insertRow"] = "插入行";
    zh["contextmenu.insertColumn"] = "插入列";
    zh["contextmenu.deleteSheet"] = "删除工作表";
    zh["contextmenu.deleteRow"] = "删除行";
    zh["contextmenu.deleteColumn"] = "删除列";
    zh["contextmenu.deleteCell"] = "删除";
    zh["contextmenu.deleteCellText"] = "删除数据";
    zh["contextmenu.validation"] = "数据验证";
    zh["contextmenu.cellprintable"] = "可打印";
    zh["contextmenu.cellnonprintable"] = "不可打印";
    zh["contextmenu.celleditable"] = "可编辑";
    zh["contextmenu.cellnoneditable"] = "不可编辑";

    zh["sort.desc"] = "降序";
    zh["sort.asc"] = "升序";
    zh["filter.empty"] = "空白";

    // German (de)
    auto& de = s_strings["de"];
    de["toolbar.undo"] = "Rückgängig machen";
    de["toolbar.redo"] = "Wiederherstellen";
    de["toolbar.print"] = "Drucken";
    de["toolbar.paintformat"] = "Format kopieren";
    de["toolbar.clearformat"] = "Format löschen";
    de["toolbar.format"] = "Format";
    de["toolbar.fontName"] = "Schriftart";
    de["toolbar.fontSize"] = "Schriftgrad";
    de["toolbar.fontBold"] = "Fett";
    de["toolbar.fontItalic"] = "Kursiv";
    de["toolbar.underline"] = "Unterstrichen";
    de["toolbar.strike"] = "Durchgestrichen";
    de["toolbar.color"] = "Textfarbe";
    de["toolbar.bgcolor"] = "Füllfarbe";
    de["toolbar.border"] = "Umrandung";
    de["toolbar.merge"] = "Zellen verbinden";
    de["toolbar.align"] = "Ausrichtung";
    de["toolbar.valign"] = "Vertikal";
    de["toolbar.textwrap"] = "Textumbruch";
    de["toolbar.freeze"] = "Zelle sperren";
    de["toolbar.autofilter"] = "Filter";
    de["toolbar.formula"] = "Funktionen";

    // Dutch (nl)
    auto& nl = s_strings["nl"];
    nl["toolbar.undo"] = "Ongedaan maken";
    nl["toolbar.redo"] = "Opnieuw uitvoeren";
    nl["toolbar.print"] = "Afdrukken";
    nl["toolbar.paintformat"] = "Opmaak kopiëren";
    nl["toolbar.clearformat"] = "Opmaak wissen";
    nl["toolbar.format"] = "Opmaak";
    nl["toolbar.fontName"] = "Lettertype";
    nl["toolbar.fontSize"] = "Tekengrootte";
    nl["toolbar.fontBold"] = "Vet";
    nl["toolbar.fontItalic"] = "Cursief";
    nl["toolbar.underline"] = "Onderstrepen";
    nl["toolbar.strike"] = "Doorstrepen";
    nl["toolbar.color"] = "Tekstkleur";
    nl["toolbar.bgcolor"] = "Opvulkleur";
    nl["toolbar.border"] = "Randen";
    nl["toolbar.merge"] = "Cellen samenvoegen";
    nl["toolbar.align"] = "Uitlijning";
    nl["toolbar.valign"] = "Verticaal";
    nl["toolbar.textwrap"] = "Terugloop";
    nl["toolbar.freeze"] = "Cel bevriezen";
    nl["toolbar.autofilter"] = "Filter";
    nl["toolbar.formula"] = "Functies";
}

void Locale::setLanguage(const std::string& lang) {
    init();
    if (s_strings.count(lang)) {
        s_currentLang = lang;
    }
}

std::string Locale::getLanguage() {
    return s_currentLang;
}

const std::vector<std::pair<std::string, std::string>>& Locale::getAvailableLanguages() {
    static const std::vector<std::pair<std::string, std::string>> langs = {
        { "en", "English" },
        { "zh-cn", "简体中文" },
        { "de", "Deutsch" },
        { "nl", "Nederlands" }
    };
    return langs;
}

std::string Locale::t(const std::string& key) {
    init();
    auto itLang = s_strings.find(s_currentLang);
    if (itLang != s_strings.end()) {
        auto itStr = itLang->second.find(key);
        if (itStr != itLang->second.end()) {
            return itStr->second;
        }
    }
    // Fallback to English
    auto itEn = s_strings.find("en");
    if (itEn != s_strings.end()) {
        auto itStr = itEn->second.find(key);
        if (itStr != itEn->second.end()) {
            return itStr->second;
        }
    }
    return key;
}

} // namespace xspreadsheet
