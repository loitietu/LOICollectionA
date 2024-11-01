#include <regex>
#include <string>

#include <ll/api/memory/Hook.h>
#include <ll/api/utils/HashUtils.h>
#include <ll/api/utils/StringUtils.h>

#include <Windows.h>

#include <fmt/core.h>
#include <fmt/color.h>

#include "include/BuiltPlugin.h"

std::string getColorCode(std::string color) {
    switch (ll::hash_utils::doHash(color)) {
        case ll::hash_utils::doHash("0"):
            return fmt::format(fmt::fg(fmt::color::black), "{}", "");
        case ll::hash_utils::doHash("1"):
            return fmt::format(fmt::fg(fmt::color::dark_blue), "{}", "");
        case ll::hash_utils::doHash("2"):
            return fmt::format(fmt::fg(fmt::color::dark_green), "{}", "");
        case ll::hash_utils::doHash("3"):
            return fmt::format(fmt::fg(fmt::color::alice_blue), "{}", "");
        case ll::hash_utils::doHash("4"):
            return fmt::format(fmt::fg(fmt::color::dark_red), "{}", "");
        case ll::hash_utils::doHash("5"):
            return fmt::format(fmt::fg(fmt::color::medium_purple), "{}", "");
        case ll::hash_utils::doHash("6"):
            return fmt::format(fmt::fg(fmt::color::gold), "{}", "");
        case ll::hash_utils::doHash("7"):
            return fmt::format(fmt::fg(fmt::color::gray), "{}", "");
        case ll::hash_utils::doHash("8"):
            return fmt::format(fmt::fg(fmt::color::dark_gray), "{}", "");
        case ll::hash_utils::doHash("9"):
            return fmt::format(fmt::fg(fmt::color::blue), "{}", "");
        case ll::hash_utils::doHash("a"):
            return fmt::format(fmt::fg(fmt::color::green), "{}", "");
        case ll::hash_utils::doHash("b"):
            return fmt::format(fmt::fg(fmt::color::aqua), "{}", "");
        case ll::hash_utils::doHash("c"):
            return fmt::format(fmt::fg(fmt::color::red), "{}", "");
        case ll::hash_utils::doHash("d"):
            return fmt::format(fmt::fg(fmt::color::purple), "{}", "");
        case ll::hash_utils::doHash("e"):
            return fmt::format(fmt::fg(fmt::color::yellow), "{}", "");
        case ll::hash_utils::doHash("f"):
            return fmt::format(fmt::fg(fmt::color::white), "{}", "");
        case ll::hash_utils::doHash("g"):
            return fmt::format(fmt::fg(fmt::color::golden_rod), "{}", "");
    };
    return "";
}

LL_STATIC_HOOK(
    WriteConsoleWHook,
    HookPriority::Highest,
    GetProcAddress(GetModuleHandle(L"kernel32.dll"), "WriteConsoleW"),
    BOOL,
    HANDLE h,
    const void* x,
    DWORD d,
    LPDWORD lpd,
    LPVOID lpv
) {
    if (x != nullptr) {
        auto wchar = (wchar_t*) x;
        std::string str = ll::string_utils::wstr2str(wchar);

        size_t mIndex = 0;
        std::smatch mMatch;
        std::regex mPattern("§(.*?)");
        while (std::regex_search(str.cbegin() + mIndex, str.cend(), mMatch, mPattern)) {
            std::string extractedContent = mMatch.str(1);
            std::string colorCode = getColorCode(extractedContent);
            if (!colorCode.empty())
                ll::string_utils::replaceAll(str, "§" + extractedContent, colorCode);
            mIndex = mMatch.position() + mMatch.length();
        };

        auto wstr = ll::string_utils::str2wstr(str);
        return origin(h, wstr.c_str(), d, lpd, lpv);
    }
    return origin(h, x, d, lpd, lpv);
};

namespace LOICollection::BuiltPlugin {
    bool ColorLogEnable = false;
    
    void setColorLog(bool enable) {
        ColorLogEnable = enable;
    }

    void registery() {
        if (ColorLogEnable) WriteConsoleWHook::hook();
    }

    void unregistery() {
        if (ColorLogEnable) WriteConsoleWHook::unhook();
    }
}