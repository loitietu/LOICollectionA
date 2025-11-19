#pragma once

#if defined(_WIN32)
    #ifdef LOICOLLECTION_A_EXPORTS 
        #define LOICOLLECTION_A_ORAPI __declspec(dllexport)
    #else
        #define LOICOLLECTION_A_ORAPI __declspec(dllimport)
    #endif
    #define LOICOLLECTION_A_NDAPI [[nodiscard]] LOICOLLECTION_A_ORAPI
    #define LOICOLLECTION_A_NOINLINE __declspec(noinline)
#else
    #define LOICOLLECTION_A_ORAPI __attribute__((visibility("default")))
    #define LOICOLLECTION_A_NDAPI [[nodiscard]] LOICOLLECTION_A_ORAPI
    #define LOICOLLECTION_A_NOINLINE __attribute__((noinline))
#endif

#define LOICOLLECTION_A_API LOICOLLECTION_A_ORAPI LOICOLLECTION_A_NOINLINE
