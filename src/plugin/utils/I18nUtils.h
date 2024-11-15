#ifndef I18NUTILS_H
#define I18NUTILS_H

#include <vector>
#include <string>
#include <filesystem>

namespace I18nUtils {
    void load(const std::filesystem::path& path);
    void setDefaultLocal(const std::string& local);

    std::string tr(std::string local, std::string key);

    std::vector<std::string> keys();
}

#endif