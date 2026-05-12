/*

  This file is part of HCE-LABORATORY.

  Copyright (C) 2024 Jose Vicente Campos Martinez, <josevcm@gmail.com>

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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <target/TargetItem.h>

#include "TargetParser.h"

static const QStringList KEY_TYPE_NAMES = {"2K3DES", "3K3DES", "AES-128"};
static const QStringList FILE_TYPE_NAMES = {"Standard Data", "Backup Data", "Value", "Linear Record", "Cyclic Record"};
static const QStringList CRYPTO_NAMES = {"2K3DES (Legacy)", "3K3DES (ISO)", "AES-128"};
static const QStringList COMM_NAMES = {"Plain", "MACed", "—", "Encrypted"};
static const QList KEY_SIZES = {16, 24, 16};

static QVariant prop(const QString &name, const QString &value)
{
   return QVariant::fromValue(QVariantList {name, value});
}

static QString accessKeyLabel(int nibble)
{
   if (nibble == 0xE) return "FREE";
   if (nibble == 0xF) return "NEVER";
   return QString("Key %1").arg(nibble);
}

struct TargetParser::Impl
{
   TargetItem *parseCard(const QJsonObject &root)
   {
      const QJsonObject payload = root["payload"].toObject();
      const QJsonObject discoveryData = root["discovery"].toObject();
      const QJsonObject info = payload["info"].toObject();
      const QJsonArray directory = payload["directory"].toArray();

      const QJsonObject ats = discoveryData["ATS"].toObject();
      const QJsonObject trInfo = info["tr"].toObject();

      const int hwSubtype = info["hw"].toObject()["subtype"].toInt();
      const int hwVersion = info["hw"].toObject()["version"].toInt();
      const int swVersion = info["sw"].toObject()["version"].toInt();

      const QString name    = root["name"].toString();
      const QString uid     = discoveryData["UID"].toString();
      const QString atqaStr = QString("0x%1").arg(discoveryData["ATQA"].toInt(), 4, 16, QChar('0'));
      const QString sakStr  = QString("0x%1").arg(discoveryData["SAK"].toInt(), 2, 16, QChar('0'));
      const QString tb1Str  = QString("0x%1").arg(ats["TB1"].toInt(), 2, 16, QChar('0'));
      const QString tc1Str  = QString("0x%1").arg(ats["TC1"].toInt(), 2, 16, QChar('0'));
      const QString hbStr   = ats["HB"].toString();
      const QString hwStr   = QString("EV%1 v%2.%3").arg(hwSubtype).arg(hwVersion >> 8).arg(hwVersion & 0xFF);
      const QString swStr   = QString("v%1.%2").arg(swVersion >> 8).arg(swVersion & 0xFF);
      const QString prodStr = QString("week %1 / 20%2").arg(trInfo["week"].toInt()).arg(trInfo["year"].toInt());

      QVariantList cardInfo;

      if (!name.isEmpty())
         cardInfo << prop("Name", name);

      cardInfo
         << prop("UID", uid)
         << prop("ATQA", atqaStr)
         << prop("SAK", sakStr)
         << prop("ATS HB", hbStr)
         << prop("Hardware", hwStr)
         << prop("Software", swStr)
         << prop("Production", prodStr)
         << prop("Applications", QString::number(directory.size()));

      const auto card = new TargetItem({
         name.isEmpty() ? uid : name, QString("DESFire EV%1").arg(hwSubtype), QByteArray(),
         QVariant::fromValue(cardInfo),
         QString(QJsonDocument(root).toJson(QJsonDocument::Compact))
      });

      card->appendChild(parseDiscovery(uid, atqaStr, sakStr, tb1Str, tc1Str, hbStr));
      card->appendChild(parseVersion(hwStr, swStr, prodStr));
      card->appendChild(parseApplications(directory));

      return card;
   }

   TargetItem *parseDiscovery(const QString &uid, const QString &atqaStr, const QString &sakStr, const QString &tb1Str, const QString &tc1Str, const QString &hbStr)
   {
      QVariantList discoveryInfo;

      discoveryInfo << prop("UID", uid)
         << prop("ATQA", atqaStr)
         << prop("SAK", sakStr)
         << prop("ATS TB1", tb1Str)
         << prop("ATS TC1", tc1Str)
         << prop("ATS HB", hbStr);

      const auto discoveryItem = new TargetItem({"Discovery", QString(), QByteArray(), QVariant::fromValue(discoveryInfo)});
      discoveryItem->appendChild(new TargetItem({"UID", uid, QByteArray(), QVariant::fromValue(QVariantList {prop("UID", uid)})}));
      discoveryItem->appendChild(new TargetItem({"ATQA", atqaStr, QByteArray(), QVariant::fromValue(QVariantList {prop("ATQA", atqaStr)})}));
      discoveryItem->appendChild(new TargetItem({"SAK", sakStr, QByteArray(), QVariant::fromValue(QVariantList {prop("SAK", sakStr)})}));
      discoveryItem->appendChild(parseAts(tb1Str, tc1Str, hbStr));

      return discoveryItem;
   }

   static TargetItem *parseAts(const QString &tb1Str, const QString &tc1Str, const QString &hbStr)
   {
      QVariantList atsInfo;
      atsInfo << prop("TB1", tb1Str) << prop("TC1", tc1Str) << prop("HB", hbStr);

      const auto atsItem = new TargetItem({"ATS", QString(), QByteArray(), QVariant::fromValue(atsInfo)});
      atsItem->appendChild(new TargetItem({"TB1", tb1Str, QByteArray(), QVariant::fromValue(QVariantList {prop("TB1", tb1Str)})}));
      atsItem->appendChild(new TargetItem({"TC1", tc1Str, QByteArray(), QVariant::fromValue(QVariantList {prop("TC1", tc1Str)})}));
      atsItem->appendChild(new TargetItem({"HB", hbStr, QByteArray(), QVariant::fromValue(QVariantList {prop("HB", hbStr)})}));

      return atsItem;
   }

   static TargetItem *parseVersion(const QString &hwStr, const QString &swStr, const QString &prodStr)
   {
      QVariantList versionInfo;
      versionInfo << prop("Hardware", hwStr)
         << prop("Software", swStr)
         << prop("Production", prodStr);

      const auto versionItem = new TargetItem({"Version", QString(), QByteArray(), QVariant::fromValue(versionInfo)});
      versionItem->appendChild(new TargetItem({"Hardware", hwStr, QByteArray(), QVariant::fromValue(QVariantList {prop("Hardware", hwStr)})}));
      versionItem->appendChild(new TargetItem({"Software", swStr, QByteArray(), QVariant::fromValue(QVariantList {prop("Software", swStr)})}));
      versionItem->appendChild(new TargetItem({"Production", prodStr, QByteArray(), QVariant::fromValue(QVariantList {prop("Production", prodStr)})}));

      return versionItem;
   }

   static TargetItem *parseApplications(const QJsonArray &directory)
   {
      QVariantList appsInfo;

      appsInfo << prop("Count", QString::number(directory.size()));

      for (const auto &appVal: directory)
      {
         QJsonObject a = appVal.toObject();
         const int aid = a["aid"].toInt();
         appsInfo << prop(QString("%1").arg(aid, 6, 16, QChar('0')), (aid == 0) ? "PICC Master" : "Application");
      }

      const auto appsItem = new TargetItem({QString("Applications [%1]").arg(directory.size()), QString(), QByteArray(), QVariant::fromValue(appsInfo)});

      for (const auto &appVal: directory)
         appsItem->appendChild(parseApplication(appVal.toObject()));

      return appsItem;
   }

   static TargetItem *parseApplication(const QJsonObject &app)
   {
      const int aid = app["aid"].toInt();
      const QString aidStr = QString("%1").arg(aid, 6, 16, QChar('0'));
      const QString appLabel = (aid == 0) ? "PICC Master" : "Application";

      const QJsonArray keys = app["keys"].toArray();
      const QJsonArray files = app["files"].toArray();

      const int keySettings1 = app["keySettings1"].toInt();
      const int keySettings2 = app["keySettings2"].toInt();
      const int cryptoMode = (keySettings2 >> 6) & 0x3;
      const int maxKeys = keySettings2 & 0x0F;
      const bool isoEnabled = (keySettings2 & 0x20) != 0;

      const auto changeAccess = [](int ks1) -> QString {
         int nibble = (ks1 >> 4) & 0xF;
         if (nibble == 0xF) return "Frozen";
         if (nibble == 0xE) return "By self";
         if (nibble == 0x0) return "By master";
         return QString("By Key %1").arg(nibble);
      };

      QVariantList appInfo;

      appInfo
         << prop("AID", aidStr)
         << prop("Crypto", (cryptoMode < CRYPTO_NAMES.size()) ? CRYPTO_NAMES[cryptoMode] : "Unknown")
         << prop("Master key", changeAccess(keySettings1))
         << prop("Free list", (keySettings1 & 0x02) ? "Yes" : "No")
         << prop("Max keys", QString::number(maxKeys))
         << prop("ISO", isoEnabled ? "Enabled" : "Disabled");

      if (isoEnabled && app.contains("isoName"))
         appInfo << prop("ISO name", app["isoName"].toString());

      appInfo << prop("Keys", QString::number(keys.size()));

      if (!files.isEmpty())
         appInfo << prop("Files", QString::number(files.size()));

      const auto appItem = new TargetItem({aidStr, appLabel, QByteArray(), QVariant::fromValue(appInfo)});

      appItem->appendChild(parseKeys(keys));

      if (!files.isEmpty())
         appItem->appendChild(parseFiles(files));

      return appItem;
   }

   static TargetItem *parseKeys(const QJsonArray &keys)
   {
      QVariantList keysInfo;
      keysInfo << prop("Count", QString::number(keys.size()));

      for (const auto &kv: keys)
      {
         QJsonObject ko = kv.toObject();
         const int kt = ko["type"].toInt();
         keysInfo << prop(QString("Key %1").arg(ko["id"].toInt(), 2, 10, QChar('0')), (kt >= 0 && kt < KEY_TYPE_NAMES.size()) ? KEY_TYPE_NAMES[kt] : "Unknown");
      }

      const auto keysItem = new TargetItem({QString("Keys [%1]").arg(keys.size()), QString(), QByteArray(), QVariant::fromValue(keysInfo)});

      for (const auto &keyVal: keys)
         keysItem->appendChild(parseKey(keyVal.toObject()));

      return keysItem;
   }

   static TargetItem *parseKey(const QJsonObject &keyObj)
   {
      const int keyType = keyObj["type"].toInt();
      const QString keyName = QString("Key %1").arg(keyObj["id"].toInt(), 2, 10, QChar('0'));
      const QString typeName = (keyType >= 0 && keyType < KEY_TYPE_NAMES.size()) ? KEY_TYPE_NAMES[keyType] : "Unknown";
      const int keySize = (keyType >= 0 && keyType < KEY_SIZES.size()) ? KEY_SIZES[keyType] : 0;
      const QByteArray keyBytes = QByteArray::fromHex(keyObj["value"].toString().toLatin1());

      QVariantList keyInfo;

      keyInfo
         << prop("Key ID", QString::number(keyObj["id"].toInt()))
         << prop("Type", typeName)
         << prop("Length", QString("%1 bytes").arg(keySize))
         << prop("Version", QString::number(keyObj["version"].toInt()));

      return new TargetItem({keyName, typeName, keyBytes, QVariant::fromValue(keyInfo)});
   }

   static TargetItem *parseFiles(const QJsonArray &files)
   {
      QVariantList filesInfo;

      filesInfo << prop("Count", QString::number(files.size()));

      for (const auto &fv: files)
      {
         QJsonObject fo = fv.toObject();
         const int ft = fo["type"].toInt();
         filesInfo << prop(QString("File %1").arg(fo["id"].toInt(), 2, 10, QChar('0')), (ft >= 0 && ft < FILE_TYPE_NAMES.size()) ? FILE_TYPE_NAMES[ft] : "Unknown");
      }

      const auto filesItem = new TargetItem({QString("Files [%1]").arg(files.size()), QString(), QByteArray(), QVariant::fromValue(filesInfo)});

      for (const auto &fileVal: files)
         filesItem->appendChild(parseFile(fileVal.toObject()));

      return filesItem;
   }

   static TargetItem *parseFile(const QJsonObject &fileObj)
   {
      const int fileType = fileObj["type"].toInt();
      const int accessRights = fileObj["accessRights"].toInt();
      const int commSettings = fileObj["commSettings"].toInt();
      const QString fileLabel = QString("File %1").arg(fileObj["id"].toInt(), 2, 10, QChar('0'));
      const QString typeName = (fileType >= 0 && fileType < FILE_TYPE_NAMES.size()) ? FILE_TYPE_NAMES[fileType] : "Unknown";
      const QString commName = (commSettings < COMM_NAMES.size()) ? COMM_NAMES[commSettings] : "Unknown";
      const QByteArray fileBytes = QByteArray::fromHex(fileObj["data"].toString().toLatin1());

      QVariantList fileInfo;

      fileInfo
         << prop("File ID", QString::number(fileObj["id"].toInt()))
         << prop("Type", typeName)
         << prop("Size", QString("%1 bytes").arg(fileObj["size"].toInt()))
         << prop("Comm", commName)
         << prop("Read", accessKeyLabel((accessRights >> 12) & 0xF))
         << prop("Write", accessKeyLabel((accessRights >> 8) & 0xF))
         << prop("R+W", accessKeyLabel((accessRights >> 4) & 0xF))
         << prop("Change", accessKeyLabel(accessRights & 0xF));

      if (fileType == 3 || fileType == 4)
      {
         fileInfo
            << prop("Record size", QString("%1 bytes").arg(fileObj["recordSize"].toInt()))
            << prop("Max records", QString::number(fileObj["maxRecords"].toInt()));
      }

      if (fileType == 2)
      {
         fileInfo
            << prop("Lower limit", QString::number(fileObj["lowerLimit"].toInt()))
            << prop("Upper limit", QString::number(fileObj["upperLimit"].toInt()))
            << prop("Value", QString::number(fileObj["value"].toInt()))
            << prop("Limited credit", fileObj["limitedCredit"].toInt() > 0 ? QString::number(fileObj["limitedCredit"].toInt()) : "Disabled");
      }

      if (fileObj.contains("isoId"))
         fileInfo << prop("ISO EF ID", QString("0x%1").arg(fileObj["isoId"].toInt(), 4, 16, QChar('0')));

      return new TargetItem({fileLabel, typeName, fileBytes, QVariant::fromValue(fileInfo)});
   }
};

TargetParser::TargetParser() : impl(std::make_unique<Impl>())
{
}

TargetParser::~TargetParser() = default;

TargetItem *TargetParser::parse(const QJsonObject &root)
{
   return impl->parseCard(root);
}
