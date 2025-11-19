#pragma once

#include <any>
#include <memory>
#include <string>
#include <utility>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::LOICollectionAPI {
    struct APIEngineConfig {
        std::string name;
        std::string delimiters;
        std::string delimitere;
        std::string escape;

        int priority;
    };

    struct Context {
        std::unordered_map<int, std::any> params;

        template <typename... Args>
        Context(Args&&... args) {
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((params[Is] = std::forward<Args>(args)), ...);
            }(std::make_index_sequence<sizeof...(Args)>{});
        }

        Context(const Context&) = delete;
        Context(Context&&) = delete;
        Context& operator=(const Context&) = delete;
        Context& operator=(Context&&) = delete;
    };

    class IContentProcessor {
    public:
        virtual ~IContentProcessor() = default;

        virtual std::string process(const std::string& content, const Context& context) = 0;
    };
    
    class APIEngine {
    public:
        LOICOLLECTION_A_NDAPI static APIEngine& getInstance() {
            static APIEngine instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI bool has(const std::string& name) const;

        LOICOLLECTION_A_NDAPI std::string process(const std::string& content, const Context& context = {});
        LOICOLLECTION_A_NDAPI std::string get(const std::string& name, const std::string& content, const Context& context = {});

        LOICOLLECTION_A_API   void registery(std::unique_ptr<IContentProcessor> processor, const APIEngineConfig& config);

    private:
        APIEngine();
        ~APIEngine();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
