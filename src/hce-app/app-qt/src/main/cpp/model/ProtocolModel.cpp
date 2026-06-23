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

#include <QColor>
#include <QVector>
#include <algorithm>

#include <hce/Frame.h>

#include <protocol/DesfireParser.h>

#include "ProtocolModel.h"

struct ProtocolModel::Impl
{
   struct Row
   {
      qulonglong time = 0;
      qulonglong responseTime = 0;
      QString command;
      QByteArray request;
      QByteArray response;
      QString status;
      QString detail;
      bool completed = false;
      bool error = false;
      DesfireParser::RequestInfo requestInfo;
   };

   QVector<Row> rows;
};

ProtocolModel::ProtocolModel(QObject *parent) : QAbstractTableModel(parent), impl(std::make_unique<Impl>())
{
}

ProtocolModel::~ProtocolModel() = default;

int ProtocolModel::rowCount(const QModelIndex &parent) const
{
   Q_UNUSED(parent)
   return static_cast<int>(impl->rows.size());
}

int ProtocolModel::columnCount(const QModelIndex &parent) const
{
   Q_UNUSED(parent)
   return 4;
}

QVariant ProtocolModel::data(const QModelIndex &index, int role) const
{
   if (!index.isValid() || index.row() < 0 || index.row() >= impl->rows.size())
      return {};

   const Impl::Row &row = impl->rows[index.row()];

   if (role == Qt::DisplayRole)
   {
      switch (index.column())
      {
         case Time: return row.time;
         case Command: return row.command;
         case Status: return row.status;
         case Detail: return row.detail;
         default: return {};
      }
   }

   if (role == RawRequestRole)
      return row.request;

   if (role == RawResponseRole)
      return row.response;

    if (role == RequestTimeRole)
      return row.time;

   if (role == ResponseTimeRole)
      return row.responseTime;

   if (role == Qt::ForegroundRole && row.error && (index.column() == Status || index.column() == Detail))
      return QColor(0xE5, 0x39, 0x35);

   if (role == Qt::TextAlignmentRole && (index.column() == Time || index.column() == Status))
      return int(Qt::AlignHCenter | Qt::AlignVCenter);

   return {};
}

QVariant ProtocolModel::headerData(const int section, Qt::Orientation orientation, const int role) const
{
   if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
      return {};

   switch (section)
   {
      case Time: return tr("Time");
      case Command: return tr("Command");
      case Status: return tr("Status");
      case Detail: return tr("Info");
      default: return {};
   }
}

Qt::ItemFlags ProtocolModel::flags(const QModelIndex &index) const
{
   if (!index.isValid())
      return Qt::NoItemFlags;

   return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void ProtocolModel::sort(const int column, const Qt::SortOrder order)
{
   if (impl->rows.size() < 2)
      return;

   auto compareString = [order](const QString &left, const QString &right) {
      const int cmp = QString::compare(left, right, Qt::CaseInsensitive);
      return (order == Qt::AscendingOrder) ? (cmp < 0) : (cmp > 0);
   };

   auto compare = [&](const Impl::Row &left, const Impl::Row &right) {
      switch (column)
      {
         case Time:
            return (order == Qt::AscendingOrder) ? (left.time < right.time) : (left.time > right.time);

         case Command:
         {
            if (QString::compare(left.command, right.command, Qt::CaseInsensitive) == 0)
               return left.time < right.time;

            return compareString(left.command, right.command);
         }

         case Status:
         {
            if (QString::compare(left.status, right.status, Qt::CaseInsensitive) == 0)
               return left.time < right.time;

            return compareString(left.status, right.status);
         }

         case Detail:
         {
            if (QString::compare(left.detail, right.detail, Qt::CaseInsensitive) == 0)
               return left.time < right.time;

            return compareString(left.detail, right.detail);
         }

         default:
            return (order == Qt::AscendingOrder) ? (left.time < right.time) : (left.time > right.time);
      }
   };

   emit layoutAboutToBeChanged();
   std::stable_sort(impl->rows.begin(), impl->rows.end(), compare);
   emit layoutChanged();
}

void ProtocolModel::append(const hce::Frame &frame)
{
   QByteArray bytes;
   bytes.reserve(frame.limit());

   for (int i = 0; i < frame.limit(); ++i)
      bytes.append(static_cast<char>(frame[i]));

   if (frame.frameType() == hce::FrameType::NfcRequestFrame)
   {
      Impl::Row row;
      row.time = frame.frameTime();
      row.request = bytes;
      row.requestInfo = DesfireParser::parseRequest(bytes);
      row.command = row.requestInfo.commandName;
      const QString requestDetail = row.requestInfo.params.isEmpty() ? row.requestInfo.summary : row.requestInfo.params;
      row.detail = requestDetail.isEmpty() ? QString("REQ: <none>") : QString("REQ: %1").arg(requestDetail);
      row.status = "pending";

      const int newRow = static_cast<int>(impl->rows.size());
      beginInsertRows({}, newRow, newRow);
      impl->rows.push_back(row);
      endInsertRows();
      return;
   }

   if (frame.frameType() == hce::FrameType::NfcResponseFrame)
   {
      int targetRow = -1;

      for (qsizetype row = impl->rows.size() - 1; row >= 0; --row)
      {
         if (!impl->rows[row].completed)
         {
            targetRow = static_cast<int>(row);
            break;
         }

         if (row == 0)
            break;
      }

      if (targetRow < 0)
      {
         Impl::Row row;
         row.time = frame.frameTime();
         row.command = "<orphan response>";
         row.response = bytes;
         row.status = "unknown";
         row.detail = "response without matching request";

         const int newRow = static_cast<int>(impl->rows.size());
         beginInsertRows({}, newRow, newRow);
         impl->rows.push_back(row);
         endInsertRows();
         return;
      }

      Impl::Row &row = impl->rows[targetRow];
      const auto response = DesfireParser::parseResponse(bytes, row.requestInfo);
      row.response = bytes;
      row.responseTime = frame.frameTime();
      row.status = response.status;
      const QString responseDetail = response.params.isEmpty() ? response.summary : response.params;

      if (responseDetail.isEmpty())
         row.detail = QString("%1 || RES: <none>").arg(row.detail);
      else
         row.detail = QString("%1 || RES: %2").arg(row.detail, responseDetail);

      row.error = !response.ok;
      row.completed = true;

      emit dataChanged(index(targetRow, 0), index(targetRow, Detail));
   }
}

void ProtocolModel::resetModel()
{
   beginResetModel();
   impl->rows.clear();
   endResetModel();
}
