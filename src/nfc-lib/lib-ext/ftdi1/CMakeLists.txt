set(CMAKE_C_STANDARD 99)

set(PUBLIC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/c)

add_library(ftdi1 STATIC
        src/main/c/ftdi.c
        src/main/c/ftdi_stream.c
        )

target_include_directories(ftdi1 PUBLIC ${PUBLIC_SOURCE_DIR})
target_include_directories(ftdi1 PRIVATE ${LIBUSB_INCLUDE_DIR})

target_link_libraries(ftdi1 ${LIBUSB_LIBRARY})
