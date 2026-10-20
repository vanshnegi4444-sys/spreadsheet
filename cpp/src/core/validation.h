#pragma once
#include "cell_range.h"
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace xspreadsheet {

struct Validator {
    std::string type = "list"; // "date", "number", "list", "phone", "email"
    bool required = false;
    std::string value;         // comma-separated list or value/min
    std::string value2;        // max value if between/notBetween
    std::string op = "be";     // "be", "nbe", "eq", "neq", "lt", "lte", "gt", "gte"

    std::vector<std::string> values() const;
    bool operator==(const Validator& o) const;

    // returns pair<bool valid, string messageKey>
    std::pair<bool, std::string> validate(const std::string& text) const;
};

struct ValidationRule {
    std::string mode = "cell";
    std::vector<std::string> refs; // e.g. ["A1:B5"]
    Validator validator;

    bool includes(int ri, int ci) const;
    void addRef(const std::string& ref);
    void remove(const CellRange& cr);

    nlohmann::json toJson() const;
    static ValidationRule fromJson(const nlohmann::json& j);
};

class Validations {
public:
    std::vector<ValidationRule> rules;
    std::map<std::string, std::string> errors; // "ri_ci" -> message

    std::string getError(int ri, int ci) const;
    bool validate(int ri, int ci, const std::string& text);

    void add(const std::string& mode, const std::string& ref, const Validator& validator);
    void remove(const CellRange& cr);

    const ValidationRule* get(int ri, int ci) const;
    ValidationRule* get(int ri, int ci);

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace xspreadsheet
