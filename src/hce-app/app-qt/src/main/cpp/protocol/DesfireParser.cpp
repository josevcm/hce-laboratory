/*

  This file is part of HCE-LABORATORY.

  Copyright (C) 2026 Jose Vicente Campos Martinez, <josevcm@gmail.com>

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

#include <algorithm>

#include <QMap>
#include <QStringList>

#include "DesfireParser.h"

constexpr unsigned char CLA_ISO = 0x00;
constexpr unsigned char CLA_WRAPPED = 0x90;
constexpr unsigned char SW1_WRAPPED = 0x91;

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
static constexpr int INS_SET_CONFIGURATION = 0x5C;

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
static constexpr int INS_ISO_UPDATE_RECORD = 0xD2;
static constexpr int INS_ISO_APPEND_RECORD = 0xE2;
static constexpr int INS_ISO_GET_CHALLENGE = 0x84;
static constexpr int INS_ISO_EXTERNAL_AUTHENTICATE = 0x82;
static constexpr int INS_ISO_INTERNAL_AUTHENTICATE = 0x88;

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

static unsigned int byteAt(const QByteArray &data, const int offset)
{
   if (offset < 0 || offset >= data.size())
      return 0;

   return static_cast<unsigned char>(data[offset]);
}

static unsigned int readLe(const QByteArray &data, const int offset, const int length)
{
   unsigned int value = 0;

   for (int i = 0; i < length && offset + i < data.size(); ++i)
      value |= (byteAt(data, offset + i) << (8 * i));

   return value;
}

static QString hexValue(const unsigned int value, const int width)
{
   return QString("0x%1").arg(value, width, 16, QChar('0'));
}

static QString communicationModeName(const unsigned int value)
{
   switch (value)
   {
      case 0x00: return "plain";
      case 0x01: return "MACed";
      case 0x03: return "encrypted";
      default: return QString("unknown(%1)").arg(hexValue(value, 2));
   }
}

static QString cryptoModeName(const unsigned int value)
{
   switch (value)
   {
      case 0x00: return "2K3DES";
      case 0x01: return "3K3DES";
      case 0x02: return "AES";
      default: return QString("unknown(%1)").arg(hexValue(value, 2));
   }
}

static QString keyTypeName(const unsigned int value)
{
   switch (value)
   {
      case 0x00: return "2K3DES";
      case 0x01: return "3K3DES";
      case 0x02: return "AES";
      default: return QString("unknown(%1)").arg(hexValue(value, 2));
   }
}

static QString accessRightsSummary(const unsigned int accessRights)
{
   return QString("AR=%1").arg(hexValue(accessRights, 4));
}

static QString parseIsoRequestParams(const unsigned int command, const QByteArray &apdu)
{
   QStringList parts;

   if (apdu.size() < 4)
      return "invalid ISO header";

   const unsigned int p1 = byteAt(apdu, 2);
   const unsigned int p2 = byteAt(apdu, 3);
   const int lc = (apdu.size() >= 5) ? static_cast<int>(byteAt(apdu, 4)) : 0;
   const int payloadStart = 5;
   const int available = std::max(0, static_cast<int>(apdu.size()) - payloadStart);
   const int payloadLen = std::min(lc, available);

   parts << QString("P1=%1").arg(hexValue(p1, 2))
      << QString("P2=%1").arg(hexValue(p2, 2));

   if (apdu.size() >= 5)
      parts << QString("Lc=%1").arg(lc);

   switch (command)
   {
      case INS_ISO_UPDATE_BINARY:
      case INS_ISO_UPDATE_RECORD:
      case INS_ISO_APPEND_RECORD:
         parts << QString("file-data tx=%1 bytes").arg(payloadLen)
            << "encrypted=depends file comm";
         break;

      case INS_ISO_EXTERNAL_AUTHENTICATE:
      case INS_ISO_INTERNAL_AUTHENTICATE:
         parts << QString("cryptogram=%1 bytes").arg(payloadLen)
            << "encrypted=yes";
         break;

      default:
         if (payloadLen > 0)
            parts << QString("payload=%1 bytes").arg(payloadLen);
         break;
   }

   return parts.join(" | ");
}

static QString parseDesfireRequestParams(const unsigned int command, const QByteArray &payload)
{
   QStringList parts;

   switch (command)
   {
      case INS_CONTINUE:
         if (!payload.isEmpty())
            parts << QString("chunk=%1 bytes").arg(payload.size());
         return parts.join(" | ");

      case INS_AUTHENTICATE_LEGACY:
      case INS_AUTHENTICATE_ISO:
      case INS_AUTHENTICATE_AES:
         if (payload.size() == 1)
            parts << QString("keyNo=%1").arg(hexValue(byteAt(payload, 0), 2));
         else if (!payload.isEmpty())
            parts << QString("cryptogram=%1 bytes").arg(payload.size()) << "encrypted=yes";
         break;

      case INS_CHANGE_KEY_SETTINGS:
         parts << QString("cryptogram=%1 bytes").arg(payload.size()) << "encrypted=yes";
         break;

      case INS_GET_KEY_SETTINGS:
         break;

      case INS_CHANGE_KEY:
         if (payload.size() < 1)
            return "invalid payload";
         parts << QString("keyId=%1").arg(hexValue(byteAt(payload, 0), 2))
            << QString("keyNo=%1").arg(byteAt(payload, 0) & 0x3F)
            << QString("keyType=%1").arg(keyTypeName(byteAt(payload, 0) >> 6));
         if (payload.size() > 1)
            parts << QString("cryptogram=%1 bytes").arg(payload.size() - 1) << "encrypted=yes";
         break;

      case INS_GET_KEY_VERSION:
         if (payload.size() < 1)
            return "invalid payload";
         parts << QString("keyNo=%1").arg(hexValue(byteAt(payload, 0), 2));
         break;

      case INS_SET_CONFIGURATION:
         if (payload.size() < 1)
            return "invalid payload";
         parts << QString("option=%1").arg(hexValue(byteAt(payload, 0), 2));
         switch (byteAt(payload, 0))
         {
            case 0x00: parts << "config-byte";
               break;
            case 0x01: parts << "default-key";
               break;
            case 0x02: parts << "custom-ATS";
               break;
            default: parts << "unknown-option";
               break;
         }
         if (payload.size() > 1)
            parts << QString("cryptogram=%1 bytes").arg(payload.size() - 1) << "encrypted=yes";
         break;

      case INS_GET_CARD_UID:
      case INS_LIST_APPLICATIONS:
      case INS_LIST_DF_NAMES:
      case INS_GET_FREE_MEMORY:
      case INS_GET_VERSION:
      case INS_LIST_FILES:
      case INS_GET_ISO_FILE_IDS:
      case INS_FORMAT_PICC:
      case INS_COMMIT_TRANSACTION:
      case INS_ABORT_TRANSACTION:
         break;

      case INS_CREATE_APPLICATION:
      {
         if (payload.size() < 5)
            return "invalid payload";

         const unsigned int aid = readLe(payload, 0, 3);
         const unsigned int keySettings1 = byteAt(payload, 3);
         const unsigned int keySettings2 = byteAt(payload, 4);
         const bool isoEnabled = (keySettings2 & 0x20) != 0;

         parts << QString("AID=%1").arg(hexValue(aid, 6))
            << QString("KS1=%1").arg(hexValue(keySettings1, 2))
            << QString("crypto=%1").arg(cryptoModeName(keySettings2 >> 6))
            << QString("maxKeys=%1").arg(keySettings2 & 0x0F)
            << QString("ISO=%1").arg(isoEnabled ? "on" : "off");

         if (isoEnabled && payload.size() >= 7)
         {
            parts << QString("isoId=%1").arg(hexValue(readLe(payload, 5, 2), 4));

            if (payload.size() > 7)
               parts << QString("isoDf=%1").arg(DesfireParser::toHex(payload.mid(7)));
         }

         break;
      }

      case INS_DELETE_APPLICATION:
      case INS_SELECT_APPLICATION:
         if (payload.size() < 3)
            return "invalid payload";
         parts << QString("AID=%1").arg(hexValue(readLe(payload, 0, 3), 6));
         break;

      case INS_GET_FILE_SETTINGS:
      case INS_DELETE_FILE:
      case INS_CLEAR_RECORD_FILE:
      case INS_GET_VALUE:
         if (payload.size() < 1)
            return "invalid payload";
         parts << QString("fileId=%1").arg(hexValue(byteAt(payload, 0), 2));
         break;

      case INS_CHANGE_FILE_SETTINGS:
         if (payload.size() < 1)
            return "invalid payload";
         parts << QString("fileId=%1").arg(hexValue(byteAt(payload, 0), 2));
         if (payload.size() > 1)
            parts << QString("cryptogram=%1 bytes").arg(payload.size() - 1) << "encrypted=yes";
         break;

      case INS_CREATE_STD_FILE:
      case INS_CREATE_BACKUP_FILE:
      {
         if (payload.size() != 7 && payload.size() != 9)
            return "invalid payload";

         int offset = 0;
         const unsigned int fileId = byteAt(payload, offset++);
         unsigned int isoId = 0;

         if (payload.size() == 9)
         {
            isoId = readLe(payload, offset, 2);
            offset += 2;
         }

         const unsigned int commSettings = byteAt(payload, offset++);
         const unsigned int accessRights = readLe(payload, offset, 2);
         offset += 2;
         const unsigned int fileSize = readLe(payload, offset, 3);

         parts << QString("fileType=%1").arg(command == INS_CREATE_STD_FILE ? "StdData" : "BackupData")
            << QString("fileId=%1").arg(hexValue(fileId, 2));

         if (payload.size() == 9)
            parts << QString("isoId=%1").arg(hexValue(isoId, 4));

         parts << QString("comm=%1").arg(communicationModeName(commSettings))
            << accessRightsSummary(accessRights)
            << QString("size=%1 bytes").arg(fileSize);
         break;
      }

      case INS_CREATE_VALUE_FILE:
         if (payload.size() < 17)
            return "invalid payload";
         parts << QString("fileId=%1").arg(hexValue(byteAt(payload, 0), 2))
            << QString("comm=%1").arg(communicationModeName(byteAt(payload, 1)))
            << accessRightsSummary(readLe(payload, 2, 2))
            << QString("lower=%1").arg(static_cast<int>(readLe(payload, 4, 4)))
            << QString("upper=%1").arg(static_cast<int>(readLe(payload, 8, 4)))
            << QString("initial=%1").arg(static_cast<int>(readLe(payload, 12, 4)))
            << QString("limitedCredit=%1").arg(byteAt(payload, 16) ? "on" : "off");
         break;

      case INS_CREATE_LINEAR_FILE:
      case INS_CREATE_CYCLIC_FILE:
      {
         if (payload.size() != 10 && payload.size() != 12)
            return "invalid payload";

         int offset = 0;
         const unsigned int fileId = byteAt(payload, offset++);
         unsigned int isoId = 0;

         if (payload.size() == 12)
         {
            isoId = readLe(payload, offset, 2);
            offset += 2;
         }

         const unsigned int commSettings = byteAt(payload, offset++);
         const unsigned int accessRights = readLe(payload, offset, 2);
         offset += 2;
         const unsigned int recordSize = readLe(payload, offset, 3);
         offset += 3;
         const unsigned int recordLimit = readLe(payload, offset, 3);

         parts << QString("fileType=%1").arg(command == INS_CREATE_LINEAR_FILE ? "LinearRecord" : "CyclicRecord")
            << QString("fileId=%1").arg(hexValue(fileId, 2));

         if (payload.size() == 12)
            parts << QString("isoId=%1").arg(hexValue(isoId, 4));

         parts << QString("comm=%1").arg(communicationModeName(commSettings))
            << accessRightsSummary(accessRights)
            << QString("recordSize=%1 bytes").arg(recordSize)
            << QString("maxRecords=%1").arg(recordLimit);
         break;
      }

      case INS_READ_DATA:
      case INS_READ_RECORDS:
         if (payload.size() < 7)
            return "invalid payload";
         parts << QString("fileId=%1").arg(hexValue(byteAt(payload, 0), 2))
            << QString("offset=%1").arg(readLe(payload, 1, 3))
            << QString("length=%1").arg(readLe(payload, 4, 3));
         break;

      case INS_WRITE_DATA:
      case INS_WRITE_RECORD:
      {
         if (payload.size() < 7)
            return "invalid payload";

         const unsigned int length = readLe(payload, 4, 3);
         const int chunk = std::max(0, static_cast<int>(payload.size()) - 7);

         parts << QString("fileId=%1").arg(hexValue(byteAt(payload, 0), 2))
            << QString("offset=%1").arg(readLe(payload, 1, 3))
            << QString("length=%1").arg(length)
            << QString("file-data tx=%1 bytes").arg(chunk)
            << "encrypted=depends file comm";
         break;
      }

      case INS_CREDIT:
      case INS_DEBIT:
      case INS_LIMITED_CREDIT:
         if (payload.size() < 1)
            return "invalid payload";
         parts << QString("fileId=%1").arg(hexValue(byteAt(payload, 0), 2));
         if (payload.size() > 1)
            parts << QString("value-field=%1 bytes").arg(payload.size() - 1) << "encrypted=depends file comm";
         break;

      default:
         if (!payload.isEmpty())
            parts << QString("payload=%1 bytes").arg(payload.size());
         break;
   }

   return parts.join(" | ");
}

static bool isEncryptedTransportCommand(const unsigned int command)
{
   switch (command)
   {
      case INS_AUTHENTICATE_LEGACY:
      case INS_AUTHENTICATE_ISO:
      case INS_AUTHENTICATE_AES:
      case INS_ISO_EXTERNAL_AUTHENTICATE:
      case INS_ISO_INTERNAL_AUTHENTICATE:
      case INS_CHANGE_KEY:
         return true;
      default:
         return false;
   }
}

static bool hasFileDataInResponse(const unsigned int command)
{
   switch (command)
   {
      case INS_READ_DATA:
      case INS_READ_RECORDS:
      case INS_ISO_READ_BINARY:
      case INS_ISO_READ_RECORDS:
         return true;
      default:
         return false;
   }
}

static QString parseIsoResponseParams(const unsigned int command, const QByteArray &payload)
{
   if (payload.isEmpty())
      return {};

   QStringList parts;

   switch (command)
   {
      case INS_ISO_GET_CHALLENGE:
         parts << QString("challenge=%1 bytes").arg(payload.size());
         break;

      case INS_ISO_SELECT:
         parts << QString("fci=%1 bytes").arg(payload.size());
         break;

      case INS_ISO_READ_BINARY:
      case INS_ISO_READ_RECORDS:
         parts << QString("file-data rx=%1 bytes").arg(payload.size());
         break;

      default:
         parts << QString("payload=%1 bytes").arg(payload.size());
         break;
   }

   return parts.join(" | ");
}

static QString parseDesfireResponseParams(const unsigned int command, const QByteArray &payload, const unsigned int status)
{
   if (payload.isEmpty())
      return {};

   QStringList parts;

   if (status == STATUS_ADDITIONAL_FRAME)
      parts << QString("additional-frame=%1 bytes").arg(payload.size());

   switch (command)
   {
      case INS_GET_KEY_SETTINGS:
         if (payload.size() >= 2)
         {
            const unsigned int keySettings1 = byteAt(payload, 0);
            const unsigned int keySettings2 = byteAt(payload, 1);
            parts << QString("KS1=%1").arg(hexValue(keySettings1, 2))
               << QString("crypto=%1").arg(cryptoModeName(keySettings2 >> 6))
               << QString("maxKeys=%1").arg(keySettings2 & 0x0F)
               << QString("ISO=%1").arg((keySettings2 & 0x20) ? "on" : "off");
         }
         break;

      case INS_GET_KEY_VERSION:
         parts << QString("keyVersion=%1").arg(hexValue(byteAt(payload, 0), 2));
         break;

      case INS_LIST_APPLICATIONS:
         if (payload.size() >= 3)
         {
            QStringList aids;
            for (int i = 0; i + 2 < payload.size(); i += 3)
               aids << hexValue(readLe(payload, i, 3), 6);

            parts << QString("AIDs=%1").arg(aids.join(", "));
         }
         break;

      case INS_LIST_FILES:
         if (!payload.isEmpty())
         {
            QStringList fileIds;
            for (int i = 0; i < payload.size(); ++i)
               fileIds << hexValue(byteAt(payload, i), 2);

            parts << QString("fileIds=%1").arg(fileIds.join(", "));
         }
         break;

      case INS_GET_ISO_FILE_IDS:
         if (payload.size() >= 2)
         {
            QStringList isoIds;
            for (int i = 0; i + 1 < payload.size(); i += 2)
               isoIds << hexValue(readLe(payload, i, 2), 4);

            parts << QString("isoIds=%1").arg(isoIds.join(", "));
         }
         break;

      case INS_GET_CARD_UID:
         parts << QString("uid=%1").arg(DesfireParser::toHex(payload));
         break;

      case INS_GET_FREE_MEMORY:
         if (payload.size() >= 3)
            parts << QString("free=%1 bytes").arg(readLe(payload, 0, 3));
         break;

      case INS_GET_VALUE:
         if (payload.size() >= 4)
            parts << QString("value=%1").arg(static_cast<int>(readLe(payload, 0, 4)));
         break;

      case INS_GET_FILE_SETTINGS:
         if (payload.size() >= 4)
         {
            const unsigned int fileType = byteAt(payload, 0);
            const unsigned int commSettings = byteAt(payload, 1);
            const unsigned int accessRights = readLe(payload, 2, 2);
            parts << QString("fileType=%1").arg(hexValue(fileType, 2))
               << QString("comm=%1").arg(communicationModeName(commSettings))
               << accessRightsSummary(accessRights);
         }
         break;

      case INS_READ_DATA:
      case INS_READ_RECORDS:
      case INS_ISO_READ_BINARY:
      case INS_ISO_READ_RECORDS:
         parts << QString("file-data rx=%1 bytes").arg(payload.size());
         break;

      default:
         break;
   }

   if (parts.isEmpty())
      parts << QString("payload=%1 bytes").arg(payload.size());

   return parts.join(" | ");
}

DesfireParser::RequestInfo DesfireParser::parseRequest(const QByteArray &apdu)
{
   RequestInfo info;

   if (apdu.isEmpty())
      return info;

   const auto cla = static_cast<unsigned char>(apdu[0]);

   if (cla == CLA_WRAPPED)
   {
      if (apdu.size() < 5)
         return info;

      const int lc = static_cast<int>(byteAt(apdu, 4));
      const int available = std::max(0, static_cast<int>(apdu.size()) - 5);
      const int payloadLen = std::min(lc, available);
      const QByteArray payload = apdu.mid(5, payloadLen);

      info.valid = true;
      info.kind = RequestKind::Wrapped;
      info.command = static_cast<unsigned char>(apdu[1]);
      info.commandName = QString("%1").arg(commandName(info.command, info.kind));
      info.summary = QString("Wrapped DESFire");
      info.params = parseDesfireRequestParams(info.command, payload);

      return info;
   }

   if (cla == CLA_ISO && apdu.size() >= 2)
   {
      info.valid = true;
      info.kind = RequestKind::Iso;
      info.command = static_cast<unsigned char>(apdu[1]);
      info.commandName = QString("ISO %1").arg(commandName(info.command, info.kind));
      info.summary = QString("ISO-7816");
      info.params = parseIsoRequestParams(info.command, apdu);
      
      return info;
   }

   info.valid = true;
   info.kind = RequestKind::Native;
   info.command = static_cast<unsigned char>(apdu[0]);
   info.commandName = commandName(info.command, info.kind);
   info.summary = QString("Native DESFire");
   info.params = parseDesfireRequestParams(info.command, apdu.mid(1));

   return info;
}

DesfireParser::ResponseInfo DesfireParser::parseResponse(const QByteArray &apdu, const RequestInfo &request)
{
   ResponseInfo info;

   if (apdu.isEmpty())
      return info;

   info.valid = true;

   if (request.kind == RequestKind::Iso)
   {
      if (apdu.size() < 2)
      {
         info.status = "<invalid>";
         info.summary = "missing SW";
         return info;
      }

      const unsigned int sw = (static_cast<unsigned char>(apdu[apdu.size() - 2]) << 8) | static_cast<unsigned char>(apdu[apdu.size() - 1]);

      info.status = QString("0x%1").arg(sw, 4, 16, QChar('0'));
      info.summary = isoStatusName(sw);

      const int payloadLen = qMax(0, static_cast<int>(apdu.size()) - 2);
      const QByteArray payload = apdu.left(payloadLen);

      if (hasFileDataInResponse(request.command))
         info.summary = QString("%1 | file-data rx=%2 bytes").arg(info.summary).arg(payloadLen);
      else if (payloadLen > 0)
         info.summary = QString("%1 | payload=%2 bytes").arg(info.summary).arg(payloadLen);

      if (isEncryptedTransportCommand(request.command))
         info.summary = QString("%1 | encrypted=yes").arg(info.summary);
      else if (hasFileDataInResponse(request.command))
         info.summary = QString("%1 | encrypted=depends file comm").arg(info.summary);

      info.ok = (sw == ISO_STATUS_OK);
      info.params = parseIsoResponseParams(request.command, payload);

      return info;
   }

   if ((request.kind == RequestKind::Wrapped || request.kind == RequestKind::None) && apdu.size() >= 2 && static_cast<unsigned char>(apdu[apdu.size() - 2]) == SW1_WRAPPED)
   {
      const unsigned int sw = (static_cast<unsigned char>(apdu[apdu.size() - 2]) << 8) | static_cast<unsigned char>(apdu[apdu.size() - 1]);
      const unsigned int st = static_cast<unsigned char>(apdu[apdu.size() - 1]);

      info.status = QString("0x%1").arg(sw, 4, 16, QChar('0'));
      info.summary = nativeStatusName(st);

      const int payloadLen = qMax(0, static_cast<int>(apdu.size()) - 2);
      const QByteArray payload = apdu.left(payloadLen);

      if (hasFileDataInResponse(request.command))
         info.summary = QString("%1 | file-data rx=%2 bytes").arg(info.summary).arg(payloadLen);
      else if (payloadLen > 0)
         info.summary = QString("%1 | payload=%2 bytes").arg(info.summary).arg(payloadLen);

      if (isEncryptedTransportCommand(request.command))
         info.summary = QString("%1 | encrypted=yes").arg(info.summary);
      else if (hasFileDataInResponse(request.command))
         info.summary = QString("%1 | encrypted=depends file comm").arg(info.summary);

      info.ok = (st == STATUS_OK || st == STATUS_NO_CHANGES || st == STATUS_ADDITIONAL_FRAME);
      info.params = parseDesfireResponseParams(request.command, payload, st);

      return info;
   }

   const unsigned int st = static_cast<unsigned char>(apdu[0]);
   info.status = QString("0x%1").arg(st, 2, 16, QChar('0'));
   info.summary = nativeStatusName(st);

   const int payloadLen = qMax(0, static_cast<int>(apdu.size()) - 1);
   const QByteArray payload = apdu.mid(1, payloadLen);

   if (hasFileDataInResponse(request.command))
      info.summary = QString("%1 | file-data rx=%2 bytes").arg(info.summary).arg(payloadLen);
   else if (payloadLen > 0)
      info.summary = QString("%1 | payload=%2 bytes").arg(info.summary).arg(payloadLen);

   if (isEncryptedTransportCommand(request.command))
      info.summary = QString("%1 | encrypted=yes").arg(info.summary);
   else if (hasFileDataInResponse(request.command))
      info.summary = QString("%1 | encrypted=depends file comm").arg(info.summary);

   info.ok = (st == STATUS_OK || st == STATUS_NO_CHANGES || st == STATUS_ADDITIONAL_FRAME);
   info.params = parseDesfireResponseParams(request.command, payload, st);

   return info;
}

QString DesfireParser::toHex(const QByteArray &data)
{
   QString text;
   text.reserve(data.size() * 3);

   for (const auto ch: data)
      text.append(QString("%1").arg(static_cast<unsigned char>(ch), 2, 16, QLatin1Char('0')));

   return text.trimmed();
}

QString DesfireParser::commandName(const unsigned int command, const RequestKind kind)
{
   static const QMap<unsigned int, QString> desfireCommands = {
      {INS_AUTHENTICATE_LEGACY, "Authenticate"},
      {INS_AUTHENTICATE_ISO, "AuthenticateISO"},
      {INS_AUTHENTICATE_AES, "AuthenticateAES"},
      {INS_CHANGE_KEY_SETTINGS, "ChangeKeySettings"},
      {INS_GET_KEY_SETTINGS, "GetKeySettings"},
      {INS_CHANGE_KEY, "ChangeKey"},
      {INS_GET_KEY_VERSION, "GetKeyVersion"},
      {INS_SET_CONFIGURATION, "SetConfiguration"},
      {INS_GET_CARD_UID, "GetCardUID"},
      {INS_CREATE_APPLICATION, "CreateApplication"},
      {INS_DELETE_APPLICATION, "DeleteApplication"},
      {INS_LIST_APPLICATIONS, "GetApplicationIDs"},
      {INS_LIST_DF_NAMES, "GetDFNames"},
      {INS_GET_FREE_MEMORY, "GetFreeMemory"},
      {INS_SELECT_APPLICATION, "SelectApplication"},
      {INS_FORMAT_PICC, "FormatPICC"},
      {INS_GET_VERSION, "GetVersion"},
      {INS_LIST_FILES, "GetFileIDs"},
      {INS_GET_ISO_FILE_IDS, "GetISOFileIDs"},
      {INS_GET_FILE_SETTINGS, "GetFileSettings"},
      {INS_CHANGE_FILE_SETTINGS, "ChangeFileSettings"},
      {INS_CREATE_STD_FILE, "CreateStdDataFile"},
      {INS_CREATE_BACKUP_FILE, "CreateBackupDataFile"},
      {INS_CREATE_VALUE_FILE, "CreateValueFile"},
      {INS_CREATE_LINEAR_FILE, "CreateLinearRecordFile"},
      {INS_CREATE_CYCLIC_FILE, "CreateCyclicRecordFile"},
      {INS_DELETE_FILE, "DeleteFile"},
      {INS_READ_DATA, "ReadData"},
      {INS_WRITE_DATA, "WriteData"},
      {INS_GET_VALUE, "GetValue"},
      {INS_CREDIT, "Credit"},
      {INS_DEBIT, "Debit"},
      {INS_LIMITED_CREDIT, "LimitedCredit"},
      {INS_WRITE_RECORD, "WriteRecord"},
      {INS_READ_RECORDS, "ReadRecords"},
      {INS_CLEAR_RECORD_FILE, "ClearRecordFile"},
      {INS_CONTINUE, "AdditionalFrame"},
      {INS_COMMIT_TRANSACTION, "CommitTransaction"},
      {INS_ABORT_TRANSACTION, "AbortTransaction"},
   };

   static const QMap<unsigned int, QString> isoCommands = {
      {INS_ISO_SELECT, "SELECT FILE"},
      {INS_ISO_READ_BINARY, "READ BINARY"},
      {INS_ISO_UPDATE_BINARY, "UPDATE BINARY"},
      {INS_ISO_READ_RECORDS, "READ RECORDS"},
      {INS_ISO_UPDATE_RECORD, "UPDATE RECORD"},
      {INS_ISO_APPEND_RECORD, "APPEND RECORD"},
      {INS_ISO_GET_CHALLENGE, "GET CHALLENGE"},
      {INS_ISO_EXTERNAL_AUTHENTICATE, "EXTERNAL AUTHENTICATE"},
      {INS_ISO_INTERNAL_AUTHENTICATE, "INTERNAL AUTHENTICATE"},
   };

   if (const QString name = (kind == RequestKind::Iso) ? isoCommands.value(command) : desfireCommands.value(command); !name.isEmpty())
      return name;

   return QString("Unknown 0x%1").arg(command, 2, 16, QChar('0'));
}

QString DesfireParser::nativeStatusName(const unsigned int status)
{
   static const QMap<unsigned int, QString> statusNames = {
      {0x00, "OK"},
      {0x0C, "NO_CHANGES"},
      {0x0E, "OUT_OF_EEPROM"},
      {0x1C, "ILLEGAL_COMMAND"},
      {0x1E, "INTEGRITY_ERROR"},
      {0x40, "NO_SUCH_KEY"},
      {0x7E, "LENGTH_ERROR"},
      {0x9D, "PERMISSION_DENIED"},
      {0x9E, "PARAMETER_ERROR"},
      {0xA0, "APPLICATION_NOT_FOUND"},
      {0xA1, "APPL_INTEGRITY_ERROR"},
      {0xAE, "AUTHENTICATION_ERROR"},
      {0xAF, "ADDITIONAL_FRAME"},
      {0xBE, "BOUNDARY_ERROR"},
      {0xC1, "PICC_INTEGRITY_ERROR"},
      {0xCA, "COMMAND_ABORTED"},
      {0xCD, "PICC_DISABLED"},
      {0xCE, "COUNT_ERROR"},
      {0xDE, "DUPLICATE_ERROR"},
      {0xEE, "EEPROM_ERROR"},
      {0xF0, "FILE_NOT_FOUND"},
      {0xF1, "FILE_INTEGRITY_ERROR"},
   };

   return statusNames.value(status, "UNKNOWN_STATUS");
}

QString DesfireParser::isoStatusName(const unsigned int status)
{
   static const QMap<unsigned int, QString> statusNames = {
      {0x9000, "OK"},
      {0x6985, "NOT_ENOUGH_DATA"},
      {0x6700, "WRONG_LENGTH"},
      {0x6982, "ACCESS_NOT_ALLOWED"},
      {0x6A82, "FILE_NOT_FOUND"},
      {0x6A86, "WRONG_PARAMETERS_P1P2"},
      {0x6A87, "WRONG_PARAMETERS_LC"},
      {0x6C00, "WRONG_PARAMETERS_LE"},
      {0x6D00, "INS_NOT_SUPPORTED"},
      {0x6E00, "CLA_NOT_SUPPORTED"},
      {0x6F00, "NO_DIAGNOSTIC"},
   };

   return statusNames.value(status, "UNKNOWN_STATUS");
}
