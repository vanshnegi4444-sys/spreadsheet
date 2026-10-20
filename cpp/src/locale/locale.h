#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace xspreadsheet {

class Locale {
public:
    static void setLanguage(const std::string& lang);
    static std::string getLanguage();
    static const std::vector<std::pair<std::string, std::string>>& getAvailableLanguages();

    static std::string t(const std::string& key);

private:
    static void init();
};

} // namespace xspreadsheet
