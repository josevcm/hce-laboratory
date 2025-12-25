set(CMAKE_CXX_STANDARD 17)

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/include)

add_library(hce-core STATIC
        src/main/cpp/Frame.cpp
        src/main/cpp/Target.cpp
        src/main/cpp/crc/CRC.cpp
        src/main/cpp/crypto/CMAC.cpp
        src/main/cpp/crypto/Cipher.cpp
        src/main/cpp/crypto/CipherAES.cpp
        src/main/cpp/crypto/CipherDES.cpp
)

target_include_directories(hce-core PUBLIC ${PUBLIC_INCLUDE_DIR})
target_include_directories(hce-core PRIVATE ${PRIVATE_SOURCE_DIR})

target_link_libraries(hce-core rt-lang cppdes microaes)