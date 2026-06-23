set(CMAKE_CXX_STANDARD 17)

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)

set(DESFIRE_TEST_SOURCES
        src/main/cpp/tests/desfire/TestAESEncipheredMode.cpp
        src/main/cpp/tests/desfire/TestAbortTransaction.cpp
        src/main/cpp/tests/desfire/TestAccessControl.cpp
        src/main/cpp/tests/desfire/TestAuthenticate.cpp
        src/main/cpp/tests/desfire/TestAuthenticateAES.cpp
        src/main/cpp/tests/desfire/TestAuthenticateISO.cpp
        src/main/cpp/tests/desfire/TestAuthenticationErrors.cpp
        src/main/cpp/tests/desfire/TestBackupFile.cpp
        src/main/cpp/tests/desfire/TestChangeFileSettings.cpp
        src/main/cpp/tests/desfire/TestChangeKey.cpp
        src/main/cpp/tests/desfire/TestChangeKeySettings.cpp
        src/main/cpp/tests/desfire/TestClearRecordBehavior.cpp
        src/main/cpp/tests/desfire/TestCmacPlain.cpp
        src/main/cpp/tests/desfire/TestCommitTransaction.cpp
        src/main/cpp/tests/desfire/TestCreateApplication.cpp
        src/main/cpp/tests/desfire/TestCreditBoundary.cpp
        src/main/cpp/tests/desfire/TestCyclicRecordFile.cpp
        src/main/cpp/tests/desfire/TestCyclicRecordWrap.cpp
        src/main/cpp/tests/desfire/TestDeleteApplicationAutoSelect.cpp
        src/main/cpp/tests/desfire/TestDeleteFile.cpp
        src/main/cpp/tests/desfire/TestDuplicateErrors.cpp
        src/main/cpp/tests/desfire/TestEncipheredMode.cpp
        src/main/cpp/tests/desfire/TestFormatCard.cpp
        src/main/cpp/tests/desfire/TestGetCardUID.cpp
        src/main/cpp/tests/desfire/TestGetFreeMemory.cpp
        src/main/cpp/tests/desfire/TestGetIsoFileIDs.cpp
        src/main/cpp/tests/desfire/TestGetKeySettings.cpp
        src/main/cpp/tests/desfire/TestGetKeyVersion.cpp
        src/main/cpp/tests/desfire/TestGetVersionInfo.cpp
        src/main/cpp/tests/desfire/TestIsoAbsoluteOffset.cpp
        src/main/cpp/tests/desfire/TestIsoReadWrite.cpp
        src/main/cpp/tests/desfire/TestIsoSFI.cpp
        src/main/cpp/tests/desfire/TestIsoSelect.cpp
        src/main/cpp/tests/desfire/TestIsoSelectById.cpp
        src/main/cpp/tests/desfire/TestLimitedCredit.cpp
        src/main/cpp/tests/desfire/TestLinearRecordFile.cpp
        src/main/cpp/tests/desfire/TestListApplications.cpp
        src/main/cpp/tests/desfire/TestListDFNames.cpp
        src/main/cpp/tests/desfire/TestMacingMode.cpp
        src/main/cpp/tests/desfire/TestMultiKeyApplication.cpp
        src/main/cpp/tests/desfire/TestNoChangesTransaction.cpp
        src/main/cpp/tests/desfire/TestSelectApplication.cpp
        src/main/cpp/tests/desfire/TestSessionKey2K3DES.cpp
        src/main/cpp/tests/desfire/TestStandardFile.cpp
        src/main/cpp/tests/desfire/TestTransactionChaining.cpp
        src/main/cpp/tests/desfire/TestUnknownCommand.cpp
        src/main/cpp/tests/desfire/TestValueFile.cpp
        src/main/cpp/tests/desfire/TestValueFileLimits.cpp
        src/main/cpp/tests/desfire/TestValueFileValidation.cpp
)

set(MIFAREPLUS_TEST_SOURCES
        src/main/cpp/tests/mifareplus/TestMFPAuthErrors.cpp
        src/main/cpp/tests/mifareplus/TestMFPAuthenticate.cpp
        src/main/cpp/tests/mifareplus/TestMFPGetUID.cpp
        src/main/cpp/tests/mifareplus/TestMFPMultiSector.cpp
        src/main/cpp/tests/mifareplus/TestMFPRead.cpp
        src/main/cpp/tests/mifareplus/TestMFPReadEncrypted.cpp
        src/main/cpp/tests/mifareplus/TestMFPValueBlock.cpp
        src/main/cpp/tests/mifareplus/TestMFPWrite.cpp
        src/main/cpp/tests/mifareplus/TestMFPWriteEncrypted.cpp
)

set(TEST_SOURCES ${DESFIRE_TEST_SOURCES} ${MIFAREPLUS_TEST_SOURCES})

# Default mode (no args): loopback against the in-process DESFire emulation — no hardware needed.
# With -pcsc: sends APDUs to a real DESFire card via a PCSC reader.
add_executable(hce-ut src/main/cpp/main.cpp ${TEST_SOURCES})

target_include_directories(hce-ut PRIVATE ${PRIVATE_SOURCE_DIR})

if (WIN32)
    set(PLATFORM_LIBS mingw32 psapi)
endif (WIN32)

target_link_libraries(hce-ut hce-cards hce-targets hw-dev rt-lang ${PLATFORM_LIBS})

add_test(NAME mifareplus-emulated COMMAND hce-ut --test-mfp)

enable_testing()
add_test(NAME desfire-emulated COMMAND hce-ut)
