cmake_minimum_required(VERSION 3.17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(NOT DEFINED BUILD_PROJECT_VERSION)
  set(BUILD_PROJECT_VERSION "0.0.0")
endif()

project(hce-lab VERSION "${BUILD_PROJECT_VERSION}" LANGUAGES C CXX)

#-------------------------------------------------------------------------------
# build flags
#-------------------------------------------------------------------------------
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer" CACHE INTERNAL "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer" CACHE INTERNAL "" FORCE)

set(CMAKE_C_FLAGS_RELEASE "-O3 -fno-math-errno -falign-functions=32 -falign-loops=32 -march=native -fopenmp" CACHE INTERNAL "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-math-errno -falign-functions=32 -falign-loops=32 -march=native -fopenmp" CACHE INTERNAL "" FORCE)

if (UNIX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
else(WIN32)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wa,-mbig-obj")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wa,-mbig-obj")
endif()

message(STATUS "Build for ${CMAKE_SYSTEM_PROCESSOR} in ${CMAKE_BUILD_TYPE} mode")

# customize CPU architecture options
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
  message(STATUS "Enabled SSE/SSE3 instruction set")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse -msse3 -mno-avx")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse -msse3 -mno-avx")
endif ()

#-------------------------------------------------------------------------------
# find dependency libusb-1.0
#-------------------------------------------------------------------------------
find_package(libusb-1.0)

if(NOT LIBUSB_FOUND)
  message(FATAL_ERROR  "Please install libusb-1.0!")
endif()

message(STATUS "libusb-1.0: found and using dynamic link mode")
message(STATUS "  include: " ${LIBUSB_INCLUDE})
message(STATUS "  library: " ${LIBUSB_LIBRARY})

include_directories(${LIBUSB_INCLUDE})

#-------------------------------------------------------------------------------
# find dependency ftdi1
#-------------------------------------------------------------------------------
find_package(libftdi1)

if(LIBFTDI_FOUND)
  message(STATUS "libftdi1: found and using dynamic link mode")
  message(STATUS "  include: " ${LIBFTDI_INCLUDE})
  message(STATUS "  library: " ${LIBFTDI_LIBRARY})
  include_directories(${LIBFTDI_INCLUDE})
else ()
  message(STATUS  "libftdi1: not found, using local bundle in static mode")
endif()

#-------------------------------------------------------------------------------
# configure resources
#-------------------------------------------------------------------------------
add_subdirectory(src)

