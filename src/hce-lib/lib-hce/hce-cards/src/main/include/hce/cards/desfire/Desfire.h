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

#ifndef HCE_CARDS_DESFIRE_H
#define HCE_CARDS_DESFIRE_H

#include <functional>
#include <memory>
#include <vector>

#include <rt/Logger.h>
#include <rt/ByteBuffer.h>

namespace hce::cards::desfire {

/* ------------------------------------------------------------------ */
/* APDU class bytes                                                   */
/* ------------------------------------------------------------------ */
static constexpr int CLA_ISO = 0x00;
static constexpr int CLA_WRAPPER = 0x90;

/* ------------------------------------------------------------------ */
/* Instruction codes                                                  */
/* ------------------------------------------------------------------ */
static constexpr int INS_AUTHENTICATE_LEGACY = 0x0A;
static constexpr int INS_AUTHENTICATE_ISO = 0x1A;
static constexpr int INS_AUTHENTICATE_AES = 0xAA;
static constexpr int INS_CHANGE_KEY = 0xC4;
static constexpr int INS_CHANGE_KEY_SETTINGS = 0x54;
static constexpr int INS_GET_KEY_SETTINGS = 0x45;
static constexpr int INS_GET_KEY_VERSION = 0x64;

static constexpr int INS_GET_VERSION = 0x60;
static constexpr int INS_GET_FREE_MEMORY = 0x6E;
static constexpr int INS_CREATE_APPLICATION = 0xCA;
static constexpr int INS_DELETE_APPLICATION = 0xDA;
static constexpr int INS_SELECT_APPLICATION = 0x5A;
static constexpr int INS_FORMAT_PICC = 0xFC;
static constexpr int INS_LIST_APPLICATIONS = 0x6A;
static constexpr int INS_LIST_DF_NAMES = 0x6D;

static constexpr int INS_CREATE_STD_FILE = 0xCD;
static constexpr int INS_CREATE_BACKUP_FILE = 0xCB;
static constexpr int INS_CREATE_VALUE_FILE = 0xCC;
static constexpr int INS_CREATE_LINEAR_FILE = 0xC1;
static constexpr int INS_CREATE_CYCLIC_FILE = 0xC0;
static constexpr int INS_CHANGE_FILE_SETTINGS = 0x5F;
static constexpr int INS_GET_FILE_SETTINGS = 0xF5;
static constexpr int INS_DELETE_FILE = 0xDF;
static constexpr int INS_LIST_FILES = 0x6F;

static constexpr int INS_READ_DATA = 0xBD;
static constexpr int INS_WRITE_DATA = 0x3D;
static constexpr int INS_GET_VALUE = 0x6C;
static constexpr int INS_CREDIT = 0x0C;
static constexpr int INS_DEBIT = 0xDC;
static constexpr int INS_LIMITED_CREDIT = 0x1C;
static constexpr int INS_WRITE_RECORD = 0x3B;
static constexpr int INS_READ_RECORDS = 0xBB;
static constexpr int INS_CLEAR_RECORD_FILE = 0xEB;
static constexpr int INS_COMMIT_TRANSACTION = 0xC7;
static constexpr int INS_ABORT_TRANSACTION = 0xA7;
static constexpr int INS_CONTINUE = 0xAF;

static constexpr int INS_GET_CARD_UID = 0x51;
static constexpr int INS_GET_ISO_FILE_IDS = 0x61;

static constexpr int INS_ISO_SELECT = 0xA4;
static constexpr int INS_ISO_READ_BINARY = 0xB0;
static constexpr int INS_ISO_UPDATE_BINARY = 0xD6;
static constexpr int INS_ISO_READ_RECORDS = 0xB2;

/* ------------------------------------------------------------------ */
/* Native-protocol status bytes (SW1=0x91, SW2=status)                */
/* ------------------------------------------------------------------ */
static constexpr int DESFIRE_SW1 = 0x91;

static constexpr int STATUS_OK = 0x00;
static constexpr int STATUS_NO_CHANGES = 0x0C;
static constexpr int STATUS_OUT_OF_EEPROM = 0x0E;
static constexpr int STATUS_ILLEGAL_COMMAND = 0x1C;
static constexpr int STATUS_INTEGRITY_ERROR = 0x1E;
static constexpr int STATUS_NO_SUCH_KEY = 0x40;
static constexpr int STATUS_LENGTH_ERROR = 0x7E;
static constexpr int STATUS_PERMISSION_DENIED = 0x9D;
static constexpr int STATUS_PARAMETER_ERROR = 0x9E;
static constexpr int STATUS_APPLICATION_NOT_FOUND = 0xA0;
static constexpr int STATUS_APPL_INTEGRITY_ERROR = 0xA1;
static constexpr int STATUS_AUTHENTICATION_ERROR = 0xAE;
static constexpr int STATUS_ADDITIONAL_FRAME = 0xAF;
static constexpr int STATUS_BOUNDARY_ERROR = 0xBE;
static constexpr int STATUS_PICC_INTEGRITY_ERROR = 0xC1;
static constexpr int STATUS_COMMAND_ABORTED = 0xCA;
static constexpr int STATUS_PICC_DISABLED = 0xCD;
static constexpr int STATUS_COUNT_ERROR = 0xCE;
static constexpr int STATUS_DUPLICATE_ERROR = 0xDE;
static constexpr int STATUS_EEPROM_ERROR = 0xEE;
static constexpr int STATUS_FILE_NOT_FOUND = 0xF0;
static constexpr int STATUS_FILE_INTEGRITY_ERROR = 0xF1;

/* ISO status */
static constexpr int ISO_STATUS_OK = 0x9000;
static constexpr int ISO_STATUS_NOT_ENOUGH_DATA = 0x6985;
static constexpr int ISO_STATUS_WRONG_LENGTH = 0x6700;
static constexpr int ISO_STATUS_ACCESS_NOT_ALLOWED = 0x6982;
static constexpr int ISO_STATUS_FILE_NOT_FOUND = 0x6A82;
static constexpr int ISO_STATUS_WRONG_PARAMETERS_P1P2 = 0x6A86;
static constexpr int ISO_STATUS_WRONG_PARAMETERS_LC = 0x6A87;
static constexpr int ISO_STATUS_WRONG_PARAMETERS_LE = 0x6C00;
static constexpr int ISO_STATUS_INS_NOT_SUPPORTED = 0x6D00;
static constexpr int ISO_STATUS_CLA_NOT_SUPPORTED = 0x6E00;
static constexpr int ISO_STATUS_NO_DIAGNOSTIC = 0x6F00;

/* ------------------------------------------------------------------ */
/* Enumerations                                                         */
/* ------------------------------------------------------------------ */
enum AuthMode
{
   AUTH_NONE = 0,
   AUTH_LEGACY = 1,
   AUTH_ISO = 2,
   AUTH_AES = 3,
};

enum CommMode
{
   COMM_PLAIN = 0x00,
   COMM_MACING = 0x01,
   COMM_CRYPT = 0x03,
};

enum FileType
{
   FILE_STANDARD = 0x00,
   FILE_BACKUP = 0x01,
   FILE_VALUE = 0x02,
   FILE_LINEAR_RECORD = 0x03,
   FILE_CYCLIC_RECORD = 0x04,
};

/* ------------------------------------------------------------------ */
/* Data structures                                                      */
/* ------------------------------------------------------------------ */
struct VersionInfo
{
   /* Hardware */
   unsigned int hwVendorId = 0;
   unsigned int hwType = 0;
   unsigned int hwSubtype = 0;
   unsigned int hwMajorVersion = 0;
   unsigned int hwMinorVersion = 0;
   unsigned int hwStorageSize = 0;
   unsigned int hwProtocol = 0;

   /* Software */
   unsigned int swVendorId = 0;
   unsigned int swType = 0;
   unsigned int swSubtype = 0;
   unsigned int swMajorVersion = 0;
   unsigned int swMinorVersion = 0;
   unsigned int swStorageSize = 0;
   unsigned int swProtocol = 0;

   /* Production */
   rt::ByteBuffer uid;
   rt::ByteBuffer batch;
   unsigned int productionWeek = 0;
   unsigned int productionYear = 0;
};

struct FileSettings
{
   unsigned int fileType = 0;
   unsigned int commSettings = 0;
   unsigned int accessRights = 0;

   /* Data / Backup files */
   unsigned int fileSize = 0;

   /* Value files */
   int lowerLimit = 0;
   int upperLimit = 0;
   int limitedCreditValue = 0;
   bool limitedCreditEnabled = false;

   /* Record files */
   unsigned int recordSize = 0;
   unsigned int maxRecords = 0;
   unsigned int currentRecords = 0;

   bool isDataFile() const
   {
      return fileType == FILE_STANDARD || fileType == FILE_BACKUP;
   }

   bool isValueFile() const
   {
      return fileType == FILE_VALUE;
   }

   bool isRecordFile() const
   {
      return fileType == FILE_LINEAR_RECORD || fileType == FILE_CYCLIC_RECORD;
   }

   bool isLinearRecordFile() const
   {
      return fileType == FILE_LINEAR_RECORD;
   }

   bool isCyclicRecordFile() const
   {
      return fileType == FILE_CYCLIC_RECORD;
   }

   /*
    * check if file has configured with PLAIN communication mode
    */
   bool isPlainComm() const
   {
      return commSettings == COMM_PLAIN;
   }

   /*
    * check if file has configured with MACED communication mode
    */
   bool isMACedComm() const
   {
      return commSettings == COMM_MACING;
   }

   /*
    * check if file has configured with FULL ENCIPHERED communication mode
    */
   bool isCryptoComm() const
   {
      return commSettings == COMM_CRYPT;
   }

   /*
    * get read key
    */
   unsigned int readKey() const
   {
      return accessRights >> 12 & 0x0F;
   }

   /*
    * get write key
    */
   unsigned int writeKey() const
   {
      return accessRights >> 8 & 0x0F;
   }

   /*
    * get readWrite key
    */
   unsigned int readWriteKey() const
   {
      return accessRights >> 4 & 0x0F;
   }

   /*
    * get change key
    */
   unsigned int changeRightsKey() const
   {
      return accessRights & 0x0F;
   }
};

/* ------------------------------------------------------------------ */
/* Transport interface                                                  */
/*                                                                      */
/* Called with a full APDU request frame (CLA INS P1 P2 [Lc data]).    */
/* Fills response with data bytes + SW1 + SW2.                         */
/* Returns the 16-bit SW value (e.g. 0x9100, 0x91AF, 0x9000).         */
/* ------------------------------------------------------------------ */
using Transport = std::function<int(const rt::ByteBuffer &request, rt::ByteBuffer &response)>;

/* ------------------------------------------------------------------ */
/* Desfire high-level client class                                      */
/* ------------------------------------------------------------------ */
class Desfire
{
   public:

      explicit Desfire(Transport transport);

      ~Desfire();

      /* -------------------------------------------------------------- */
      /* PICC Level Commands                                             */
      /* -------------------------------------------------------------- */

      int getVersionInfo(VersionInfo &info);

      int getFreeMemory(unsigned int &freeMemory);

      int formatCard();

      int createApplication(unsigned int appId, unsigned int keySettings1, unsigned int keySettings2);

      int deleteApplication(unsigned int appId);

      int selectApplication(unsigned int appId);

      int isoSelectByName(const rt::ByteBuffer &isoName);

      int isoSelectById(int isoId);

      int listApplications(std::vector<unsigned int> &appIds);

      int listDFNames(std::vector<rt::ByteBuffer> &dfs);

      int getCardUID(rt::ByteBuffer &uid);

      /* -------------------------------------------------------------- */
      /* Security Commands                                               */
      /* -------------------------------------------------------------- */

      int authenticateLegacy(unsigned int keyId, const rt::ByteBuffer &keyData);

      int authenticateISO(unsigned int keyId, const rt::ByteBuffer &keyData);

      int authenticateAES(unsigned int keyId, const rt::ByteBuffer &keyData);

      int getKeySettings(unsigned int &keySettings, unsigned int &numKeys);

      int getKeyVersion(unsigned int keyId, unsigned int &keyVersion);

      int changeKey(unsigned int keyNo, const rt::ByteBuffer &newKey, const rt::ByteBuffer &oldKey);

      int changeKeySettings(unsigned int newSettings);

      /* -------------------------------------------------------------- */
      /* Application-Level File Management                               */
      /* -------------------------------------------------------------- */

      int listFiles(std::vector<unsigned int> &fileIds);

      int getIsoFileIDs(std::vector<unsigned int> &ids);

      int getFileSettings(unsigned int fileId, FileSettings &settings);

      int changeFileSettings(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey);

      int createStandardFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int fileSize);

      int createApplicationIso(unsigned int appId, unsigned int keySettings1, unsigned int keySettings2, unsigned int isoId, const rt::ByteBuffer &isoName);

      int createStandardFileIso(unsigned int fileId, unsigned int isoFileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int fileSize);

      int createBackupFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int fileSize);

      int createValueFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, int lowerLimit, int upperLimit, int initialValue, bool limitedCredit);

      int createLinearRecordFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int recordSize, unsigned int maxRecords);

      int createCyclicRecordFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int recordSize, unsigned int maxRecords);

      int deleteFile(unsigned int fileId);

      /* -------------------------------------------------------------- */
      /* Data Manipulation Commands                                      */
      /* -------------------------------------------------------------- */

      int readData(unsigned int fileId, unsigned int offset, unsigned int length, CommMode mode, rt::ByteBuffer &data);

      int writeData(unsigned int fileId, unsigned int offset, unsigned int length, CommMode mode, const rt::ByteBuffer &data);

      int getValue(unsigned int fileId, CommMode mode, int &value);

      int credit(unsigned int fileId, int amount, CommMode mode);

      int debit(unsigned int fileId, int amount, CommMode mode);

      int limitedCredit(unsigned int fileId, int amount, CommMode mode);

      int readRecords(unsigned int fileId, unsigned int offset, unsigned int count, CommMode mode, rt::ByteBuffer &data);

      int writeRecord(unsigned int fileId, unsigned int offset, unsigned int length, CommMode mode, const rt::ByteBuffer &data);

      int clearRecords(unsigned int fileId);

      int commitTransaction();

      int abortTransaction();

      int isoReadBinary(unsigned int p1, unsigned int p2, unsigned int le, rt::ByteBuffer &data);

      int isoUpdateBinary(unsigned int p1, unsigned int p2, const rt::ByteBuffer &data);

      /* -------------------------------------------------------------- */
      /* Session state accessors                                         */
      /* -------------------------------------------------------------- */

      AuthMode authMode() const;

      int authKeyId() const;

   private:

      struct Impl;

      std::unique_ptr<Impl> impl;
};

} // namespace hce::cards::desfire

#endif // HCE_CARDS_DESFIRE_H
