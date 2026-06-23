set(CMAKE_CXX_STANDARD 17)

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/include)

add_library(hce-cards STATIC
        src/main/cpp/desfire/Desfire.cpp
        src/main/cpp/mifareplus/MifarePlus.cpp
)

target_include_directories(hce-cards PUBLIC ${PUBLIC_INCLUDE_DIR})
target_include_directories(hce-cards PRIVATE ${PRIVATE_SOURCE_DIR})

target_link_libraries(hce-cards hce-core rt-lang)
