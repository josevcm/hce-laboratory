# Integrating Protocol Buffers in a CMake C++ Project

Step-by-step guide to add Protocol Buffers (protobuf) support to any CMake-based C++17 project using `FetchContent`. This approach is self-contained: no system packages, vcpkg, or Conan required. It works on Linux and Windows (MinGW or MSVC).

## Prerequisites

- CMake 3.21 or later
- C++17 compiler (GCC, Clang, or MSVC)
- Git (FetchContent clones protobuf from GitHub)

## Step 1: Bump CMake minimum version

In your root `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.21)
```

Protobuf v5 and its Abseil dependency require CMake 3.21+.

## Step 2: Add FetchContent block for protobuf

Add this to your root `CMakeLists.txt`, before any `add_subdirectory()` calls that consume protobuf:

```cmake
#-------------------------------------------------------------------------------
# fetch and build protobuf
#-------------------------------------------------------------------------------
include(FetchContent)

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOBUF_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_TAG        v5.29.3
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(protobuf)
```

**What each option does:**

| Option | Purpose |
|--------|---------|
| `protobuf_BUILD_TESTS OFF` | Skip protobuf's own tests (saves significant build time) |
| `protobuf_BUILD_EXAMPLES OFF` | Skip example programs |
| `protobuf_BUILD_PROTOC_BINARIES ON` | Build the `protoc` compiler (needed for code generation) |
| `protobuf_BUILD_SHARED_LIBS OFF` | Static library (avoids DLL issues on Windows/MinGW) |
| `protobuf_INSTALL OFF` | Don't generate install rules for protobuf internals |
| `ABSL_PROPAGATE_CXX_STD ON` | Abseil inherits the C++ standard from your project |
| `GIT_SHALLOW TRUE` | Shallow clone to speed up download |

## Step 3: Create a proto module

Create a directory structure for your `.proto` files and the CMake target that wraps them:

```
my-proto/
  CMakeLists.txt
  src/main/proto/
    example.proto
  src/test/cpp/
    verify_proto.cpp
```

### `my-proto/CMakeLists.txt`

```cmake
# generated proto output directory
set(PROTO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GENERATED_DIR})

# collect .proto sources
file(GLOB PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/main/proto/*.proto)

# generate C++ from .proto files
set(PROTO_SRCS)
set(PROTO_HDRS)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    list(APPEND PROTO_SRCS ${PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.cc)
    list(APPEND PROTO_HDRS ${PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.h)

    add_custom_command(
        OUTPUT ${PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.cc
               ${PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.h
        COMMAND protobuf::protoc
            --cpp_out=${PROTO_GENERATED_DIR}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/src/main/proto
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE} protobuf::protoc
        COMMENT "Generating protobuf sources for ${PROTO_NAME}.proto"
    )
endforeach()

# static library with generated code
add_library(my-proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})

target_include_directories(my-proto PUBLIC ${PROTO_GENERATED_DIR})

target_link_libraries(my-proto PUBLIC protobuf::libprotobuf)

set_target_properties(my-proto PROPERTIES
    CXX_STANDARD 17
    POSITION_INDEPENDENT_CODE ON
)

# optional verification target
option(MY_PROTO_BUILD_VERIFY "Build protobuf verification target" OFF)

if(MY_PROTO_BUILD_VERIFY)
    add_executable(my-proto-verify src/test/cpp/verify_proto.cpp)
    target_link_libraries(my-proto-verify my-proto)
    set_target_properties(my-proto-verify PROPERTIES CXX_STANDARD 17)
endif()
```

**Key points:**
- `protobuf::protoc` is the imported target provided by FetchContent — it resolves to the freshly-built `protoc` binary.
- `protobuf::libprotobuf` is the protobuf runtime library. Linked as `PUBLIC` so consumers get the headers transitively.
- Generated files go into the build tree only; never committed to source control.
- The `file(GLOB ...)` picks up any new `.proto` file automatically (re-run cmake configure after adding files).

### `src/main/proto/example.proto`

```protobuf
syntax = "proto3";

package myapp.proto;

// Example message for build verification.
message Ping {
  string label = 1;
  int64 timestamp = 2;
}
```

### `src/test/cpp/verify_proto.cpp`

```cpp
#include <iostream>
#include "example.pb.h"

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    myapp::proto::Ping ping;
    ping.set_label("hello");
    ping.set_timestamp(42);

    std::string serialized;
    ping.SerializeToString(&serialized);

    myapp::proto::Ping decoded;
    decoded.ParseFromString(serialized);

    std::cout << "label: " << decoded.label() << std::endl;
    std::cout << "timestamp: " << decoded.timestamp() << std::endl;

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
```

## Step 4: Register the module

In the parent `CMakeLists.txt` that manages your libraries:

```cmake
add_subdirectory(my-proto)
```

Place it before any modules that will depend on it.

## Step 5: Use from other targets

Any module that needs protobuf messages:

```cmake
target_link_libraries(my-target my-proto)
```

```cpp
#include "example.pb.h"

void foo() {
    myapp::proto::Ping msg;
    msg.set_label("test");
    // ...
}
```

## Step 6: Build and verify

```bash
# Configure (first time downloads protobuf — needs internet)
cmake -DCMAKE_BUILD_TYPE=Debug -DMY_PROTO_BUILD_VERIFY=ON -S . -B build

# Build the verification target
cmake --build build --target my-proto-verify -- -j$(nproc)

# Run verification
./build/path/to/my-proto-verify
# Expected output:
#   label: hello
#   timestamp: 42

# Build your full project to confirm nothing is broken
cmake --build build --target your-main-app -- -j$(nproc)
```

## CI/CD

No changes needed. FetchContent downloads and builds protobuf automatically during configure. There is no dependency on system packages.

**Optional optimization**: cache the `build/_deps/` directory in your CI to avoid re-downloading on every run:

```yaml
- name: Cache FetchContent dependencies
  uses: actions/cache@v4
  with:
    key: ${{ runner.os }}-fetchcontent
    path: build/_deps
```

## Platform notes

### Windows (MinGW)

- Ensure `-Wa,-mbig-obj` is set for debug builds. Protobuf generates large translation units that can exceed the default COFF section limit:
  ```cmake
  if(WIN32)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wa,-mbig-obj")
  endif()
  ```
- Static linking (`protobuf_BUILD_SHARED_LIBS OFF`) avoids DLL export issues on MinGW.

### Linux

- No special flags needed. If using threads, ensure `-pthread` is set (most projects already have this).
- System `zlib` is optional — protobuf will print `Could NOT find ZLIB` during configure but works fine without it.

## Adding new `.proto` files

1. Create a new `.proto` file in `src/main/proto/`.
2. Re-run `cmake configure` (the `file(GLOB)` picks it up).
3. Build — the new `*.pb.h` / `*.pb.cc` are generated automatically.

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `protobuf::protoc` not found | FetchContent block is after the `add_subdirectory` that uses it | Move FetchContent block earlier in root CMakeLists.txt |
| `Could NOT find ZLIB` warning | zlib not installed (optional) | Safe to ignore — protobuf works without it |
| Slow first build (~5-10 min) | Protobuf + Abseil building from source | Normal — incremental builds are fast afterwards |
| `file too big` on MinGW | Missing `-Wa,-mbig-obj` flag | Add it to debug CXX flags (see Windows notes above) |
| Generated headers not found | Include path not propagated | Ensure `target_link_libraries` links to the proto library target |
