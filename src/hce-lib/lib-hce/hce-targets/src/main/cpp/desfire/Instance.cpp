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

#include "Instance.h"

namespace hce::targets::desfire {

using namespace crc;
using namespace crypto;

Instance::Instance() : header(256), buffer(8192)
{
   initialize();
}

/*
 * initialize PICC application
 */
void Instance::initialize()
{
   // create master APP
   const Application app {
      .aid = DESFIRE_MASTER_APP_ID,
      .cryptoMode = 0,
      .keySettings = DESFIRE_MASTER_KEY_SETTINGS,
      .maximumKeys = 1,
      .isoEnabled = true,
      .isoId = DESFIRE_ISO_MASTERFILE_ID,
      .isoName = DESFIRE_ISO_MASTERFILE_NAME,
      .keys = {
         {
            DESFIRE_MASTER_KEY_ID, KeyEntry {
               .id = DESFIRE_MASTER_KEY_ID,
               .type = DESFIRE_MASTER_KEY_TYPE,
               .version = DESFIRE_MASTER_KEY_VERSION,
               .key = DESFIRE_MASTER_KEY_DEFAULT_2K3DES
            }
         }
      },
      .files = {}
   };

   // add PICC MasterApp
   addApplication(app);

   // by default PICC selects MasterApp
   selectApplication(app.aid);
}

/*
 * get equivalent used memory
 */
unsigned int Instance::usedMemory()
{
   // start with 3 NV-block for master APP
   unsigned int blocksUsed = 3;

   // if only master APP is present, finish...
   if (applications.size() == 1)
      return blocksUsed * 32;

   // each 4 applications require 1 extra NV-block
   blocksUsed += std::ceil(static_cast<float>(applications.size() - 1) / 4.0f);

   // each 2 keys use 1 NV-block
   for (auto &[aid, app]: applications)
   {
      // skip master APP
      if (aid == 0)
         continue;

      // app consume 1 NV-block
      blocksUsed += 1;

      // keys consumes 1 NV-block for each 2 keys
      blocksUsed += 1 + static_cast<int>(std::ceil(static_cast<float>(app.keys.size()) / 2.0f));

      // files consume 1 NV-block for each 2 files
      blocksUsed += static_cast<int>(std::ceil(static_cast<float>(app.files.size()) / 2.0f));

      // process files
      for (auto &[fileId, file]: app.files)
      {
         int fileBytes = file.usedMemory();

         LOG_INFO(log, "\tfile[{}]: {} bytes", {fileId, fileBytes});

         blocksUsed += static_cast<int>(std::ceil(static_cast<float>(fileBytes) / 32.0f));
      }
   }

   LOG_INFO(log, "\ttotal blocks: {}", {blocksUsed});

   return blocksUsed * 32;
}

/*
 * get equivalent free memory
 */
unsigned int Instance::freeMemory()
{
   const unsigned int memorySize = 2 << (hardwareStorage >> 1);

   return memorySize - usedMemory();
}

/*
 * ISO add directory file
 */
void Instance::isoAddDirectoryFile(const DirectoryFile &df)
{
   directoryFiles.emplace(df.isoId, df);
}

/*
 * ISO add elementary file
 */
void Instance::isoAddElementaryFile(const ElementaryFile &ef)
{
   assert(directoryFile != nullptr);
   directoryFile->ef.emplace(ef.isoId, ef);
}

/*
 * ISO select directory file by name
 */
void Instance::isoSelectDirectoryFile(const rt::ByteBuffer &name)
{
   for (auto &[id,df]: directoryFiles)
   {
      if (df.name == name)
      {
         directoryFile = &df;
         selectApplication(directoryFile->appId);
      }
   }
}

/*
 * ISO select elementary file by id
 */
void Instance::isoSelectElementaryFile(unsigned int id)
{
   assert(directoryFile != nullptr);
   if (const auto it = directoryFile->ef.find(id); it != directoryFile->ef.end())
      elementaryFile = &it->second;
}

/*
 * ISO select DF or EF by ID
 */
void Instance::isoSelectFile(const unsigned int id)
{
   const DirectoryFile *oldDf = directoryFile;

   for (auto &[dfId, df]: directoryFiles)
   {
      if (dfId == id)
      {
         directoryFile = &df;
         elementaryFile = nullptr;
      }
      else if (auto ef = df.ef.find(id); ef != df.ef.end())
      {
         directoryFile = &df;
         elementaryFile = &ef->second;
      }
   }

   // update Desfire underline application
   if (directoryFile != oldDf)
      selectApplication(directoryFile->appId);
}

/*
 * ISO select directory file by name
 */
bool Instance::isoHasDirectoryFile(const rt::ByteBuffer &name) const
{
   return std::any_of(directoryFiles.cbegin(), directoryFiles.cend(), [&](auto &entry) {
      return entry.second.name == name;
   });
}

/*
 * ISO select directory file by name
 */
bool Instance::isoHasElementaryFile(const unsigned int id) const
{
   assert(directoryFile != nullptr);
   return directoryFile->ef.find(id) != directoryFile->ef.end();
}

/*
 * ISO select directory file by name
 */
bool Instance::isoHasFile(const unsigned int id) const
{
   if (directoryFiles.find(id) != directoryFiles.end())
      return true;

   return std::any_of(directoryFiles.cbegin(), directoryFiles.cend(), [&](auto &entry) {
      return entry.second.ef.find(id) != entry.second.ef.end();
   });
}

/*
 * list user applications (exclude master APPS)
 */
std::vector<DirectoryFile> Instance::isoListDirectoryFiles() const
{
   std::vector<DirectoryFile> dfs;

   for (auto &[id, df]: directoryFiles)
   {
      if (id != DESFIRE_ISO_MASTERFILE_ID)
         dfs.push_back(df);
   }

   return dfs;
}

/*
 * add application
 */
void Instance::addApplication(const Application &app)
{
   applications[app.aid] = app;

   // add ISO DirectoryFile
   if (app.isoEnabled)
   {
      const DirectoryFile mf {
         .isoId = app.isoId,
         .appId = app.aid,
         .name = app.isoName
      };

      // add ISO MasterFile
      isoAddDirectoryFile(mf);
   }

   // set picc as dirty state
   dirty = true;
}

/*
 * delete application by AID
 */
void Instance::deleteApplication(const unsigned int aid)
{
   if (const auto it = applications.find(aid); it != applications.end())
   {
      // remove ISO en
      if (it->second.isoEnabled)
         directoryFiles.erase(it->second.isoId);

      // remove application
      applications.erase(it);

      // set picc as dirty state
      dirty = true;
   }
}

/*
 * check if application AID exits
 */
bool Instance::hasApplication(unsigned int aid) const
{
   return applications.find(aid) != applications.end();
}

/*
 * get application reference by AID
 */
Application *Instance::getApplication(unsigned int aid)
{
   return &applications.find(aid)->second;
}

/*
 * list user applications (exclude master APPS)
 */
std::vector<unsigned int> Instance::listApplications() const
{
   std::vector<unsigned int> apps;

   for (auto &[id, app]: applications)
      apps.push_back(id);

   return apps;
}

/*
 * select application by AIS
 */
void Instance::selectApplication(const unsigned int aid)
{
   // skip re-selection of same application
   if (application != nullptr && application->aid == aid)
      return;

   // find new AID
   if (const auto it = applications.find(aid); it != applications.end())
   {
      // update selected application
      application = &it->second;

      // update ISO application
      if (application->isoEnabled)
         isoSelectFile(application->isoId);
   }
}

/*
 * check if AID is selected
 */
bool Instance::isApplicationSelected(const unsigned int aid) const
{
   return application != nullptr && application->aid == aid;
}

/*
 * check if any application different from master APP is selected.
 */
bool Instance::isApplicationSelected() const
{
   return application != nullptr && application->aid != DESFIRE_MASTER_APP_ID;
}

/*
 * check if current selected application has ISO enabled.
 */
bool Instance::isApplicationIsoEnabled() const
{
   return application != nullptr && application->isoEnabled;
}

/*
 * check current app crypto mode
 */
bool Instance::isApplicationCryptoMode(int mode) const
{
   return application != nullptr && application->cryptoMode == mode;
}

/*
 * clear all user applications (exclude master App)
 */
void Instance::clearApplications()
{
   // delete all except the master one
   for (auto it = applications.begin(); it != applications.end();)
   {
      if (it->first != DESFIRE_MASTER_APP_ID)
         it = applications.erase(it);
      else
         ++it;
   }

   for (auto it = directoryFiles.begin(); it != directoryFiles.end();)
   {
      if (it->first != DESFIRE_ISO_MASTERFILE_ID)
         it = directoryFiles.erase(it);
      else
         ++it;
   }

   // set picc as dirty state
   dirty = true;
}

/*
 * check if file exists
 */
bool Instance::hasFile(unsigned int fileId) const
{
   assert(application != nullptr);
   return application->files.find(fileId) != application->files.end();
}

/*
 * add file to current selected application
 */
void Instance::addFile(const FileEntry &file)
{
   assert(application != nullptr);

   application->files.emplace(file.fileId, file);

   // add ISO ElementaryFile
   if (isApplicationIsoEnabled())
   {
      const ElementaryFile ef {
         .isoId = file.isoId,
         .fileId = file.fileId
      };

      isoAddElementaryFile(ef);
   }

   // set picc as dirty state
   dirty = true;
}

/*
 * delete file from current selected application
 */
void Instance::deleteFile(const unsigned int fileId)
{
   assert(application != nullptr);

   // remove file
   if (application->files.erase(fileId) > 0)
   {
      // set picc as dirty state
      dirty = true;
   }
}

/*
 * list al files inside curren application
 */
std::vector<unsigned int> Instance::listFiles() const
{
   assert(application != nullptr);

   std::vector<unsigned int> files;

   for (auto &[id, file]: application->files)
      files.push_back(id);

   return files;
}

/*
 * get file reference by its ID
 */
FileEntry *Instance::getFile(unsigned int fileId) const
{
   assert(application != nullptr);
   return &application->files.find(fileId)->second;
}

/*
 * get file reference by its Short File ID (SFI = isoId & 0x1F)
 */
FileEntry *Instance::getFileByShortFID(unsigned int sfi) const
{
   if (directoryFile == nullptr)
      return nullptr;

   for (const auto &[id, ef]: directoryFile->ef)
   {
      if ((ef.isoId & 0x1F) == sfi)
         return getFile(ef.fileId);
   }
   return nullptr;
}

/*
 * check effective communication mode for read operations
 */
unsigned int Instance::authorizeForRead(const unsigned int commSettings, const std::initializer_list<unsigned int> &keys) const
{
   // check keys grant free access
   if (isAuthenticatedAnyKeys(keys))
   {
      // for Legacy auth, keep file settings
      if (isAuthenticatedLegacy())
         return commSettings;

      // in ISO and AES authentication there are no really PLAIN communication, transform to MACING
      return commSettings == PlainCommunication ? MACedCommunication : commSettings;
   }

   // check keys grant free access
   if (isFreeAccessAnyKeys(keys))
      return PlainCommunication;

   // none of the keys grant communication... trigger AUTHENTICATION ERROR
   return -1;
}

/*
 * check effective communication mode for write operations
 */
unsigned int Instance::authorizeForWrite(const unsigned int commSettings, const std::initializer_list<unsigned int> &keys) const
{
   // check if current authentication match keys
   if (isAuthenticatedAnyKeys(keys))
      return commSettings;

   // check keys grant free access
   if (isFreeAccessAnyKeys(keys))
      return PlainCommunication;

   // none of the keys grant communication... trigger AUTHENTICATION ERROR
   return -1;
}

/*
 * update master key settings of current selected application
 */
void Instance::setKeySettings(unsigned int keySettings)
{
   assert(application != nullptr);

   // set picc as dirty state if key settings is changed
   if (application->keySettings != keySettings)
      dirty = true;

   application->keySettings = keySettings;
}

/*
 * check if current master key settings allow free directory listing
 */
bool Instance::isFreeDirectoryListing() const
{
   assert(application != nullptr);
   return application->keySettings & DESFIRE_KEY_SETTINGS_FREE_DIRECTORY_LISTING;
}

/*
 * check if current master key settings allow free create / delete
 */
bool Instance::isFreeCreateDelete() const
{
   assert(application != nullptr);
   return application->keySettings & DESFIRE_KEY_SETTINGS_FREE_CREATE_DELETE;
}

/*
 * check current master key settings allow change configuration
 */
bool Instance::isAllowChangeConfig() const
{
   assert(application != nullptr);
   return application->keySettings & DESFIRE_KEY_SETTINGS_ALOW_CHANGE_CONFIG;
}

/*
 * check if current master key settings allow change master key
 */
bool Instance::isMasterKeyChangeable() const
{
   assert(application != nullptr);
   return application->keySettings & DESFIRE_KEY_SETTINGS_ALOW_CHANGE_MASTER_KEY;
}

/*
 * check if current authentication status allow to change given key
 */
bool Instance::isKeyChangeable(unsigned int keyId) const
{
   assert(auth != nullptr);
   assert(application != nullptr);

   // for PICC check if master key is changeable
   if (isApplicationSelected(DESFIRE_MASTER_APP_ID))
      return isMasterKeyChangeable();

   // get application changeKey
   const unsigned int changeKey = application->keySettings >> 4;

   // authentication with the key to be changed is necessary
   if (changeKey == 0x0E)
      return auth->keyEntry->id == keyId;

   // all keys except application master key are frozen
   if (changeKey == 0x0F)
      return keyId == DESFIRE_MASTER_KEY_ID && isMasterKeyChangeable();

   // authentication with a specific key is necessary
   return auth->keyEntry->id == changeKey;
}

/*
 * check if selected application has key
 */
bool Instance::hasKeyEntry(unsigned int keyNo) const
{
   assert(application != nullptr);
   return application->keys.find(keyNo) != application->keys.end();
}

/*
 * get key entry within current selected application
 */
KeyEntry *Instance::getKeyEntry(unsigned int keyNo) const
{
   assert(application != nullptr);
   return &application->keys.find(keyNo)->second;
}

/*
 * check if it has valid authentication state (any key and any mode)
 */
bool Instance::isAuthenticated() const
{
   return auth != nullptr;
}

/*
 * check if it has valid Legacy authentication state (any key)
 */
bool Instance::isAuthenticatedLegacy() const
{
   return auth != nullptr && auth->mode == LegacyAuthentication;
}

/*
 * check if it has valid ISO authentication state (any key)
 */
bool Instance::isAuthenticatedISO() const
{
   return auth != nullptr && auth->mode == ISOAuthentication;
}

/*
 * check if it has valid AES authentication state (any key)
 */
bool Instance::isAuthenticatedAES() const
{
   return auth != nullptr && auth->mode == AESAuthentication;
}

/*
 * check if it has valid authentication state with specified key
 */
bool Instance::isAuthenticatedWithKey(unsigned int key) const
{
   return auth && auth->keyEntry->id == key;
}

/*
 * check if it has valid authentication state with specified key
 */
bool Instance::isAuthenticatedAnyKeys(const std::initializer_list<unsigned int> &keys) const
{
   if (auth == nullptr)
      return false;

   return std::any_of(keys.begin(), keys.end(), [&](const unsigned int key) {
      return this->isAuthenticatedWithKey(key);
   });
}

/*
 * check if it has valid authentication with master key (id = 0)
 */
bool Instance::isAuthenticatedWithMasterKey() const
{
   return isAuthenticatedAnyKeys({DESFIRE_MASTER_KEY_ID});
}

/*
 * check if it has valid authentication to read specified file
 */
bool Instance::isAuthenticatedForRead(const FileEntry *file) const
{
   return isAuthenticatedAnyKeys({file->readKey(), file->readWriteKey()});
}

/*
 * check if it has valid authentication to write specified file
 */
bool Instance::isAuthenticatedForWrite(const FileEntry *file) const
{
   return isAuthenticatedAnyKeys({file->writeKey(), file->readWriteKey()});
}

/*
 * check if it has valid authentication to read specified file
 */
bool Instance::isAuthenticatedForReadWrite(const FileEntry *file) const
{
   return isAuthenticatedAnyKeys({file->readWriteKey()});
}

/*
 * invalidate instance authentication state
 */
void Instance::setAuthentication(Authentication &authentication)
{
   auth = &authentication;
}

/*
 * invalidate instance authentication state
 */
void Instance::invalidateAuth()
{
   auth = nullptr;
}

/*
 * check if file has free access for all keys (read / write & readWrite)
 */
bool Instance::hasFreeFullAccess(const FileEntry *file) const
{
   assert(file != nullptr);
   return isFreeAccessKey(file->readKey()) && isFreeAccessKey(file->writeKey()) && isFreeAccessKey(file->readWriteKey());
}

/*
 * check if file has free read access
 */
bool Instance::hasFreeReadAccess(const FileEntry *file) const
{
   assert(file != nullptr);
   return isFreeAccessKey(file->readKey());
}

/*
 * check if file has free read access
 */
bool Instance::hasFreeWriteAccess(const FileEntry *file) const
{
   assert(file != nullptr);
   return isFreeAccessKey(file->writeKey());
}

/*
 * check if file has free read access
 */
bool Instance::hasFreeReadWriteAccess(const FileEntry *file) const
{
   assert(file != nullptr);
   return isFreeAccessKey(file->readWriteKey());
}

/*
 * check current authenticated key has read access to file
 */
bool Instance::hasReadAccess(const FileEntry *file) const
{
   assert(file != nullptr);
   return hasAccess(file->readKey());
}

/*
 * check current authenticated key has write access to file
 */
bool Instance::hasWriteAccess(const FileEntry *file) const
{
   assert(file != nullptr);
   return hasAccess(file->writeKey());
}

/*
 * check current authenticated key has read/write access to file
 */
bool Instance::hasReadWriteAccess(const FileEntry *file) const
{
   assert(file != nullptr);
   return hasAccess(file->readWriteKey());
}

/*
 * check current authenticated key can change file settings
 */
bool Instance::hasChangeAccess(const FileEntry *file) const
{
   assert(file != nullptr);
   return hasAccess(file->changeRightsKey());
}

/*
 * check if key has access with current authentication state
 */
bool Instance::hasAccess(unsigned int key) const
{
   // free access
   if (isFreeAccessKey(key))
      return true;

   // never access
   if (isNeverAccessKey(key))
      return false;

   return auth && auth->keyEntry->id == key;
}

/*
 * check if some of the keys is free access
 */
bool Instance::isFreeAccessAnyKeys(const std::initializer_list<unsigned int> &keys) const
{
   return std::any_of(keys.begin(), keys.end(), [&](const unsigned int key) { return isFreeAccessKey(key); });
}

/*
 * check if key has free access
 */
bool Instance::isFreeAccessKey(unsigned int key) const
{
   return key == 0x0E;
}

/*
 * check if some of the keys is free access
 */
bool Instance::isNeverAccessAllKeys(const std::initializer_list<unsigned int> &keys) const
{
   return std::all_of(keys.begin(), keys.end(), [&](const unsigned int key) { return isNeverAccessKey(key); });
}

/*
 * check if key is never access
 */
bool Instance::isNeverAccessKey(unsigned int key) const
{
   return key == 0x0F;
}

/*
 * commit transaction
 */
bool Instance::commitData()
{
   if (!application)
      return false;

   // flag to detect has commited data
   bool committed = false;

   // for each file copy all backup data to main data
   for (auto &[id, file]: application->files)
   {
      if (file.commit())
      {
         committed = true;
         LOG_DEBUG(log, "\t[{02x}/{3}] {x}", {file.fileId, file.data.remaining(), file.data});
      }
   }

   if (committed)
   {
      LOG_INFO(log, "transaction committed");

      // set picc as dirty state
      dirty = true;
   }

   return committed;
}

/*
 * rollback transaction
 */
bool Instance::rollbackData()
{
   if (!application)
      return false;

   // flag to detect has rolledback data
   bool rolledback = false;

   // for each file copy all backup data to main data
   for (auto &[id, file]: application->files)
   {
      if (file.rollback())
      {
         rolledback = true;
         LOG_DEBUG(log, "\t[{02x}/{3}] {x}", {file.fileId, file.data.remaining(), file.data});
      }
   }

   return rolledback;
}

/*
 * detect communication mode and receive data from reader
 */
int Instance::decodeData(const rt::ByteBuffer &data, const unsigned int length, const unsigned int commSettings)
{
   return decodeData(data, length, commSettings, {0x0E});
}

/*
 * detect communication mode and receive data from reader with keys check
 */
int Instance::decodeData(const rt::ByteBuffer &data, const unsigned int length, const unsigned int commSettings, const std::initializer_list<unsigned int> &keys)
{
   // check if access is denied
   if (isNeverAccessAllKeys(keys))
      return DESFIRE_STATUS_PERMISSION_DENIED;

   switch (authorizeForWrite(commSettings, keys))
   {
      case PlainCommunication:
         return decodeDataPlain(data, length);

      case MACedCommunication:
         return decodeDataMacing(data, length);

      case CryptCommunication:
         return decodeDataCrypto(data, length);

      default:
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;
   }
}

/*
 * receive data from reader in PLAIN mode
 */
int Instance::decodeDataPlain(const rt::ByteBuffer &data, const unsigned int length)
{
   // add data to write buffer
   buffer.put(data);

   // check if all data is received
   if (buffer.position() < length)
      return DESFIRE_STATUS_ADDITIONAL_FRAME;

   // check if all data is received
   if (buffer.position() != length)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // prepare buffer changes
   buffer.flip();

   // in ISO / AES authentication, CMAC must be processed to keep track of IV
   if (isAuthenticatedISO() || isAuthenticatedAES())
      requestCmac(header, data);

   // finish receive data
   return DESFIRE_STATUS_OK;
}

/*
 * receive data from reader in MACING mode
 */
int Instance::decodeDataMacing(const rt::ByteBuffer &data, const unsigned int length)
{
   assert(auth != nullptr);

   // legacy auth has 4 byte MAC and ISO / AES 8 byte CMAC
   const unsigned int cmacLength = isAuthenticatedLegacy() ? 4 : 8;

   // add data to write
   buffer.put(data);

   // check if all data is received, including CMAC
   if (buffer.position() < length + cmacLength)
      return DESFIRE_STATUS_ADDITIONAL_FRAME;

   // check total length
   if (buffer.position() != length + cmacLength)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // commit data
   buffer.flip();

   // get CMAC received
   const rt::ByteBuffer receivedCmac = buffer.popBuffer(cmacLength);

   // check if CMAC is valid
   if (receivedCmac != requestCmac(header, buffer))
      return DESFIRE_STATUS_INTEGRITY_ERROR;

   // finish receive data
   return DESFIRE_STATUS_OK;
}

/*
 * receive data from reader in CRYPTO mode
 */
int Instance::decodeDataCrypto(const rt::ByteBuffer &data, const unsigned int length)
{
   assert(auth != nullptr);

   // calculate lengths...
   const unsigned int crclen = isAuthenticatedLegacy() ? 2 : 4;
   const unsigned int block = isAuthenticatedAES() ? 16 : 8;
   const unsigned int padding = (length + crclen) % block ? block - (length + crclen) % block : 0;

   // add data to write
   buffer.put(data);

   // check if all data is received, include crc + padding (if required)
   if (buffer.position() < length + crclen + padding)
      return DESFIRE_STATUS_ADDITIONAL_FRAME;

   // check total length
   if (buffer.position() != length + crclen + padding)
      return DESFIRE_STATUS_LENGTH_ERROR;

   // prepare buffer for decrypt
   buffer.flip();

   // prepare output buffer
   rt::ByteBuffer plain;

   switch (auth->mode)
   {
      case LegacyAuthentication:
      {
         if (const int result = decryptDataEV0(buffer, plain, length); result != DESFIRE_STATUS_OK)
            return result;

         break;
      }

      // ISOAuthentication or AESAuthentication:
      default:
      {
         // prepare params buffer
         rt::ByteBuffer params(32);
         params.putInt(command, 1).put(header).flip();

         // decrypt data to write
         if (const int result = decryptDataEV1(params, buffer, plain, length); result != DESFIRE_STATUS_OK)
            return result;

         break;
      }
   }

   // clear buffer to store decrypted data
   buffer.clear();

   // copy decrypted data to buffer
   buffer.put(plain).flip();

   // finish receive data
   return DESFIRE_STATUS_OK;
}

/*
 * prepare data to send,
 */
int Instance::encodeData(rt::ByteBuffer &data, const unsigned int length, const unsigned int commSettings)
{
   return encodeData(data, length, commSettings, {0x0E});
}

/*
 * prepare data to send with keys check
 */
int Instance::encodeData(rt::ByteBuffer &data, const unsigned int length, const unsigned int commSettings, const std::initializer_list<unsigned int> &keys)
{
   // check if access is denied
   if (isNeverAccessAllKeys(keys))
      return DESFIRE_STATUS_PERMISSION_DENIED;

   // LOG_DEBUG(log, "\tdata[{}]: {x}", {data.size(), data});

   // clear transmit buffer
   buffer.clear();

   // check authorization mode
   switch (authorizeForRead(commSettings, keys))
   {
      case PlainCommunication:
         return encodeDataPlain(data, length);

      case MACedCommunication:
         return encodeDataMacing(data, length);

      case CryptCommunication:
         return encodeDataCrypto(data, length);

      default:
         return DESFIRE_STATUS_AUTHENTICATION_ERROR;
   }
}

/*
 * prepare data to send in PLAIN mode
 */
int Instance::encodeDataPlain(const rt::ByteBuffer &data, const unsigned int length)
{
   // plain communication, nothing to do, only copy content
   buffer.put(data);

   // in ISO / AES authentication, CMAC must be added to response to keep track of IV
   if (isAuthenticatedISO() || isAuthenticatedAES())
      buffer.put(responseCmac(data));

   // prepare buffer for transmit
   buffer.flip();

   return DESFIRE_STATUS_OK;
}

/*
 * prepare data to send in MACING mode
 */
int Instance::encodeDataMacing(const rt::ByteBuffer &data, const unsigned int length)
{
   assert(auth != nullptr);

   // write data + CMAC
   buffer.put(data);
   buffer.put(responseCmac(data));

   // prepare buffer for transmit
   buffer.flip();

   return DESFIRE_STATUS_OK;
}

/*
 * prepare data to send in CRYPTO mode
 */
int Instance::encodeDataCrypto(const rt::ByteBuffer &data, const unsigned int length)
{
   assert(auth != nullptr);

   // prepare output buffer
   rt::ByteBuffer crypt;

   switch (auth->mode)
   {
      case LegacyAuthentication:
         encryptDataEV0(data, crypt, length);
         break;

      // ISOAuthentication or AESAuthentication:
      default:
         encryptDataEV1(data, crypt, length);
         break;
   }

   // add encrypted data to buffer
   buffer.put(crypt).flip();

   return DESFIRE_STATUS_OK;
}

/*
 * peek next block to send from buffer
 */
int Instance::sendData(rt::ByteBuffer &response, const unsigned int size)
{
   // next block to send
   rt::ByteBuffer block(size < DESFIRE_MAX_FRAME_SIZE ? size : 59);

   // extract data from buffer
   buffer.get(block);

   // add block to response
   response.put(block);

   // return  status byte
   return buffer.isEmpty() ? DESFIRE_STATUS_OK : DESFIRE_STATUS_ADDITIONAL_FRAME;
}

/*
* send successful ACK and append CMAC if required
*/
int Instance::sendAck(rt::ByteBuffer &response)
{
   // in ISO / AES authentication, CMAC must be added to response to keep track of IV
   if (isAuthenticatedISO() || isAuthenticatedAES())
      response.put(responseCmac());

   return DESFIRE_STATUS_OK;
}

/*
 * encrypt data using legacy 3DES
 */
void Instance::encryptDataEV0(const rt::ByteBuffer &input, rt::ByteBuffer &output, unsigned int length) const
{
   assert(auth != nullptr);
   assert(auth->cipher != nullptr);

   // DES block size in bytes
   constexpr unsigned int blockSize = 8;

   // buffer for encryption
   rt::ByteBuffer tmp(input.remaining() + 2 + blockSize);

   // add data to temp buffer
   tmp.put(input);

   // add crc to temp buffer
   tmp.putInt(CRC::iso14443A(input), 2);

   // add 0x80 if length is zero
   if (length == 0)
      tmp.put(0x80);

   // add padding up to block size
   if (const unsigned int padding = blockSize - (tmp.position() % blockSize); padding < blockSize)
   {
      for (int i = 0; i < padding; i++)
         tmp.put(0x00);
   }

   // prepare buffer for reading
   tmp.flip();

   // encrypt data — no chain IV across commands (CBC send mode)
   rt::ByteBuffer copyIv = auth->sessionIv.copy();
   output = auth->cipher->encrypt(tmp, copyIv);
}

/*
 * decrypt data using legacy 3DES
 */
int Instance::decryptDataEV0(const rt::ByteBuffer &input, rt::ByteBuffer &output, unsigned int length) const
{
   assert(auth != nullptr);
   assert(auth->cipher != nullptr);

   // buffer for decryption
   rt::ByteBuffer tmp(length);

   // decrypt data — no chain IV across commands (CBC recv mode)
   rt::ByteBuffer copyIv = auth->sessionIv.copy();
   rt::ByteBuffer plain = auth->cipher->decrypt(input, copyIv);

   // read decrypted data
   plain.get(tmp);

   // verify CRC of changed key
   if (const unsigned short check = plain.getInt(2); check != CRC::iso14443A(tmp))
      return DESFIRE_STATUS_INTEGRITY_ERROR;

   // return decrypted data
   output = tmp;

   // decrypt OK
   return DESFIRE_STATUS_OK;
}

/*
 * encrypt data using standard 3DES/AES
 */
void Instance::encryptDataEV1(const rt::ByteBuffer &input, rt::ByteBuffer &output, unsigned int length) const
{
   assert(auth != nullptr);
   assert(auth->cipher != nullptr);

   const unsigned int blockSize = auth->mode == AESAuthentication ? 16 : 8;

   // buffer for encryption and crc
   rt::ByteBuffer tmp(input.remaining() + 4 + blockSize);
   rt::ByteBuffer crc(input.remaining() + 1);

   // prepare CRC buffer data to encrypt
   crc.put(input).put(DESFIRE_STATUS_OK).flip();

   // add data to temp buffer
   tmp.put(input);

   // add crc to temp buffer
   tmp.putInt(CRC::ccitt32(crc), 4);

   // add 0x80 if length is zero
   if (length == 0)
      tmp.put(0x80);

   // add padding up to block size
   if (const unsigned int padding = blockSize - (tmp.position() % blockSize); padding < blockSize)
   {
      for (int i = 0; i < padding; i++)
         tmp.put(0x00);
   }

   // prepare buffer for reading
   tmp.flip();

   // encrypt data
   output = auth->cipher->encrypt(tmp, auth->sessionIv);
}

/*
 * decrypt data using standard 3DES/AES
 */
int Instance::decryptDataEV1(const rt::ByteBuffer &params, const rt::ByteBuffer &input, rt::ByteBuffer &output, unsigned int length) const
{
   assert(auth != nullptr);
   assert(auth->cipher != nullptr);

   // buffer for encryption and crc
   rt::ByteBuffer tmp(length);
   rt::ByteBuffer crc(params.remaining() + tmp.remaining());

   // encrypt data
   rt::ByteBuffer plain = auth->cipher->decrypt(input, auth->sessionIv);

   // read decrypted data
   plain.get(tmp);

   // verify CRC of received data
   crc.put(params).put(tmp).flip();

   // verify CRC of received data
   if (const unsigned int check = plain.getInt(4); check != CRC::ccitt32(crc))
      return DESFIRE_STATUS_INTEGRITY_ERROR;

   // return decrypted data
   output = tmp;

   // decrypt OK
   return DESFIRE_STATUS_OK;
}

rt::ByteBuffer Instance::requestCmac(const rt::ByteBuffer &header, const rt::ByteBuffer &data)
{
   rt::ByteBuffer tmp(header.remaining() + data.remaining());

   // for ISO and AES auth, prepend data with command info (parameters)
   if (isAuthenticatedISO() || isAuthenticatedAES())
      tmp.put(header);

   tmp.put(data).flip();

   return requestCmac(tmp);
}

rt::ByteBuffer Instance::requestCmac(const rt::ByteBuffer &data)
{
   // buffer for CMAC input
   rt::ByteBuffer tmp(1 + data.remaining());

   // for ISO and AES auth, prepend data with command
   if (isAuthenticatedISO() || isAuthenticatedAES())
      tmp.put(command);

   // add data
   tmp.put(data);

   // commit buffer
   tmp.flip();

   // build CMAC for request
   return cmac(tmp);
}

/*
 * macing data using standard 3DES/AES
 */
rt::ByteBuffer Instance::responseCmac(const rt::ByteBuffer &data)
{
   constexpr unsigned char status = DESFIRE_STATUS_OK;

   // buffer for CMAC input
   rt::ByteBuffer tmp(1 + data.remaining());

   // add data and flip
   tmp.put(data);

   // for ISO and AES auth, add status to buffer
   if (isAuthenticatedISO() || isAuthenticatedAES())
      tmp.put(status);

   // commit buffer
   tmp.flip();

   // build CMAC for response
   return cmac(tmp);
}

/*
 * macing data using standard 3DES/AES
 */
rt::ByteBuffer Instance::cmac(const rt::ByteBuffer &data) const
{
   if (!isAuthenticated())
      return rt::ByteBuffer::empty();

   switch (auth->mode)
   {
      case LegacyAuthentication:
      {
         rt::ByteBuffer temp(data.remaining() + 8);
         temp.put(data);
         temp.padding(0, 8);
         temp.flip();
         return auth->cipher->encrypt(temp).slice(-8, 4);
      }
      case ISOAuthentication:
      {
         auth->sessionIv = CMAC::cmac(auth->sessionKey, data, auth->sessionIv, CMAC::CmacTDES);
         return auth->sessionIv.copy();
      }
      case AESAuthentication:
      {
         auth->sessionIv = CMAC::cmac(auth->sessionKey, data, auth->sessionIv, CMAC::CmacAES128);
         return auth->sessionIv.slice(0, 8);
      }

      default:
         return rt::ByteBuffer::empty();
   }
}

/*
 *
 */
void Instance::readHeader(const rt::ByteBuffer &request, unsigned int length)
{
   request.peek(header, length);
}

/*
 *
 */
void Instance::updateIv(const rt::ByteBuffer &request, const unsigned int length)
{
   // read header
   request.peek(header, length);

   // legacy auth do not update IV vector
   if (!auth || auth->mode == LegacyAuthentication)
      return;

   // prepare buffer for request CMAC
   rt::ByteBuffer plain(1 + request.remaining());
   plain.put(command).put(request).flip();

   // update sessionIv for next cryptographic operation
   if (auth->mode == ISOAuthentication)
      auth->sessionIv = CMAC::cmac(auth->sessionKey, plain, auth->sessionIv, CMAC::CmacTDES);
   else
      auth->sessionIv = CMAC::cmac(auth->sessionKey, plain, auth->sessionIv, CMAC::CmacAES128);
}

}
