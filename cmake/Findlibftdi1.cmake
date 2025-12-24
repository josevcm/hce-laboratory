
if (LIBFTDI_LIBRARY AND LIBFTDI_INCLUDE)
    set(LIBUSB_FOUND TRUE)
else (LIBFTDI_LIBRARY AND LIBFTDI_INCLUDE)

    find_path(LIBFTDI_INCLUDE
            NAMES
                ftdi.h
            PATHS
                /usr/include
                /usr/local/include
                /opt/local/include
            PATH_SUFFIXES
                libftdi1
    )

    find_library(LIBFTDI_LIBRARY
            NAMES
                ftdi1
            PATHS
                /usr/local/lib64
                /opt/local/lib64
                /usr/lib64
                /usr/local/lib
                /opt/local/lib
                /usr/lib
    )

    if (LIBFTDI_INCLUDE AND LIBFTDI_LIBRARY)
        set(LIBFTDI_FOUND TRUE)
    else ()
        set(LIBFTDI_LIBRARY "ftdi1")
    endif ()

endif (LIBFTDI_LIBRARY AND LIBFTDI_INCLUDE)