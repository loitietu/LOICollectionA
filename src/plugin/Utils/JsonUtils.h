#ifndef JSONUTILS_H
#define JSONUTILS_H

#include <vector>
#include <filesystem>
#include <string_view>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

class JsonUtils {
public:
    explicit JsonUtils(const std::filesystem::path& path);
    ~JsonUtils() = default;

    bool remove(std::string_view key);
    bool has(std::string_view key) const;
    bool isEmpty() const;

    template <typename T>
    T get(std::string_view key) const {
        return this->d_json.at(key).get<T>();
    }

    template <typename T>
    void set(std::string_view key, T value) {
        this->d_json[key] = value;
    }

    void write(nlohmann::ordered_json& json);
    void save() const;
    
    std::vector<std::string> keys() const;

    std::string toString(int indent = 0) const;
    nlohmann::ordered_json toJson(std::string_view key) const;
    nlohmann::ordered_json toJson() const;
private:
    std::filesystem::path d_path;
    nlohmann::ordered_json d_json = nlohmann::ordered_json::object();
};

#endif