#pragma once

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

    void set(const std::string& local, const std::string& key, const std::string& value);

    [[nodiscard]] bool has(const std::string& local) const;

    [[nodiscard]] std::string get(const std::string& local, const std::string& key) const;
    
public:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;
    std::string defaultLocale{};
};

namespace I18nUtilsTools {
    inline std::string tr(const std::string& local, const std::string& key) {
        return I18nUtils::getInstance()->get(
            I18nUtils::getInstance()->has(local) ? local : I18nUtils::getInstance()->defaultLocale, key
        );
    }
}