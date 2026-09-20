set(CMAKE_C_STANDARD 99)

add_library(kftdi STATIC src/main/c/libkftdi_mpsse.c)

target_include_directories(kftdi PUBLIC src/main/include)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "[Aa][Rr][Mm]64|aarch64|AARCH64")
        set(FTD2XX_ARCH "ARM64")
    else()
        set(FTD2XX_ARCH "x64")
    endif()
else()
    set(FTD2XX_ARCH "Win32")
endif()

set(FTD2XX_LIB ${CMAKE_CURRENT_SOURCE_DIR}/dll/${FTD2XX_ARCH}/ftd2xx.lib)

if (NOT EXISTS ${FTD2XX_LIB})
    message(FATAL_ERROR "ftd2xx import lib not found for ${FTD2XX_ARCH}: ${FTD2XX_LIB}")
endif()

message(STATUS "kftdi: linking FTDI D2XX (${FTD2XX_ARCH})")

target_link_libraries(kftdi PUBLIC ${FTD2XX_LIB})
