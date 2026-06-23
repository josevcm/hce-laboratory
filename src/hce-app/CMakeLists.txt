option(HCE_BUILD_APP_QT "Build the Qt6 GUI application (hce-lab)" ON)
option(HCE_BUILD_APP_T4T "Build the Type 4 target emulator (hce-t4t)" ON)
option(HCE_BUILD_APP_UT "Build the Unitary Tests (hce-ut)" ON)

if (HCE_BUILD_APP_QT)
    add_subdirectory(app-qt)
endif ()

if (HCE_BUILD_APP_T4T)
    add_subdirectory(app-t4t)
endif ()

if (HCE_BUILD_APP_UT)
    add_subdirectory(app-ut)
endif ()

