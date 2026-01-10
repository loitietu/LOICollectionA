#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::LOICollectionAPI {
    class APIUtils {
    public:
        LOICOLLECTION_A_NDAPI static APIUtils& getInstance();

        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string()> callback);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(Player&)> callback);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(const std::vector<std::string>&)> callback);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(Player&, const std::vector<std::string>&)> callback);

        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name);
        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, Player& player);
        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, const std::vector<std::string>& parameter);
        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, Player& player, const std::vector<std::string>& parameter);

        LOICOLLECTION_A_API   std::string translate(const std::string& str, Player& player);
        LOICOLLECTION_A_API   std::string translate(const std::string& str);

    private:
        APIUtils();
        ~APIUtils();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}