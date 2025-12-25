set(CMAKE_CXX_STANDARD 17)

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)

# Build for WIN32 / UNIX-LINUX
add_executable(hce-t4t src/main/cpp/main-pn7160.cpp)

target_include_directories(hce-t4t PRIVATE ${PRIVATE_SOURCE_DIR})
target_include_directories(hce-t4t PRIVATE ${AUTOGEN_BUILD_DIR}/include)

if (WIN32)
    set(PLATFORM_LIBS mingw32 psapi)
endif (WIN32)

target_link_libraries(hce-t4t hce-tasks hce-targets hce-core hw-dev rt-lang ${PLATFORM_LIBS})

# Install targets
if (UNIX)
    install(TARGETS hce-t4t DESTINATION bin)
endif (UNIX)
