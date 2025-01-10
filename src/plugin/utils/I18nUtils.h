#ifndef I18NUTILS_H
#define I18NUTILS_H

#include <memory>
#include <string>
#include <unordered_map>

class I18nUtils {
public:
    inline static std::unique_ptr<I18nUtils>& getInstance() {
        static std::unique_ptr<I18nUtils> instance;
        return instance;
    }

    explicit I18nUtils(const std::string& path);
    ~I18nUtils() = default;

    inline void setDefaultLocal(const std::string& local) {
        defaultLocal = local;
    }

    inline void set(const std::string& local, const std::string& key, const std::string& value) {
        if (this->data.find(local) == this->data.end())
            this->data[local] = std::unordered_map<std::string, std::string>{};
        this->data[local].insert({key, value});
    }

    std::string get(const std::string& local, const std::string& key);
public:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;
    std::string defaultLocal{};
};

namespace I18nUtilsTools {
    inline std::string tr(const std::string& local, const std::string& key) {
        return I18nUtils::getInstance()->get(local, key);
    }
}

#endif