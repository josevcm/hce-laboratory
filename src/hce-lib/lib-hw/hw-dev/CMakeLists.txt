set(CMAKE_CXX_STANDARD 17)

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/include)

if (HCE_MPSSE_BACKEND STREQUAL "d2xx")
    set(MPSSE_SRC  src/main/cpp/hw/proto/MPSSE.d2xx.cpp)
    set(MPSSE_LIBS kftdi)
else()
    set(MPSSE_SRC  src/main/cpp/hw/proto/MPSSE.cpp)
    set(MPSSE_LIBS ${LIBFTDI_LIBRARIES})
endif()

add_library(hw-dev STATIC
        src/main/cpp/hw/dev/ACR122U.cpp
        src/main/cpp/hw/dev/PCSC.cpp
        src/main/cpp/hw/dev/Usb.cpp
        src/main/cpp/hw/proto/HSU.cpp
        ${MPSSE_SRC}
        src/main/cpp/hw/ic/PN7160.cpp
        src/main/cpp/hw/ic/PN532.cpp
)

target_include_directories(hw-dev PUBLIC ${PUBLIC_INCLUDE_DIR})
target_include_directories(hw-dev PRIVATE ${PRIVATE_SOURCE_DIR})
target_include_directories(hw-dev PRIVATE ${LIBUSB_INCLUDE_DIRS})

if (WIN32)
    set(PLATFORM_LIBS winscard)
endif (WIN32)

target_link_libraries(hw-dev rt-lang ${PLATFORM_LIBS} ${MPSSE_LIBS})
