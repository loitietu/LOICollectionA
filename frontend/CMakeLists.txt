cmake_minimum_required(VERSION 3.20)

project(LOICollectionAFrontend
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Standalone build of the LCUI language frontend core. It has no dependency on
# the LeviLamina SDK: ll::Expected is replaced with std::expected and native
# class implementations are kept as declarations only.

add_library(loicollectiona_frontend_core STATIC
    src/LOICollectionA/frontend/Callback.cpp
    src/LOICollectionA/frontend/Lexer.cpp
    src/LOICollectionA/frontend/Parser.cpp
    src/LOICollectionA/frontend/SemanticAnalyzer.cpp
    src/LOICollectionA/frontend/ir/BytecodeCache.cpp
    src/LOICollectionA/frontend/ir/Compiler.cpp
    src/LOICollectionA/frontend/ir/Optimizer.cpp
    src/LOICollectionA/frontend/ir/VM.cpp
    src/LOICollectionA/utils/core/MathUtils.cpp
)

target_include_directories(loicollectiona_frontend_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(loicollectiona_frontend_core PRIVATE -Wno-trigraphs)
endif()

# ---- Tests ----
include(FetchContent)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)

enable_testing()

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

add_executable(loicollectiona_frontend_tests ${TEST_SOURCES})
target_link_libraries(loicollectiona_frontend_tests
    PRIVATE loicollectiona_frontend_core gtest_main
)
target_include_directories(loicollectiona_frontend_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

include(GoogleTest)
gtest_discover_tests(loicollectiona_frontend_tests)
