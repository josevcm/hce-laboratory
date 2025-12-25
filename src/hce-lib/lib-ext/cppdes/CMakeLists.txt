set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/include)

add_library(cppdes STATIC
        src/main/cpp/des.cpp
        src/main/cpp/des3.cpp
        src/main/cpp/des3cbc.cpp
        src/main/cpp/descbc.cpp)

target_include_directories(cppdes PUBLIC ${PUBLIC_INCLUDE_DIR})
target_include_directories(cppdes PRIVATE ${PRIVATE_SOURCE_DIR})
