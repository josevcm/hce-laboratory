set(CMAKE_CXX_STANDARD 17)

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/include)

add_library(hce-targets STATIC
        # T4T implementation  ---
        src/main/cpp/t4t/T4T.cpp
)

target_include_directories(hce-targets PUBLIC ${PUBLIC_INCLUDE_DIR})
target_include_directories(hce-targets PRIVATE ${PRIVATE_SOURCE_DIR})

target_link_libraries(hce-targets hce-core rt-lang)