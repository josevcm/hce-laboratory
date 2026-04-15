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

#include <chrono>

#include <nlohmann/json.hpp>

#include <rt/Logger.h>
#include <rt/Format.h>

#include <hce/crypto/Cipher.h>

#include <hce/targets/Desfire.h>

#include "Instance.h"

// --- Security Related Commands ---
#include "DesfireAuthenticate.h"
#include "DesfireAuthenticateAES.h"
#include "DesfireAuthenticateISO.h"
#include "DesfireChangeKey.h"
#include "DesfireChangeKeySettings.h"
#include "DesfireGetKeySettings.h"
#include "DesfireGetKeyVersion.h"

// --- PICC Level Commands ---
#include "DesfireCreateApplication.h"
#include "DesfireDeleteApplication.h"
#include "DesfireFormatPICC.h"
#include "DesfireGetFreeMemory.h"
#include "DesfireGetVersion.h"
#include "DesfireListApplications.h"
#include "DesfireListDFNames.h"
#include "DesfireSelectApplication.h"

// --- Application Level Commands ---
#include "DesfireChangeFileSettings.h"
#include "DesfireCreateDataFile.h"
#include "DesfireCreateRecordFile.h"
#include "DesfireCreateValueFile.h"
#include "DesfireDeleteFile.h"
#include "DesfireGetFileSettings.h"
#include "DesfireListFiles.h"

// --- Data Manipulation Commands ---
#include "DesfireAbortTransaction.h"
#include "DesfireClearRecordFile.h"
#include "DesfireCommitTransaction.h"
#include "DesfireCredit.h"
#include "DesfireDebit.h"
#include "DesfireGetValue.h"
#include "DesfireLimitedCredit.h"
#include "DesfireReadData.h"
#include "DesfireReadRecords.h"
#include "DesfireWriteData.h"
#include "DesfireWriteRecord.h"

// --- ISO7816-4 Commands ---
#include "DesfireIsoSelectFile.h"
#include "DesfireIsoReadBinary.h"
#include "DesfireIsoUpdateBinary.h"
#include "DesfireIsoReadRecords.h"
#include "DesfireIsoUpdateRecord.h"
#include "DesfireIsoAppendRecord.h"
#include "DesfireIsoAuthentication.h"

using json = nlohmann::json;

namespace hce::targets {

// using CommandFn = std::function<int(rt::ByteBuffer &, rt::ByteBuffer &)>;

enum ProtocolMode
{
   UnknowProtocol = 0x00,
   NativeProtocol = 0x01,
   WrappedProtocol = 0x02
};

struct Desfire::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hce.targets.desfire.Desfire");

   // --- Security Related Commands ---
   DesfireAuthenticate authenticate;
   DesfireAuthenticateISO authenticateISO;
   DesfireAuthenticateAES authenticateAES;
   DesfireChangeKeySettings changeKeySettings;
   DesfireGetKeySettings getKeySettings;
   DesfireChangeKey changeKey;
   DesfireGetKeyVersion getKeyVersion;

   // --- PICC Level Commands ---
   DesfireCreateApplication createApplication;
   DesfireDeleteApplication deleteApplication;
   DesfireSelectApplication selectApplication;
   DesfireListApplications listApplications;
   DesfireListDFNames listDFNames;
   DesfireGetFreeMemory getFreeMemory;
   DesfireGetVersion getVersion;
   DesfireFormatPICC formatPICC;

   // --- Application Level Commands ---
   DesfireCreateDataFile createStandardFile;
   DesfireCreateDataFile createBackupFile;
   DesfireCreateValueFile createValueFile;
   DesfireCreateRecordFile createLinearRecordFile;
   DesfireCreateRecordFile createCyclicRecordFile;
   DesfireChangeFileSettings changeFileSettings;
   DesfireListFiles listFiles;
   DesfireGetFileSettings getFileSettings;
   DesfireDeleteFile deleteFile;

   // --- Data Manipulation Commands ---
   DesfireReadData readData;
   DesfireWriteData writeData;
   DesfireCredit credit;
   DesfireDebit debit;
   DesfireGetValue getValue;
   DesfireLimitedCredit limitedCredit;
   DesfireWriteRecord writeRecord;
   DesfireReadRecords readRecords;
   DesfireClearRecordFile clearRecordFile;
   DesfireCommitTransaction commitTransaction;
   DesfireAbortTransaction abortTransaction;

   // --- ISO7816-4 Commands ---
   DesfireIsoSelectFile isoSelectFile;
   DesfireIsoReadBinary isoReadBinary;
   DesfireIsoUpdateBinary isoUpdateBinary;
   DesfireIsoReadRecords isoReadRecords;
   DesfireIsoUpdateRecord isoUpdateRecord;
   DesfireIsoAppendRecord isoAppendRecord;
   DesfireIsoAuthentication isoAuthentication;

   // card bundle
   Instance instance;

   // target parameters
   unsigned short targetATQA = 0x4403;
   unsigned char targetSAK = 0x20;
   unsigned char targetTB1 = 0x81;
   unsigned char targetTC1 = 0x02;
   rt::ByteBuffer targetHB = {0x80};
   rt::ByteBuffer targetUID = rt::ByteBuffer::random(7);

   explicit Impl() :

      // --- Security Related Commands ---
      authenticate(instance),
      authenticateISO(instance),
      authenticateAES(instance),
      changeKeySettings(instance),
      getKeySettings(instance),
      changeKey(instance),
      getKeyVersion(instance),

      // --- PICC Level Commands ---
      createApplication(instance),
      deleteApplication(instance),
      selectApplication(instance),
      listApplications(instance),
      listDFNames(instance),
      getFreeMemory(instance),
      getVersion(instance),
      formatPICC(instance),

      // --- Application Level Commands ---
      createStandardFile(instance, StandardFile),
      createBackupFile(instance, BackupFile),
      createValueFile(instance),
      createLinearRecordFile(instance, LinearRecordFile),
      createCyclicRecordFile(instance, CyclicRecordFile),
      changeFileSettings(instance),
      listFiles(instance),
      getFileSettings(instance),
      deleteFile(instance),

      // --- Data Manipulation Commands ---
      readData(instance),
      writeData(instance),
      credit(instance),
      debit(instance),
      getValue(instance),
      limitedCredit(instance),
      writeRecord(instance),
      readRecords(instance),
      clearRecordFile(instance),
      commitTransaction(instance),
      abortTransaction(instance),

      // --- ISO7816-4 Commands ---
      isoSelectFile(instance),
      isoReadBinary(instance),
      isoUpdateBinary(instance),
      isoReadRecords(instance),
      isoUpdateRecord(instance),
      isoAppendRecord(instance),
      isoAuthentication(instance)
   {
      instance.uid = targetUID;
   }

   explicit Impl(const DesfireSize size) : Impl()
   {
      instance.softwareStorage = size;
      instance.hardwareStorage = size;
   }

   explicit Impl(const std::string &tag) : Impl()
   {
      load(tag);
   }

   rt::Variant get(int id)
   {
      switch (id)
      {
         case PARAM_ATQA:
            return targetATQA;

         case PARAM_SAK:
            return targetSAK;

         case PARAM_UID:
            return targetUID;

         case PARAM_RATS_TB1:
            return targetTB1;

         case PARAM_RATS_TC1:
            return targetTC1;

         case PARAM_RATS_HB:
            return targetHB;

         default:
            return {};
      }
   }

   bool set(int id, const rt::Variant &value)
   {
      switch (id)
      {
         case PARAM_ATQA:
         {
            if (const auto v = std::get_if<unsigned short>(&value))
            {
               targetATQA = *v;
               return true;
            }

            log->error("invalid value type for PARAM_ATQA");
            return false;
         }
         case PARAM_SAK:
         {
            if (const auto v = std::get_if<unsigned char>(&value))
            {
               targetSAK = *v;
               return true;
            }

            log->error("invalid value type for PARAM_SAK");
            return false;
         }
         case PARAM_UID:
         {
            if (const auto v = std::get_if<rt::ByteBuffer>(&value))
            {
               instance.uid = targetUID = rt::ByteBuffer(v->ptr(), v->size());
               return true;
            }

            log->error("invalid value type for PARAM_UID");
            return false;
         }
         case PARAM_RATS_TB1:
         {
            if (const auto v = std::get_if<unsigned char>(&value))
            {
               targetTB1 = *v;
               return true;
            }

            log->error("invalid value type for PARAM_RATS_TB1");
            return false;
         }
         case PARAM_RATS_TC1:
         {
            if (const auto v = std::get_if<unsigned char>(&value))
            {
               targetTC1 = *v;
               return true;
            }

            log->error("invalid value type for PARAM_RATS_TC1");
            return false;
         }
         case PARAM_RATS_HB:
         {
            if (const auto v = std::get_if<rt::ByteBuffer>(&value))
            {
               targetHB = rt::ByteBuffer(v->ptr(), v->size());
               return true;
            }

            log->error("invalid value type for PARAM_RATS_HIST");
            return false;
         }
         default:
            log->warn("unknown or unsupported configuration id {}", {id});
            return false;
      }
   }

   void select()
   {
      // initialize command processor
      instance.protocol = 0;
      instance.command = 0;
      instance.chaining = 0;

      // invalidate application and auth
      instance.invalidateAuth();

      // select master by default
      instance.selectApplication(DESFIRE_MASTER_APP_ID);
   }

   void deselect()
   {
   }

   int load(const std::string &raw)
   {
      std::vector<Application> loaded;

      const json target = json::parse(raw);

      if (auto required = require(target, {"type", "version"}); !required.empty())
         return error(-1, "invalid target, missing attributes: {}", {required});

      if (target["type"] != "desfire")
         return error(-1, "invalid target type, must be: 'desfire'");

      if (target["version"] != 1)
         return error(-1, "invalid target version, must be: 1");

      // load discovery parameters
      if (target.contains("discovery"))
      {
         auto discovery = target["discovery"];

         if (discovery.contains("ATQA"))
            targetATQA = discovery["ATQA"];

         if (discovery.contains("SAK"))
            targetSAK = discovery["SAK"];

         if (discovery.contains("UID"))
            instance.uid = targetUID = rt::ByteBuffer::fromHex(discovery["UID"]);

         if (discovery.contains("ATS"))
         {
            auto ats = discovery["ATS"];

            if (ats.contains("TB1"))
               targetTB1 = ats["TB1"];

            if (ats.contains("TC1"))
               targetTC1 = ats["TC1"];

            if (ats.contains("HB"))
            {
               if (auto hb = static_cast<std::string>(ats["HB"]); !rt::Format::isHex(hb))
                  return error(-1, "invalid ATS field HB '{}', must be hex string", {hb});

               targetHB = rt::ByteBuffer::fromHex(ats["HB"]);
            }
         }
      }

      // load desfire payload data
      if (target.contains("payload"))
      {
         auto payload = target["payload"];

         if (payload.contains("info"))
         {
            auto info = payload["info"];

            if (auto required = require(info, {"hw", "sw", "tr"}); !required.empty())
               return error(-1, "invalid desfire payload info, missing attributes: {}", {required});

            auto hw = info["hw"];
            auto sw = info["sw"];
            auto tr = info["tr"];

            if (auto required = require(hw, {"vendor", "type", "subtype", "version", "storage", "protocol"}); !required.empty())
               return error(-1, "invalid desfire payload hw info, missing attributes: {}", {required});

            if (auto required = require(sw, {"vendor", "type", "subtype", "version", "storage", "protocol"}); !required.empty())
               return error(-1, "invalid desfire payload sw info, missing attributes: {}", {required});

            if (auto required = require(tr, {"year", "week", "batch"}); !required.empty())
               return error(-1, "invalid desfire payload tr info, missing attributes: {}", {required});

            // load HW info
            instance.hardwareVendor = hw["vendor"];
            instance.hardwareType = hw["type"];
            instance.hardwareSubtype = hw["subtype"];
            instance.hardwareVersion = hw["version"];
            instance.hardwareStorage = hw["storage"];
            instance.hardwareProtocol = hw["protocol"];

            // load SW info
            instance.softwareVendor = sw["vendor"];
            instance.softwareType = sw["type"];
            instance.softwareSubtype = sw["subtype"];
            instance.softwareVersion = sw["version"];
            instance.softwareStorage = sw["storage"];
            instance.softwareProtocol = sw["protocol"];

            // load TR info
            instance.productionYear = tr["year"];
            instance.productionWeek = tr["week"];
            instance.batchNumber = tr["batch"];
         }

         // load desfire applications
         if (payload.contains("directory"))
         {
            auto directory = payload["directory"];

            if (!directory.is_array())
               return error(-1, "invalid application, directory must be an array");

            for (const auto &app: directory)
            {
               if (auto required = require(app, {"aid", "keySettings1", "keySettings2"}); !required.empty())
                  return error(-1, "invalid application, missing attributes: {}", {required});

               unsigned int aid = app["aid"];
               unsigned int keySettings1 = app["keySettings1"];
               unsigned int keySettings2 = app["keySettings2"];

               LOG_INFO(log, "load application: 0x{06x}", {aid});

               Application appEntry {
                  .aid = aid,
                  .cryptoMode = keySettings2 >> 6,
                  .keySettings = keySettings1,
                  .maximumKeys = keySettings2 & 0x0F,
                  .isoEnabled = (keySettings2 & 0x20) != 0
               };

               if (appEntry.isoEnabled)
               {
                  if (auto required = require(app, {"isoId"}); !required.empty())
                     return error(-1, "invalid application 0x{06x}, missing attributes: {}", {aid, required});

                  appEntry.isoId = app["isoId"];

                  if (app.contains("isoName"))
                  {
                     if (auto appIsoName = static_cast<std::string>(app["isoName"]); !rt::Format::isHex(appIsoName))
                        return error(-1, "invalid application 0x{06x}, isoName '{}' must be hex string", {aid, appIsoName});

                     appEntry.isoName = rt::ByteBuffer::fromHex(app["isoName"]);
                  }
               }

               // load application keys
               if (app.contains("keys"))
               {
                  auto keys = app["keys"];

                  if (!keys.is_array())
                     return error(-1, "invalid application 0x{06}, keys must be an array", {aid});

                  for (const auto &key: keys)
                  {
                     if (auto required = require(key, {"id", "type", "version", "value"}); !required.empty())
                        return error(-1, "invalid application 0x{06x}, missing key attributes: {}", {aid, required});

                     if (auto value = static_cast<std::string>(key["value"]); !rt::Format::isHex(value))
                        return error(-1, "invalid application 0x{06x}, key '{}', must be hex string", {aid, value});

                     KeyEntry keyEntry {
                        .id = key["id"],
                        .type = key["type"],
                        .version = key["version"],
                        .key = rt::ByteBuffer::fromHex(key["value"]),
                     };

                     appEntry.keys.emplace(keyEntry.id, keyEntry);

                     LOG_INFO(log, "load key 0x{02x}", {keyEntry.id});
                  }
               }

               // load application files
               if (app.contains("files"))
               {
                  auto files = app["files"];

                  if (!files.is_array())
                     return error(-1, "invalid application 0x{06}, files must be an array", {aid});

                  for (const auto &file: files)
                  {
                     if (auto required = require(file, {"id", "type", "accessRights", "commSettings"}); !required.empty())
                        return error(-1, "invalid application 0x{06x}, missing file attributes: {}", {aid, required});

                     FileEntry fileEntry {
                        .fileId = file["id"],
                        .fileType = file["type"],
                        .commSettings = file["commSettings"],
                        .accessRights = file["accessRights"]
                     };

                     if (appEntry.isoEnabled)
                     {
                        if (auto required = require(file, {"isoId"}); !required.empty())
                           return error(-1, "invalid application 0x{06x}, missing file attributes: {}", {aid, required});

                        fileEntry.isoId = file["isoId"];
                     }

                     switch (fileEntry.fileType)
                     {
                        case StandardFile:
                        case BackupFile:

                           if (auto required = require(file, {"size", "data"}); !required.empty())
                              return error(-1, "invalid application {06x}, file {}, missing file attributes: {}", {aid, fileEntry.fileId, required});

                           if (auto data = static_cast<std::string>(file["data"]); !rt::Format::isHex(data))
                              return error(-1, "invalid application 0x{06x}, file {} data '{}', must be hex string", {aid, fileEntry.fileId, data});

                           fileEntry.fileSize = file["size"];
                           fileEntry.data = rt::ByteBuffer::fromHex(file["data"]);
                           fileEntry.backup = fileEntry.data.copy();
                           break;

                        case LinearRecordFile:
                        case CyclicRecordFile:

                           if (auto required = require(file, {"size", "data", "recordSize", "maxRecords"}); !required.empty())
                              return error(-1, "invalid application {06x}, missing file attributes: {}", {aid, required});

                           fileEntry.fileSize = file["size"];
                           fileEntry.recordSize = file["recordSize"];
                           fileEntry.recordLimit = file["maxRecords"];
                           fileEntry.data = rt::ByteBuffer::fromHex(file["data"]);
                           fileEntry.backup = fileEntry.data.copy();
                           break;

                        case ValueFile:

                           if (auto required = require(file, {"lowerLimit", "upperLimit", "limitedCredit", "value"}); !required.empty())
                              return error(-1, "invalid application {06x}, missing file attributes: {}", {aid, required});

                           fileEntry.lowerLimit = file["lowerLimit"];
                           fileEntry.upperLimit = file["upperLimit"];
                           fileEntry.creditLimit = file["limitedCredit"];
                           fileEntry.value = file["value"];
                           fileEntry.backupValue = fileEntry.value;
                           fileEntry.backupCreditLimit = 0;
                           break;
                     }

                     appEntry.files.emplace(fileEntry.fileId, fileEntry);

                     LOG_INFO(log, "load file 0x{02x}", {fileEntry.fileId});
                  }
               }

               // add new application
               loaded.push_back(appEntry);
            }
         }
      }

      // store loaded apps in target instance
      for (Application &app: loaded)
      {
         instance.addApplication(app);
      }

      return static_cast<int>(loaded.size());
   }

   std::string dump()
   {
      // build applications
      json applications;

      for (auto &[aid, app]: instance.applications)
      {
         json application;
         application["aid"] = aid;
         application["keySettings1"] = app.keySettings;
         application["keySettings2"] = app.cryptoMode << 6 | app.maximumKeys;

         if (app.isoEnabled)
         {
            application["isoId"] = app.isoId;
            application["isoName"] = rt::ByteBuffer::toHex(app.isoName);
         }

         // add application keys
         for (auto &[id, entry]: app.keys)
         {
            application["keys"].push_back({
               {"id", id},
               {"type", entry.type},
               {"version", entry.version},
               {"value", rt::ByteBuffer::toHex(entry.key)},
            });
         }

         // add application files
         for (auto &[id, entry]: app.files)
         {
            json file = {
               {"id", id},
               {"type", entry.fileType},
               {"accessRights", entry.accessRights},
               {"commSettings", entry.commSettings},
            };

            if (app.isoEnabled)
               file["isoId"] = entry.isoId;

            switch (entry.fileType)
            {
               case StandardFile:
               case BackupFile:
                  file["size"] = entry.fileSize;
                  file["data"] = rt::ByteBuffer::toHex(entry.data);
                  break;
               case LinearRecordFile:
               case CyclicRecordFile:
                  file["size"] = entry.fileSize;
                  file["recordSize"] = entry.recordSize;
                  file["maxRecords"] = entry.recordLimit;
                  file["data"] = rt::ByteBuffer::toHex(entry.data);
                  break;

               case ValueFile:
                  file["lowerLimit"] = entry.recordSize;
                  file["upperLimit"] = entry.recordLimit;
                  file["limitedCredit"] = entry.creditLimit;
                  file["value"] = entry.value;
                  break;
            }

            application["files"].push_back(file);
         }

         applications.push_back(application);
      }

      // build desfire info
      json info = {
         {
            "hw", {
               {"vendor", instance.hardwareVendor},
               {"type", instance.hardwareType},
               {"subtype", instance.hardwareSubtype},
               {"version", instance.hardwareVersion},
               {"storage", instance.hardwareStorage},
               {"protocol", instance.hardwareProtocol}
            }
         },
         {
            "sw", {
               {"vendor", instance.softwareVendor},
               {"type", instance.softwareType},
               {"subtype", instance.softwareSubtype},
               {"version", instance.softwareVersion},
               {"storage", instance.softwareStorage},
               {"protocol", instance.softwareProtocol}
            }
         },
         {
            "tr", {
               {"year", instance.productionYear},
               {"week", instance.productionWeek},
               {"batch", instance.batchNumber}
            }
         }
      };

      // build payload contents
      json payload = {
         {"info", info},
         {"directory", applications}
      };

      // build discovery parameters
      json discovery = {
         {"ATQA", targetATQA},
         {"SAK", targetSAK},
         {"UID", rt::ByteBuffer::toHex(targetUID)},
         {
            "ATS", {
               {"TB1", targetTB1},
               {"TC1", targetTC1},
               {"HB", rt::ByteBuffer::toHex(targetHB)}
            }
         },
      };

      // final target dump
      const json raw {
         {"type", "desfire"},
         {"version", 1},
         {"discovery", discovery},
         {"payload", payload}
      };

      return raw.dump(2);
   }

   static std::vector<std::string> require(const json &json, const std::vector<std::string> &keys)
   {
      std::vector<std::string> missing;

      for (const std::string &key: keys)
      {
         if (!json.contains(key))
            missing.push_back(key);
      }

      return missing;
   }

   // command processor
   int process(rt::ByteBuffer request, rt::ByteBuffer &response)
   {
      int cla = -1;
      int ins = 0;
      int lc = 0;
      int le = 0;
      int status = 0;
      rt::ByteBuffer data(256);

      // detect protocol mode on first command
      if (instance.protocol == UnknowProtocol)
      {
         if (request[0] == DESFIRE_CLA_WRAPPED || request[0] == DESFIRE_CLA_ISO)
            instance.protocol = WrappedProtocol;
         else
            instance.protocol = NativeProtocol;
      }

      do
      {
         // Wrapped or ISO7816 protocol header parser
         if (instance.protocol == WrappedProtocol)
         {
            if (request.remaining() < 5)
            {
               status = DESFIRE_ISO_STATUS_WRONG_LENGTH;
               break;
            }

            // read class and ins
            cla = request[0];
            ins = request[1];

            // check valid APDU classes
            if (cla != DESFIRE_CLA_WRAPPED && cla != DESFIRE_CLA_ISO)
            {
               status = DESFIRE_ISO_STATUS_CLA_NOT_SUPPORTED;
               break;
            }

            // for WRAPPED protocol remove header and LE before pass to command processor,
            if (cla == DESFIRE_CLA_WRAPPED)
            {
               // skip APDU header
               request.skip(5);

               // check presence of LE, and remove
               if (lc = request[4]; request.remaining() > lc)
                  le = request.pop();
            }
         }

         // Native header
         else
         {
            if (request.remaining() < 1)
            {
               status = DESFIRE_STATUS_LENGTH_ERROR;
               break;
            }

            // get native command
            ins = request.get();
         }

         // flush buffers
         if (instance.chaining == 0)
         {
            instance.command = ins;
            instance.header.clear();
            instance.buffer.clear();
         }

         // invalidate command if chaining is active and device do not continue
         if (instance.chaining && ins != DESFIRE_STATUS_ADDITIONAL_FRAME)
            instance.command = 0;
      }
      while (false);

      // execute ISO commands
      if (cla == DESFIRE_CLA_ISO)
      {
         // if has previous errors, skip command
         if (status == 0)
         {
            // execute command, use switch to avoid virtual calls
            switch (instance.command)
            {
               // --- ISO Manipulation Commands ---
               case DESFIRE_CMD_ISO_SELECT_FILE:
                  status = isoSelectFile.process(request, data);
                  break;
               case DESFIRE_CMD_ISO_READ_BINARY:
                  status = isoReadBinary.process(request, data);
                  break;
               case DESFIRE_CMD_ISO_UPDATE_BINARY:
                  status = isoUpdateBinary.process(request, data);
                  break;
               case DESFIRE_CMD_ISO_READ_RECORDS:
                  status = isoReadRecords.process(request, data);
                  break;
               case DESFIRE_CMD_ISO_UPDATE_RECORD:
                  status = isoUpdateRecord.process(request, data);
                  break;
               case DESFIRE_CMD_ISO_APPEND_RECORD:
                  status = isoAppendRecord.process(request, data);
                  break;
               case DESFIRE_CMD_ISO_GET_CHALLENGE:
               case DESFIRE_CMD_ISO_INTERNAL_AUTHENTICATE:
               case DESFIRE_CMD_ISO_EXTERNAL_AUTHENTICATE:
                  status = isoAuthentication.process(request, data);
                  break;
               default:
                  status = DESFIRE_ISO_STATUS_INS_NOT_SUPPORTED;
            }
         }

         // finish data processing
         data.flip();

         // add response data and status
         response.put(data);
         response.putInt(status, 2, rt::ByteBuffer::BigEndian);
      }

      // Execute native or wrapped commands
      else
      {
         // if has previous errors, skip command
         if (status == 0)
         {
            switch (instance.command)
            {
               // --- Security Related Commands ---
               case DESFIRE_CMD_AUTHENTICATE:
                  status = authenticate.process(request, data);
                  break;
               case DESFIRE_CMD_AUTHENTICATE_ISO:
                  status = authenticateISO.process(request, data);
                  break;
               case DESFIRE_CMD_AUTHENTICATE_AES:
                  status = authenticateAES.process(request, data);
                  break;
               case DESFIRE_CMD_CHANGE_KEY_SETTINGS:
                  status = changeKeySettings.process(request, data);
                  break;
               case DESFIRE_CMD_CHANGE_KEY:
                  status = changeKey.process(request, data);
                  break;
               case DESFIRE_CMD_GET_KEY_VERSION:
                  status = getKeyVersion.process(request, data);
                  break;
               case DESFIRE_CMD_GET_KEY_SETTINGS:
                  status = getKeySettings.process(request, data);
                  break;

               // --- PICC Level Commands ---
               case DESFIRE_CMD_CREATE_APPLICATION:
                  status = createApplication.process(request, data);
                  break;
               case DESFIRE_CMD_DELETE_APPLICATION:
                  status = deleteApplication.process(request, data);
                  break;
               case DESFIRE_CMD_SELECT_APPLICATION:
                  status = selectApplication.process(request, data);
                  break;
               case DESFIRE_CMD_GET_APPLICATION_IDS:
                  status = listApplications.process(request, data);
                  break;
               case DESFIRE_CMD_GET_DF_NAMES:
                  status = listDFNames.process(request, data);
                  break;
               case DESFIRE_CMD_GET_FREE_MEMORY:
                  status = getFreeMemory.process(request, data);
                  break;
               case DESFIRE_CMD_GET_VERSION:
                  status = getVersion.process(request, data);
                  break;
               case DESFIRE_CMD_FORMAT_PICC:
                  status = formatPICC.process(request, data);
                  break;

               // --- Application Level Commands ---
               case DESFIRE_CMD_GET_FILE_IDS:
                  status = listFiles.process(request, data);
                  break;
               case DESFIRE_CMD_GET_FILE_SETTINGS:
                  status = getFileSettings.process(request, data);
                  break;
               case DESFIRE_CMD_CHANGE_FILE_SETTINGS:
                  status = changeFileSettings.process(request, data);
                  break;
               case DESFIRE_CMD_CREATE_STD_DATA_FILE:
                  status = createStandardFile.process(request, data);
                  break;
               case DESFIRE_CMD_CREATE_BACKUP_DATA_FILE:
                  status = createBackupFile.process(request, data);
                  break;
               case DESFIRE_CMD_CREATE_VALUE_FILE:
                  status = createValueFile.process(request, data);
                  break;
               case DESFIRE_CMD_CREATE_LINEAR_RECORD_FILE:
                  status = createLinearRecordFile.process(request, data);
                  break;
               case DESFIRE_CMD_CREATE_CYCLIC_RECORD_FILE:
                  status = createCyclicRecordFile.process(request, data);
                  break;
               case DESFIRE_CMD_DELETE_FILE:
                  status = deleteFile.process(request, data);
                  break;

               // --- Data Manipulation Commands ---
               case DESFIRE_CMD_READ_DATA:
                  status = readData.process(request, data);
                  break;
               case DESFIRE_CMD_WRITE_DATA:
                  status = writeData.process(request, data);
                  break;
               case DESFIRE_CMD_WRITE_RECORD:
                  status = writeRecord.process(request, data);
                  break;
               case DESFIRE_CMD_READ_RECORDS:
                  status = readRecords.process(request, data);
                  break;
               case DESFIRE_CMD_CLEAR_RECORD_FILE:
                  status = clearRecordFile.process(request, data);
                  break;
               case DESFIRE_CMD_GET_VALUE:
                  status = getValue.process(request, data);
                  break;
               case DESFIRE_CMD_CREDIT:
                  status = credit.process(request, data);
                  break;
               case DESFIRE_CMD_DEBIT:
                  status = debit.process(request, data);
                  break;
               case DESFIRE_CMD_LIMITED_CREDIT:
                  status = limitedCredit.process(request, data);
                  break;
               case DESFIRE_CMD_COMMIT_TRANSACTION:
                  status = commitTransaction.process(request, data);
                  break;
               case DESFIRE_CMD_ABORT_TRANSACTION:
                  status = abortTransaction.process(request, data);
                  break;

               default:
                  status = DESFIRE_STATUS_LENGTH_ERROR;
            }
         }

         // finish data processing
         data.flip();

         // update chaining status
         if (status != DESFIRE_STATUS_ADDITIONAL_FRAME)
         {
            // reset chaining status
            instance.chaining = 0;

            // invalidate changes on fail
            if (status != DESFIRE_STATUS_OK && status != DESFIRE_STATUS_NO_CHANGES)
               instance.rollbackData();

            // invalidate authentication status on fail (only for ISO / AES modes)
            if (status != DESFIRE_STATUS_OK && (instance.isAuthenticatedISO() || instance.isAuthenticatedAES()))
               instance.invalidateAuth();
         }
         else
         {
            instance.chaining++;
         }

         // in native protocol status is the first byte of response
         if (instance.protocol == NativeProtocol)
            response.put(status);

         // add response data
         response.put(data);

         // in APDU protocol status are the 2 last bytes of response
         if (instance.protocol == WrappedProtocol)
            response.put(DESFIRE_SW1).put(status);
      }

      // log last sessionIv
      if (instance.auth && instance.auth->mode != LegacyAuthentication)
         LOG_DEBUG(log, " << sessionIv: {x}", {instance.auth->sessionIv.copy()});

      return 0;
   }

   int error(int result, const std::string &format, const std::vector<rt::Variant> &params = {})
   {
      LOG_ERROR(log, format, params);
      return result;
   }
};

Desfire::Desfire(DesfireSize size) : impl(std::make_unique<Impl>(size))
{
}

Desfire::Desfire(const std::string &raw) : impl(std::make_unique<Impl>(raw))
{
}

rt::Variant Desfire::get(const int id) const
{
   return impl->get(id);
}

bool Desfire::set(const int id, const rt::Variant &value)
{
   return impl->set(id, value);
}

void Desfire::select()
{
   impl->select();
}

void Desfire::deselect()
{
   impl->deselect();
}

std::string Desfire::raw() const
{
   return impl->dump();
}

int Desfire::process(const rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_DEBUG(impl->log, "Desfire >> {x}", {request});

   const auto startTime = std::chrono::high_resolution_clock::now();
   const int res = impl->process(request, response);
   const auto endTime = std::chrono::high_resolution_clock::now();

   response.flip();

   const auto time = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

   LOG_DEBUG(impl->log, "Desfire << {x} [{}]", {response, time});

   return res;
}

}
