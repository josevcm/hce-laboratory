set(CMAKE_CXX_STANDARD 17)

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/include)

add_library(hce-tasks STATIC
        src/main/cpp/tasks/TargetListenerTask.cpp
        )

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    target_compile_options(hce-tasks PRIVATE "-msse2" -DUSE_SSE2)
endif ()

target_include_directories(hce-tasks PUBLIC ${PUBLIC_INCLUDE_DIR})
target_include_directories(hce-tasks PRIVATE ${PRIVATE_SOURCE_DIR})

target_link_libraries(hce-tasks hce-targets hw-dev rt-lang nlohmann)