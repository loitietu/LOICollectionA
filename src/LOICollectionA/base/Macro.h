#pragma once

#ifdef LOICOLLECTION_A_EXPORTS
#define LOICOLLECTION_A_UNAPI __declspec(dllexport)
#define LOICOLLECTION_A_API [[maybe_unused]] LOICOLLECTION_A_UNAPI
#else
#define LOICOLLECTION_A_UNAPI __declspec(dllimport)
#define LOICOLLECTION_A_API [[maybe_unused]] LOICOLLECTION_A_UNAPI
#endif

#ifndef LOICOLLECTION_A_NDAPI
#define LOICOLLECTION_A_NDAPI [[nodiscard]] LOICOLLECTION_A_API
#endif