add_subdirectory(cppdes)
#add_subdirectory(ftdi1)
add_subdirectory(microaes)
add_subdirectory(microtar)
add_subdirectory(nlohmann)

if (HCE_MPSSE_BACKEND STREQUAL "d2xx")
  add_subdirectory(kftdi)
endif()


