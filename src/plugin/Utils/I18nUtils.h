#ifndef I18NUTILS_H
#define I18NUTILS_H

#include <string>
#include <vector>
#include <filesystem>

namespace I18nUtils {
    void load(std::filesystem::path path);

    std::string tr(const std::string& local, const std::string& key);
    std::string getName(const std::string& local);
    std::string getLocalFromName(const std::string& name);

    std::vector<std::string> keys();
}

#endif