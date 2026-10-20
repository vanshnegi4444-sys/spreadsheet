#include "validation.h"
#include <sstream>
#include <regex>
#include <algorithm>

namespace xspreadsheet {

std::vector<std::string> Validator::values() const {
    std::vector<std::string> res;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t s = item.find_first_not_of(" \t");
        size_t e = item.find_last_not_of(" \t");
        if (s != std::string::npos && e != std::string::npos) {
            res.push_back(item.substr(s, e - s + 1));
        } else if (!item.empty()) {
            res.push_back(item);
        }
    }
    return res;
}

bool Validator::operator==(const Validator& o) const {
    return type == o.type && required == o.required && op == o.op && value == o.value && value2 == o.value2;
}

std::pair<bool, std::string> Validator::validate(const std::string& text) const {
    if (required && text.empty()) {
        return { false, "validation.required" };
    }
    if (text.empty()) {
        return { true, "" };
    }

    if (type == "phone") {
        static const std::regex phoneRegex("^[1-9]\\d{10}$");
        if (!std::regex_match(text, phoneRegex)) {
            return { false, "validation.notMatch" };
        }
    } else if (type == "email") {
        static const std::regex emailRegex("^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+$");
        if (!std::regex_match(text, emailRegex)) {
            return { false, "validation.notMatch" };
        }
    } else if (type == "list") {
        auto listVals = values();
        if (std::find(listVals.begin(), listVals.end(), text) == listVals.end()) {
            return { false, "validation.notIn" };
        }
    } else if (type == "number") {
        char* end = nullptr;
        double v = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0') {
            return { false, "validation.notMatch" };
        }
        double minV = std::strtod(value.c_str(), nullptr);
        double maxV = std::strtod(value2.c_str(), nullptr);

        if (op == "be") {
            if (!(v >= minV && v <= maxV)) return { false, "validation.between" };
        } else if (op == "nbe") {
            if (v >= minV && v <= maxV) return { false, "validation.notBetween" };
        } else if (op == "eq") {
            if (v != minV) return { false, "validation.equal" };
        } else if (op == "neq") {
            if (v == minV) return { false, "validation.notEqual" };
        } else if (op == "lt") {
            if (v >= minV) return { false, "validation.lessThan" };
        } else if (op == "lte") {
            if (v > minV) return { false, "validation.lessThanEqual" };
        } else if (op == "gt") {
            if (v <= minV) return { false, "validation.greaterThan" };
        } else if (op == "gte") {
            if (v < minV) return { false, "validation.greaterThanEqual" };
        }
    } else if (type == "date") {
        if (op == "be") {
            if (!(text >= value && text <= value2)) return { false, "validation.between" };
        } else if (op == "nbe") {
            if (text >= value && text <= value2) return { false, "validation.notBetween" };
        } else if (op == "eq") {
            if (text != value) return { false, "validation.equal" };
        } else if (op == "neq") {
            if (text == value) return { false, "validation.notEqual" };
        } else if (op == "lt") {
            if (text >= value) return { false, "validation.lessThan" };
        } else if (op == "lte") {
            if (text > value) return { false, "validation.lessThanEqual" };
        } else if (op == "gt") {
            if (text <= value) return { false, "validation.greaterThan" };
        } else if (op == "gte") {
            if (text < value) return { false, "validation.greaterThanEqual" };
        }
    }

    return { true, "" };
}

bool ValidationRule::includes(int ri, int ci) const {
    for (const auto& ref : refs) {
        if (CellRange::valueOf(ref).includes(ri, ci)) return true;
    }
    return false;
}

void ValidationRule::addRef(const std::string& ref) {
    remove(CellRange::valueOf(ref));
    refs.push_back(ref);
}

void ValidationRule::remove(const CellRange& cr) {
    std::vector<std::string> nrefs;
    for (const auto& r : refs) {
        CellRange existing = CellRange::valueOf(r);
        if (existing.intersects(cr)) {
            auto diffs = existing.difference(cr);
            for (const auto& d : diffs) {
                nrefs.push_back(d.toString());
            }
        } else {
            nrefs.push_back(r);
        }
    }
    refs = std::move(nrefs);
}

nlohmann::json ValidationRule::toJson() const {
    nlohmann::json j;
    j["mode"] = mode;
    j["refs"] = refs;
    j["type"] = validator.type;
    j["required"] = validator.required;
    j["operator"] = validator.op;
    if (validator.op == "be" || validator.op == "nbe") {
        j["value"] = { validator.value, validator.value2 };
    } else {
        j["value"] = validator.value;
    }
    return j;
}

ValidationRule ValidationRule::fromJson(const nlohmann::json& j) {
    ValidationRule r;
    if (j.contains("mode") && j["mode"].is_string()) r.mode = j["mode"].get<std::string>();
    if (j.contains("refs") && j["refs"].is_array()) {
        for (const auto& it : j["refs"]) {
            if (it.is_string()) r.refs.push_back(it.get<std::string>());
        }
    }
    if (j.contains("type") && j["type"].is_string()) r.validator.type = j["type"].get<std::string>();
    if (j.contains("required") && j["required"].is_boolean()) r.validator.required = j["required"].get<bool>();
    if (j.contains("operator") && j["operator"].is_string()) r.validator.op = j["operator"].get<std::string>();
    if (j.contains("value")) {
        if (j["value"].is_array()) {
            if (j["value"].size() >= 1 && j["value"][0].is_string()) r.validator.value = j["value"][0].get<std::string>();
            if (j["value"].size() >= 2 && j["value"][1].is_string()) r.validator.value2 = j["value"][1].get<std::string>();
        } else if (j["value"].is_string()) {
            r.validator.value = j["value"].get<std::string>();
        }
    }
    return r;
}

std::string Validations::getError(int ri, int ci) const {
    std::string key = std::to_string(ri) + "_" + std::to_string(ci);
    auto it = errors.find(key);
    if (it != errors.end()) return it->second;
    return "";
}

bool Validations::validate(int ri, int ci, const std::string& text) {
    std::string key = std::to_string(ri) + "_" + std::to_string(ci);
    const ValidationRule* rule = get(ri, ci);
    if (!rule) {
        errors.erase(key);
        return true;
    }
    auto [ok, msg] = rule->validator.validate(text);
    if (!ok) {
        errors[key] = msg;
        return false;
    } else {
        errors.erase(key);
        return true;
    }
}

void Validations::add(const std::string& mode, const std::string& ref, const Validator& validator) {
    for (auto& rule : rules) {
        if (rule.validator == validator) {
            rule.addRef(ref);
            return;
        }
    }
    ValidationRule rule;
    rule.mode = mode;
    rule.refs = { ref };
    rule.validator = validator;
    rules.push_back(std::move(rule));
}

void Validations::remove(const CellRange& cr) {
    std::vector<ValidationRule> next;
    for (auto& r : rules) {
        r.remove(cr);
        if (!r.refs.empty()) {
            next.push_back(std::move(r));
        }
    }
    rules = std::move(next);
}

const ValidationRule* Validations::get(int ri, int ci) const {
    for (const auto& r : rules) {
        if (r.includes(ri, ci)) return &r;
    }
    return nullptr;
}

ValidationRule* Validations::get(int ri, int ci) {
    for (auto& r : rules) {
        if (r.includes(ri, ci)) return &r;
    }
    return nullptr;
}

nlohmann::json Validations::toJson() const {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& r : rules) {
        if (!r.refs.empty()) {
            j.push_back(r.toJson());
        }
    }
    return j;
}

void Validations::fromJson(const nlohmann::json& j) {
    rules.clear();
    errors.clear();
    if (j.is_array()) {
        for (const auto& rj : j) {
            rules.push_back(ValidationRule::fromJson(rj));
        }
    }
}

} // namespace xspreadsheet
