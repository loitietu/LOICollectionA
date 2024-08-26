#ifndef JSONUTILS_H
#define JSONUTILS_H

#include <string>
#include <vector>
#include <cstddef>
#include <filesystem>
#include <string_view>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

class JsonUtils {
public:
    JsonUtils(const std::filesystem::path& path);
    ~JsonUtils() {
        this->d_path.clear();
        this->d_json.clear();
    }

    bool del(std::string_view key);

    template<typename T>
    T get(std::string_view key);

    bool has(std::string_view key);
    bool empty();
    bool reset();

    void set(std::string_view key, const std::string& value);
    void set(std::string_view key, nlohmann::ordered_json& value);
    void save();
    
    std::vector<std::string> keys();

    size_t size();

    std::string toString(int indent = 0);
    nlohmann::ordered_json toJson(std::string key);
    nlohmann::ordered_json toJson();
private:
    std::filesystem::path d_path;
    nlohmann::ordered_json d_json = nlohmann::ordered_json::object();
};

#endif