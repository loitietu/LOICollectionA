#pragma once

#ifdef LOICOLLECTION_A_EXPORTS
#define LOICOLLECTION_A_API [[maybe_unused]] __declspec(dllexport)
#else
#define LOICOLLECTION_A_API [[maybe_unused]] __declspec(dllimport)
#endif

#ifndef LOICOLLECTION_A_NDAPI
#define LOICOLLECTION_A_NDAPI [[nodiscard]] LOICOLLECTION_A_API
#endif