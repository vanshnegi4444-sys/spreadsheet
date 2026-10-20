#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

struct BorderStyle {
    std::string style = "thin"; // "thin", "medium", "thick", "dashed", "dotted", "double"
    std::string color = "#000000";

    bool operator==(const BorderStyle& o) const {
        return style == o.style && color == o.color;
    }
};

struct CellBorder {
    std::optional<BorderStyle> top;
    std::optional<BorderStyle> bottom;
    std::optional<BorderStyle> left;
    std::optional<BorderStyle> right;

    bool hasAny() const {
        return top.has_value() || bottom.has_value() || left.has_value() || right.has_value();
    }

    bool operator==(const CellBorder& o) const {
        return top == o.top && bottom == o.bottom && left == o.left && right == o.right;
    }
};

struct FontStyle {
    std::string name = "Arial";
    float size = 10.0f; // pt
    bool bold = false;
    bool italic = false;

    bool operator==(const FontStyle& o) const {
        return name == o.name && size == o.size && bold == o.bold && italic == o.italic;
    }
};

struct Style {
    std::string bgcolor = "#ffffff";
    std::string align = "left";     // "left", "center", "right"
    std::string valign = "middle";  // "top", "middle", "bottom"
    bool textwrap = false;
    bool strike = false;
    bool underline = false;
    std::string color = "#0a0a0a";
    std::string format = "normal";  // "normal", "text", "number", "percent", "rmb", "usd", "eur", "date", "time", "datetime", "duration"
    CellBorder border;
    FontStyle font;

    bool operator==(const Style& o) const {
        return bgcolor == o.bgcolor && align == o.align && valign == o.valign &&
               textwrap == o.textwrap && strike == o.strike && underline == o.underline &&
               color == o.color && format == o.format && border == o.border && font == o.font;
    }

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["bgcolor"] = bgcolor;
        j["align"] = align;
        j["valign"] = valign;
        j["textwrap"] = textwrap;
        j["strike"] = strike;
        j["underline"] = underline;
        j["color"] = color;
        j["format"] = format;

        nlohmann::json f;
        f["name"] = font.name;
        f["size"] = font.size;
        f["bold"] = font.bold;
        f["italic"] = font.italic;
        j["font"] = f;

        if (border.hasAny()) {
            nlohmann::json b;
            if (border.top) b["top"] = { border.top->style, border.top->color };
            if (border.bottom) b["bottom"] = { border.bottom->style, border.bottom->color };
            if (border.left) b["left"] = { border.left->style, border.left->color };
            if (border.right) b["right"] = { border.right->style, border.right->color };
            j["border"] = b;
        }
        return j;
    }

    static Style fromJson(const nlohmann::json& j) {
        Style s;
        if (j.contains("bgcolor") && j["bgcolor"].is_string()) s.bgcolor = j["bgcolor"].get<std::string>();
        if (j.contains("align") && j["align"].is_string()) s.align = j["align"].get<std::string>();
        if (j.contains("valign") && j["valign"].is_string()) s.valign = j["valign"].get<std::string>();
        if (j.contains("textwrap") && j["textwrap"].is_boolean()) s.textwrap = j["textwrap"].get<bool>();
        if (j.contains("strike") && j["strike"].is_boolean()) s.strike = j["strike"].get<bool>();
        if (j.contains("underline") && j["underline"].is_boolean()) s.underline = j["underline"].get<bool>();
        if (j.contains("color") && j["color"].is_string()) s.color = j["color"].get<std::string>();
        if (j.contains("format") && j["format"].is_string()) s.format = j["format"].get<std::string>();

        if (j.contains("font") && j["font"].is_object()) {
            const auto& f = j["font"];
            if (f.contains("name") && f["name"].is_string()) s.font.name = f["name"].get<std::string>();
            if (f.contains("size") && f["size"].is_number()) s.font.size = f["size"].get<float>();
            if (f.contains("bold") && f["bold"].is_boolean()) s.font.bold = f["bold"].get<bool>();
            if (f.contains("italic") && f["italic"].is_boolean()) s.font.italic = f["italic"].get<bool>();
        }

        if (j.contains("border") && j["border"].is_object()) {
            const auto& b = j["border"];
            auto parseBorder = [](const nlohmann::json& bj) -> std::optional<BorderStyle> {
                if (bj.is_array() && bj.size() >= 2) {
                    return BorderStyle{ bj[0].get<std::string>(), bj[1].get<std::string>() };
                }
                return std::nullopt;
            };
            if (b.contains("top")) s.border.top = parseBorder(b["top"]);
            if (b.contains("bottom")) s.border.bottom = parseBorder(b["bottom"]);
            if (b.contains("left")) s.border.left = parseBorder(b["left"]);
            if (b.contains("right")) s.border.right = parseBorder(b["right"]);
        }
        return s;
    }
};

} // namespace xspreadsheet
