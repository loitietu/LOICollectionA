#pragma once

#if defined(_WIN32)
    #define LOICOLLECTION_A_NOINLINE __declspec(noinline)
    #ifdef LOICOLLECTION_A_EXPORTS 
        #define LOICOLLECTION_A_API __declspec(dllexport) LOICOLLECTION_A_NOINLINE
    #else
        #define LOICOLLECTION_A_API __declspec(dllimport) LOICOLLECTION_A_NOINLINE
    #endif
    #define LOICOLLECTION_A_NDAPI [[nodiscard]] LOICOLLECTION_A_API
#else
    #define LOICOLLECTION_A_NOINLINE __attribute__((noinline))
    #ifdef LOICOLLECTION_A_EXPORTS
        #define LOICOLLECTION_A_API __attribute__((visibility("default"))) LOICOLLECTION_A_NOINLINE
    #else
        #define LOICOLLECTION_A_API LOICOLLECTION_A_NOINLINE
    #endif
    #define LOICOLLECTION_A_NDAPI [[nodiscard]] LOICOLLECTION_A_API
#endif
