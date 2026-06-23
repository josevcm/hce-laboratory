set(CMAKE_CXX_STANDARD 17)

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/include)

add_library(hce-targets STATIC

        # Generic T4T implementation  ---
        src/main/cpp/t4t/T4T.cpp

        # Desfire implementation  ---
        src/main/cpp/desfire/Command.cpp
        src/main/cpp/desfire/Desfire.cpp
        src/main/cpp/desfire/Instance.cpp
        src/main/cpp/desfire/FileEntry.cpp

        # Security Related Commands ---
        src/main/cpp/desfire/DesfireAuthenticate.cpp
        src/main/cpp/desfire/DesfireSetConfiguration.cpp
        src/main/cpp/desfire/DesfireAuthenticateAES.cpp
        src/main/cpp/desfire/DesfireAuthenticateISO.cpp
        src/main/cpp/desfire/DesfireChangeKey.cpp
        src/main/cpp/desfire/DesfireChangeKeySettings.cpp
        src/main/cpp/desfire/DesfireGetKeySettings.cpp
        src/main/cpp/desfire/DesfireGetKeyVersion.cpp

        # PICC Level Commands ---
        src/main/cpp/desfire/DesfireGetCardUID.cpp
        src/main/cpp/desfire/DesfireCreateApplication.cpp
        src/main/cpp/desfire/DesfireDeleteApplication.cpp
        src/main/cpp/desfire/DesfireSelectApplication.cpp
        src/main/cpp/desfire/DesfireFormatPICC.cpp
        src/main/cpp/desfire/DesfireListApplications.cpp
        src/main/cpp/desfire/DesfireListDFNames.cpp
        src/main/cpp/desfire/DesfireGetFreeMemory.cpp
        src/main/cpp/desfire/DesfireGetVersion.cpp

        # Application Level Commands ---
        src/main/cpp/desfire/DesfireCreateDataFile.cpp
        src/main/cpp/desfire/DesfireCreateRecordFile.cpp
        src/main/cpp/desfire/DesfireCreateValueFile.cpp
        src/main/cpp/desfire/DesfireListFiles.cpp
        src/main/cpp/desfire/DesfireGetIsoFileIDs.cpp
        src/main/cpp/desfire/DesfireDeleteFile.cpp
        src/main/cpp/desfire/DesfireChangeFileSettings.cpp
        src/main/cpp/desfire/DesfireGetFileSettings.cpp

        # Data Manipulation Commands ---
        src/main/cpp/desfire/DesfireReadData.cpp
        src/main/cpp/desfire/DesfireWriteData.cpp
        src/main/cpp/desfire/DesfireWriteRecord.cpp
        src/main/cpp/desfire/DesfireReadRecords.cpp
        src/main/cpp/desfire/DesfireClearRecordFile.cpp
        src/main/cpp/desfire/DesfireCredit.cpp
        src/main/cpp/desfire/DesfireDebit.cpp
        src/main/cpp/desfire/DesfireLimitedCredit.cpp
        src/main/cpp/desfire/DesfireGetValue.cpp
        src/main/cpp/desfire/DesfireCommitTransaction.cpp
        src/main/cpp/desfire/DesfireAbortTransaction.cpp

        # ISO/IEC 7816-4 Commands
        src/main/cpp/desfire/DesfireIsoSelectFile.cpp
        src/main/cpp/desfire/DesfireIsoReadBinary.cpp
        src/main/cpp/desfire/DesfireIsoUpdateBinary.cpp
        src/main/cpp/desfire/DesfireIsoReadRecords.cpp
        src/main/cpp/desfire/DesfireIsoUpdateRecord.cpp
        src/main/cpp/desfire/DesfireIsoAppendRecord.cpp
        src/main/cpp/desfire/DesfireIsoAuthentication.cpp

        # MifarePlus implementation  ---
        src/main/cpp/mifareplus/Command.cpp
        src/main/cpp/mifareplus/Instance.cpp
        src/main/cpp/mifareplus/MifarePlus.cpp
        src/main/cpp/mifareplus/MifarePlusResetAuth.cpp
        src/main/cpp/mifareplus/MifarePlusFirstAuth.cpp
        src/main/cpp/mifareplus/MifarePlusFollowingAuth.cpp
        src/main/cpp/mifareplus/MifarePlusRead.cpp
        src/main/cpp/mifareplus/MifarePlusReadEncrypted.cpp
        src/main/cpp/mifareplus/MifarePlusWrite.cpp
        src/main/cpp/mifareplus/MifarePlusWriteEncrypted.cpp
        src/main/cpp/mifareplus/MifarePlusIncrement.cpp
        src/main/cpp/mifareplus/MifarePlusDecrement.cpp
        src/main/cpp/mifareplus/MifarePlusRestore.cpp
        src/main/cpp/mifareplus/MifarePlusTransfer.cpp
        src/main/cpp/mifareplus/MifarePlusIncrementTransfer.cpp
        src/main/cpp/mifareplus/MifarePlusDecrementTransfer.cpp
        src/main/cpp/mifareplus/MifarePlusRestoreTransfer.cpp
        src/main/cpp/mifareplus/MifarePlusGetUID.cpp
)

target_include_directories(hce-targets PUBLIC ${PUBLIC_INCLUDE_DIR})
target_include_directories(hce-targets PRIVATE ${PRIVATE_SOURCE_DIR})

target_link_libraries(hce-targets hce-core rt-lang nlohmann)