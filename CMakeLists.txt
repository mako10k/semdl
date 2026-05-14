cmake_minimum_required(VERSION 3.20)

project(semdl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(semdl_core STATIC
    src/core/document_store.cpp
    src/core/extract_embeddings.cpp
    src/core/query_validation.cpp
    src/core/semantic_model.cpp
)

target_include_directories(semdl_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(semdl_core PUBLIC cxx_std_20)

add_executable(ssd
    src/cli/main.cpp
    src/cli/cli_app.cpp
)

target_link_libraries(ssd PRIVATE semdl_core)

add_executable(semdl_runner
    src/runner/main.cpp
    src/runner/discovery.cpp
    src/runner/manifest.cpp
    src/runner/executor.cpp
)

target_include_directories(semdl_runner
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(semdl_runner
    PRIVATE
        SEMDL_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)