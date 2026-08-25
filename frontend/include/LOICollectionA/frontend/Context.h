#pragma once

#include <any>
#include <unordered_map>

namespace LOICollection::frontend {
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
}