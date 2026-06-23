/*

This file is part of HCE-LABORATORY.

  Copyright (C) 2025 Jose Vicente Campos Martinez, <josevcm@gmail.com>

  HCE-LABORATORY is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  HCE-LABORATORY is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with HCE-LABORATORY. If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef HCE_DESFIRE_INSTANCE_H
#define HCE_DESFIRE_INSTANCE_H

#include <map>

#include <hce/crc/CRC.h>
#include <hce/crypto/CMAC.h>
#include <hce/crypto/CipherDES.h>
#include <hce/crypto/CipherAES.h>

#include "KeyEntry.h"
#include "FileEntry.h"

#define DESFIRE_MASTER_APP_ID                         0x000000
#define DESFIRE_MASTER_KEY_ID                         0x00
#define DESFIRE_MASTER_KEY_TYPE                       0x00
#define DESFIRE_MASTER_KEY_VERSION                    0x00
#define DESFIRE_MASTER_KEY_SETTINGS                   0x0F
#define DESFIRE_MASTER_KEY_DEFAULT_2K3DES             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define DESFIRE_MASTER_KEY_DEFAULT_3K3DES             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define DESFIRE_MASTER_KEY_DEFAULT_AES                {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

// --- ISO settings ---
#define DESFIRE_ISO_MASTERFILE_ID                     0x3F00
#define DESFIRE_ISO_MASTERFILE_NAME                   {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00}

// --- Master key settings masks ---
#define DESFIRE_KEY_SETTINGS_ALOW_CHANGE_MASTER_KEY   0x01
#define DESFIRE_KEY_SETTINGS_FREE_DIRECTORY_LISTING   0x02
#define DESFIRE_KEY_SETTINGS_FREE_CREATE_DELETE       0x04
#define DESFIRE_KEY_SETTINGS_ALOW_CHANGE_CONFIG       0x08

// --- Application change key settings ---
#define DESFIRE_KEY_SETTINGS_CHANGE_KEY_MASTER        0x00
#define DESFIRE_KEY_SETTINGS_CHANGE_KEY_SAME          0x0E
#define DESFIRE_KEY_SETTINGS_CHANGE_KEY_FREEZE        0x0F

// --- Application crypto mode
#define DESFIRE_CRYPTO_MODE_LEGACY                    0x00
#define DESFIRE_CRYPTO_MODE_ISO                       0x01
#define DESFIRE_CRYPTO_MODE_AES                       0x02

// --- Limits---
#define DESFIRE_MAX_FILE_ID        31
#define DESFIRE_MAX_FRAME_SIZE     59

// --- Status values ---
#define DESFIRE_STATUS_OK                        0x00
#define DESFIRE_STATUS_NO_CHANGES                0x0C
#define DESFIRE_STATUS_OUT_OF_EEPROM             0x0E
#define DESFIRE_STATUS_ILLEGAL_COMMAND           0x1C
#define DESFIRE_STATUS_INTEGRITY_ERROR           0x1E
#define DESFIRE_STATUS_NO_SUCH_KEY               0x40
#define DESFIRE_STATUS_LENGTH_ERROR              0x7E
#define DESFIRE_STATUS_PERMISSION_DENIED         0x9D
#define DESFIRE_STATUS_PARAMETER_ERROR           0x9E
#define DESFIRE_STATUS_APPLICATION_NOT_FOUND     0xA0
#define DESFIRE_STATUS_APPL_INTEGRITY_ERROR      0xA1
#define DESFIRE_STATUS_AUTHENTICATION_ERROR      0xAE
#define DESFIRE_STATUS_ADDITIONAL_FRAME          0xAF
#define DESFIRE_STATUS_BOUNDARY_ERROR            0xBE
#define DESFIRE_STATUS_PICC_INTEGRITY_ERROR      0xC1
#define DESFIRE_STATUS_COMMAND_ABORTED           0xCA
#define DESFIRE_STATUS_PICC_DISABLED             0xCD
#define DESFIRE_STATUS_COUNT_ERROR               0xCE
#define DESFIRE_STATUS_DUPLICATE_ERROR           0xDE
#define DESFIRE_STATUS_EEPROM_ERROR              0xEE
#define DESFIRE_STATUS_FILE_NOT_FOUND            0xF0
#define DESFIRE_STATUS_FILE_INTEGRITY_ERROR      0xF1

// --- ISO Status values ---
#define DESFIRE_ISO_STATUS_NOT_ENOUGH_DATA       0x6985
#define DESFIRE_ISO_STATUS_WRONG_LENGTH          0x6700
#define DESFIRE_ISO_STATUS_ACCESS_NOT_ALLOWED    0x6982
#define DESFIRE_ISO_STATUS_FILE_NOT_FOUND        0x6A82
#define DESFIRE_ISO_STATUS_WRONG_PARAMETERS_P1P2 0x6A86
#define DESFIRE_ISO_STATUS_WRONG_PARAMETERS_LC   0x6A87
#define DESFIRE_ISO_STATUS_WRONG_PARAMETERS_LE   0x6C00
#define DESFIRE_ISO_STATUS_INS_NOT_SUPPORTED     0x6D00
#define DESFIRE_ISO_STATUS_CLA_NOT_SUPPORTED     0x6E00
#define DESFIRE_ISO_STATUS_NO_DIAGNOSTIC         0x6F00
#define DESFIRE_ISO_STATUS_OK                    0x9000

namespace hce::targets::desfire {

using namespace crc;
using namespace crypto;

enum AuthMode
{
   LegacyAuthentication = 0x00,
   ISOAuthentication = 0x01,
   AESAuthentication = 0x02
};

struct Application
{
   unsigned int aid = 0;
   unsigned int cryptoMode = 0;
   unsigned int keySettings = 0x0F;
   unsigned int maximumKeys = 0;
   bool isoEnabled = false;
   unsigned int isoId = 0;
   rt::ByteBuffer isoName = {};
   std::map<unsigned int, KeyEntry> keys;
   std::map<unsigned int, FileEntry> files;
};

struct Authentication
{
   unsigned int mode = -1;
   KeyEntry *keyEntry = nullptr;
   Cipher *cipher = nullptr;
   rt::ByteBuffer sessionIv;
   rt::ByteBuffer sessionKey;

   bool isMasterKey() const
   {
      return keyEntry != nullptr && keyEntry->id == DESFIRE_MASTER_KEY_ID;
   }
};

struct ElementaryFile
{
   unsigned int isoId;
   unsigned int fileId;
};

struct DirectoryFile
{
   unsigned int isoId;
   unsigned int appId;
   rt::ByteBuffer name;
   std::map<unsigned int, ElementaryFile> ef;
};

struct Instance
{
   rt::Logger *log = rt::Logger::getLogger("hce.targets.desfire.Instance");

   // UID
   rt::ByteBuffer uid;

   // Hardware info
   unsigned int hardwareVendor = 0x04;
   unsigned int hardwareType = 0x01;
   unsigned int hardwareSubtype = 0x01;
   unsigned int hardwareVersion = 0x0100;
   unsigned int hardwareStorage = 0x18;
   unsigned int hardwareProtocol = 0x05;

   // Hardware info
   unsigned int softwareVendor = 0x04;
   unsigned int softwareType = 0x01;
   unsigned int softwareSubtype = 0x01;
   unsigned int softwareVersion = 0x0104;
   unsigned int softwareStorage = 0x18;
   unsigned int softwareProtocol = 0x05;

   // batch and production
   unsigned int productionYear = 0x16;
   unsigned int productionWeek = 0x08;
   unsigned long long batchNumber = 0xA0B31874BA;

   // current selected application
   Application *application = nullptr;

   // current authenticated key
   Authentication *auth = nullptr;

   // current selected directory and elementary file
   DirectoryFile *directoryFile = nullptr;
   ElementaryFile *elementaryFile = nullptr;

   // available ciphers
   CipherDES des;
   CipherAES aes;

   // PICC configuration flags (write-once)
   bool formatDisabled = false;
   bool randomId = false;

   // default key applied to new applications (option 0x01), empty = use zero key
   rt::ByteBuffer defaultKey = {};
   unsigned int defaultKeyVersion = 0;

   // user-defined ATS (option 0x02), empty = use hardware default
   rt::ByteBuffer customAts = {};

   // current command and chaining status
   int protocol = 0;
   int command = 0;
   int chaining = 0;
   bool dirty = false;

   // current command header a tx/rx buffer
   rt::ByteBuffer header;
   rt::ByteBuffer buffer;

   // list of applications
   std::map<unsigned int, Application> applications;

   // list of ISO directory files
   std::map<unsigned int, DirectoryFile> directoryFiles;

   Instance();

   /*
    * initialize PICC application
    */
   void initialize();

   /*
    * get equivalent used memory
    */
   unsigned int usedMemory();

   /*
    * get equivalent free memory
    */
   unsigned int freeMemory();

   /*
    * ISO add directory file
    */
   void isoAddDirectoryFile(const DirectoryFile &df);

   /*
    * ISO add elementary file
    */
   void isoAddElementaryFile(const ElementaryFile &ef);

   /*
    * ISO select DF
    */
   void isoSelectDirectoryFile(const rt::ByteBuffer &name);

   /*
    * ISO select EF
   */
   void isoSelectElementaryFile(unsigned int id);

   /*
    * ISO select DF or EF
    */
   void isoSelectFile(unsigned int id);

   /*
    * ISO check if DirectoryFile exists by name
    */
   bool isoHasDirectoryFile(const rt::ByteBuffer &name) const;

   /*
    * ISO check if ElementaryFile exists under current DirectoryFile
    */
   bool isoHasElementaryFile(unsigned int id) const;

   /*
    * ISO check if DF or EF exists by FID
    */
   bool isoHasFile(unsigned int id) const;

   /*
    * list user applications (exclude master APPS)
    */
   std::vector<DirectoryFile> isoListDirectoryFiles() const;

   /*
    * add application
    */
   void addApplication(const Application &app);

   /*
    * delete application by AID
    */
   void deleteApplication(unsigned int aid);

   /*
    * check if application AID exits
    */
   bool hasApplication(unsigned int aid) const;

   /*
    * get application reference by AID
    */
   Application *getApplication(unsigned int aid);

   /*
    * list user applications (exclude master APPS)
    */
   std::vector<unsigned int> listApplications() const;

   /*
    * select application by AIS
    */
   void selectApplication(unsigned int aid);

   /*
    * check if AID is selected
    */
   bool isApplicationSelected(unsigned int aid) const;

   /*
    * check if any application different from master APP is selected.
    */
   bool isApplicationSelected() const;

   /*
    * check if current selected application has ISO enabled.
    */
   bool isApplicationIsoEnabled() const;

   /*
    * check current app crypto mode
    */
   bool isApplicationCryptoMode(int mask) const;

   /*
    * clear all user applications (exclude master App)
    */
   void clearApplications();

   /*
    * check if file exists
    */
   bool hasFile(unsigned int fileId) const;

   /*
    * add file to current selected application
    */
   void addFile(const FileEntry &file);

   /*
    * delete file from current selected application
    */
   void deleteFile(unsigned int fileId);

   /*
    * list al files inside curren application
    */
   std::vector<unsigned int> listFiles() const;

   /*
    * get file reference by its ID
    */
   FileEntry *getFile(unsigned int fileId) const;

   /*
    * get file reference by its Short File ID (SFI = isoId & 0x1F)
    */
   FileEntry *getFileByShortFID(unsigned int sfi) const;

   /*
    * check effective communication mode for read operations
    */
   unsigned int authorizeForRead(unsigned int commSettings, const std::initializer_list<unsigned int> &keys) const;

   /*
    * check effective communication mode for write operations
    */
   unsigned int authorizeForWrite(unsigned int commSettings, const std::initializer_list<unsigned int> &keys) const;

   /*
    * update master key settings of current selected application
    */
   void setKeySettings(unsigned int keySettings);

   /*
    * check if current master key settings allow free directory listing
    */
   bool isFreeDirectoryListing() const;

   /*
    * check if current master key settings allow free create / delete
    */
   bool isFreeCreateDelete() const;

   /*
    * check current master key settings allow change configuration
    */
   bool isAllowChangeConfig() const;

   /*
    * check if current master key settings allow change master key
    */
   bool isMasterKeyChangeable() const;

   /*
    * check if current authentication status allow to change given key
    */
   bool isKeyChangeable(unsigned int keyId) const;

   /*
    * check if selected application has key
    */
   bool hasKeyEntry(unsigned int keyNo) const;

   /*
    * get key entry within current selected application
    */
   KeyEntry *getKeyEntry(unsigned int keyNo) const;

   /*
    * check if it has valid authentication state (any key and any mode)
    */
   bool isAuthenticated() const;

   /*
    * check if it has valid Legacy authentication state (any key)
    */
   bool isAuthenticatedLegacy() const;

   /*
    * check if it has valid ISO authentication state (any key)
    */
   bool isAuthenticatedISO() const;

   /*
    * check if it has valid AES authentication state (any key)
    */
   bool isAuthenticatedAES() const;

   /*
    * check if it has valid authentication state with specified key
    */
   bool isAuthenticatedWithKey(unsigned int key) const;

   /*
    * check if it has valid authentication state with specified key
    */
   bool isAuthenticatedAnyKeys(const std::initializer_list<unsigned int> &keys) const;

   /*
    * check if it has valid authentication with master key (id = 0)
    */
   bool isAuthenticatedWithMasterKey() const;

   /*
    * check if it has valid authentication to read specified file
    */
   bool isAuthenticatedForRead(const FileEntry *file) const;

   /*
    * check if it has valid authentication to write specified file
    */
   bool isAuthenticatedForWrite(const FileEntry *file) const;

   /*
    * check if it has valid authentication to read specified file
    */
   bool isAuthenticatedForReadWrite(const FileEntry *file) const;

   /*
    * invalidate instance authentication state
    */
   void setAuthentication(Authentication &authentication);

   /*
    * invalidate instance authentication state
    */
   void invalidateAuth();

   /*
    * check if file has free access for all keys (read / write & readWrite)
    */
   bool hasFreeFullAccess(const FileEntry *file) const;

   /*
    * check if file has free read access
    */
   bool hasFreeReadAccess(const FileEntry *file) const;

   /*
    * check if file has free read access
    */
   bool hasFreeWriteAccess(const FileEntry *file) const;

   /*
    * check if file has free read access
    */
   bool hasFreeReadWriteAccess(const FileEntry *file) const;

   /*
    * check current authenticated key has read access to file
    */
   bool hasReadAccess(const FileEntry *file) const;

   /*
    * check current authenticated key has write access to file
    */
   bool hasWriteAccess(const FileEntry *file) const;

   /*
    * check current authenticated key has read/write access to file
    */
   bool hasReadWriteAccess(const FileEntry *file) const;

   /*
    * check current authenticated key can change file settings
    */
   bool hasChangeAccess(const FileEntry *file) const;

   /*
    * check if key has access with current authentication state
    */
   bool hasAccess(unsigned int key) const;

   /*
    * check if some of the keys is free access
    */
   bool isFreeAccessAnyKeys(const std::initializer_list<unsigned int> &keys) const;

   /*
    * check if key has free access
    */
   bool isFreeAccessKey(unsigned int key) const;

   /*
    * check if some of the keys is free access
    */
   bool isNeverAccessAllKeys(const std::initializer_list<unsigned int> &keys) const;

   /*
    * check if key is never access
    */
   bool isNeverAccessKey(unsigned int key) const;

   /*
    * commit transaction
    */
   bool commitData();

   /*
    * rollback transaction
    */
   bool rollbackData();

   /*
    * detect communication mode and receive data from reader
    */
   int decodeData(const rt::ByteBuffer &data, unsigned int length, unsigned int commSettings);

   /*
    * detect communication mode and receive data from reader with keys check
    */
   int decodeData(const rt::ByteBuffer &data, unsigned int length, unsigned int commSettings, const std::initializer_list<unsigned int> &keys);

   /*
    * receive data from reader in PLAIN mode
    */
   int decodeDataPlain(const rt::ByteBuffer &data, unsigned int length);

   /*
    * receive data from reader in MACING mode
    */
   int decodeDataMacing(const rt::ByteBuffer &data, unsigned int length);

   /*
    * receive data from reader in CRYPTO mode
    */
   int decodeDataCrypto(const rt::ByteBuffer &data, unsigned int length);

   /*
    * prepare data to send,
    */
   int encodeData(rt::ByteBuffer &data, unsigned int length, unsigned int commSettings);

   /*
    * prepare data to send with keys check
    */
   int encodeData(rt::ByteBuffer &data, unsigned int length, unsigned int commSettings, const std::initializer_list<unsigned int> &keys);

   /*
    * prepare data to send in PLAIN mode
    */
   int encodeDataPlain(const rt::ByteBuffer &data, unsigned int length);

   /*
    * prepare data to send in MACING mode
    */
   int encodeDataMacing(const rt::ByteBuffer &data, unsigned int length);

   /*
    * prepare data to send in CRYPTO mode
    */
   int encodeDataCrypto(const rt::ByteBuffer &data, unsigned int length);

   /*
    * peek next block to send from buffer
    */
   int sendData(rt::ByteBuffer &response, unsigned int size = DESFIRE_MAX_FRAME_SIZE);

   /*
    * send successful ACK and append CMAC if required
    */
   int sendAck(rt::ByteBuffer &response);

   /*
    * encrypt data using legacy 3DES
    */
   void encryptDataEV0(const rt::ByteBuffer &input, rt::ByteBuffer &output, unsigned int length) const;

   /*
    * decrypt data using legacy 3DES
    */
   int decryptDataEV0(const rt::ByteBuffer &input, rt::ByteBuffer &output, unsigned int length) const;

   /*
    * encrypt data using standard 3DES/AES
    */
   void encryptDataEV1(const rt::ByteBuffer &input, rt::ByteBuffer &output, unsigned int length) const;

   /*
    * decrypt data using standard 3DES/AES
    */
   int decryptDataEV1(const rt::ByteBuffer &params, const rt::ByteBuffer &input, rt::ByteBuffer &output, unsigned int length) const;

   /*
    * macing data using standard 3DES/AES
    */
   rt::ByteBuffer requestCmac(const rt::ByteBuffer &header, const rt::ByteBuffer &data);

   /*
    * macing data using standard 3DES/AES
    */
   rt::ByteBuffer requestCmac(const rt::ByteBuffer &data = rt::ByteBuffer::empty());

   /*
    * macing data using standard 3DES/AES
    */
   rt::ByteBuffer responseCmac(const rt::ByteBuffer &data = rt::ByteBuffer::empty());

   /*
    * macing data using standard 3DES/AES
    */
   rt::ByteBuffer cmac(const rt::ByteBuffer &data) const;

   /*
    *
    */
   void readHeader(const rt::ByteBuffer &request, unsigned int length);

   /*
    *
    */
   void updateIv(const rt::ByteBuffer &request, unsigned int length);
};

}
#endif //HCE_DESFIRE_INSTANCE_H
