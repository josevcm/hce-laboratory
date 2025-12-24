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

#include <QDebug>

#include <QFont>
#include <QLabel>
#include <QQueue>
#include <QDateTime>
#include <QReadLocker>

#include <hce/Frame.h>

#include "StreamModel.h"

struct StreamModel::Impl
{
   // time format
   int timeSource = Elapsed;

   // fonts
   QFont defaultFont;
   QFont requestDefaultFont;
   QFont responseDefaultFont;

   // table header
   QVector<QString> headers;

   // column tooltips
   QVector<QString> tooltips;

   // column types
   QVector<QMetaType::Type> types;

   // frame list
   QList<hce::Frame> frames;

   // frame stream
   QQueue<hce::Frame> stream;

   // stream lock
   QReadWriteLock lock;

   explicit Impl()
   {
      // setup column names
      headers << tr("#");
      headers << tr("Time");
      headers << tr("Delta");
      headers << tr("Rate");
      headers << tr("Type");
      headers << tr("Event");
      headers << tr("Origin");
      headers << tr("Frame");

      // setup column tooltips
      tooltips << tr("Frame sequence number");
      tooltips << tr("Start time of frame");
      tooltips << tr("Time between two consecutive events");
      tooltips << tr("Protocol symbol rate");
      tooltips << tr("Protocol modulation type");
      tooltips << tr("Protocol event name");
      tooltips << tr("Message origin from");
      tooltips << tr("Raw message data");

      // request fonts
      requestDefaultFont.setBold(true);

      // response fonts
      responseDefaultFont.setItalic(true);
   }

   ~Impl()
   {
   }

   int dataType(int section) const
   {
      switch (section)
      {
         case Id:
            return QMetaType::Int;

         case Time:
            return QMetaType::LongLong;

         case Delta:
            return QMetaType::LongLong;

         case Rate:
            return QMetaType::Int;

         case Tech:
         case Event:
            return QMetaType::QString;

         case Flags:
            return QMetaType::QStringList;

         case Data:
            return QMetaType::QByteArray;

         default:
            return QMetaType::UnknownType;
      }
   }

   QVariant dataValue(const QModelIndex &index, const hce::Frame &frame, const hce::Frame &prev) const
   {
      switch (index.column())
      {
         case Id:
            return index.row();

         case Time:
            return frameTime(frame);

         case Delta:
            return frameDelta(frame, prev);

         case Rate:
            return frameRate(frame);

         case Tech:
            return frameTech(frame);

         case Event:
            return frameEvent(frame, prev);

         case Flags:
            return frameFlags(frame);

         case Data:
            return frameData(frame);
      }

      return {};
   }

   QVariant frameStart(const hce::Frame &frame) const
   {
      return QVariant::fromValue(frame.frameTime());
   }

   QVariant frameTime(const hce::Frame &frame) const
   {
      return QVariant::fromValue(frame.frameTime());
   }

   QVariant frameDelta(const hce::Frame &frame, const hce::Frame &prev) const
   {
      if (!prev)
         return {};

      return QVariant::fromValue(frame.frameTime() - prev.frameTime());
   }

   QVariant frameRate(const hce::Frame &frame) const
   {
      if (frame.frameType() == hce::FrameType::NfcNoneFrame ||
         frame.frameType() == hce::FrameType::NfcActivateFrame ||
         frame.frameType() == hce::FrameType::NfcDeactivateFrame)
         return {};

      return QVariant::fromValue(frame.frameRate());
   }

   QVariant frameTech(const hce::Frame &frame) const
   {
      if (frame.techType() == hce::FrameTech::NfcATech)
         return "NfcA";

      if (frame.techType() == hce::FrameTech::NfcBTech)
         return "NfcB";

      return {};
   }

   QVariant frameEvent(const hce::Frame &frame, const hce::Frame &prev) const
   {
      switch (frame.frameType())
      {

         case hce::FrameType::NfcActivateFrame:
            return {"ACTIVATE"};

         case hce::FrameType::NfcDeactivateFrame:
            return {"DEACTIVATE"};

         default:
            break;
      }

      switch (frame.techType())
      {
         case hce::FrameTech::NfcATech:
            return eventNfcA(frame, prev);

         case hce::FrameTech::NfcBTech:
            return eventNfcB(frame, prev);

         default:
            break;
      }

      return {};
   }

   QVariant frameFlags(const hce::Frame &frame) const
   {
      QStringList flags;

      switch (frame.frameType())
      {
         case hce::FrameType::NfcActivateFrame:
            flags.append("activate");
            break;
         case hce::FrameType::NfcDeactivateFrame:
            flags.append("deactivate");
            break;
         case hce::FrameType::NfcRequestFrame:
            flags.append("request");
            break;
         case hce::FrameType::NfcResponseFrame:
            flags.append("response");
            break;
      }

      return QVariant::fromValue(flags);
   }

   QVariant frameData(const hce::Frame &frame) const
   {
      QByteArray data;

      for (int i = 0; i < frame.limit(); i++)
      {
         data.append(frame[i]);
      }

      return data;
   }

   static QString eventNfcA(const hce::Frame &frame, const hce::Frame &prev)
   {
      QString result;

      return {};
   }

   static QString eventNfcB(const hce::Frame &frame, const hce::Frame &prev)
   {
      QString result;

      return {};
   }
};

StreamModel::StreamModel(QObject *parent) : QAbstractTableModel(parent), impl(new Impl)
{
}

int StreamModel::rowCount(const QModelIndex &parent) const
{
   return impl->frames.size();
}

int StreamModel::columnCount(const QModelIndex &parent) const
{
   return impl->headers.size();
}

QVariant StreamModel::data(const QModelIndex &index, int role) const
{
   if (!index.isValid() || index.row() >= impl->frames.size() || index.row() < 0)
      return {};

   hce::Frame prev;

   auto frame = impl->frames.at(index.row());

   if (index.row() > 0)
      prev = impl->frames.at(index.row() - 1);

   if (role == Qt::DisplayRole)
      return impl->dataValue(index, frame, prev);

   if (role == Qt::FontRole)
   {
      switch (index.column())
      {
         case Data:
         {
            if (frame.frameType() == hce::FrameType::NfcRequestFrame)
               return impl->requestDefaultFont;

            if (frame.frameType() == hce::FrameType::NfcResponseFrame)
               return impl->responseDefaultFont;

            break;
         }

         case Event:
         {
            if (frame.frameType() == hce::FrameType::NfcRequestFrame)
               return impl->responseDefaultFont;
         }
      }

      return impl->defaultFont;
   }

   if (role == Qt::ForegroundRole)
   {
      switch (index.column())
      {
         case Event:
         case Data:
            if (frame.frameType() == hce::FrameType::NfcResponseFrame)
               return QColor(Qt::darkGray);
      }

      return {};
   }

   if (role == Qt::TextAlignmentRole)
   {
      switch (index.column())
      {
         case Time:
         case Delta:
            return int(Qt::AlignRight | Qt::AlignVCenter);

         case Id:
         case Tech:
         case Rate:
         case Event:
            return int(Qt::AlignHCenter | Qt::AlignVCenter);
      }

      return int(Qt::AlignLeft | Qt::AlignVCenter);
   }

   if (role == Qt::SizeHintRole)
   {
      return QSize(0, 20);
   }

   return {};
}

Qt::ItemFlags StreamModel::flags(const QModelIndex &index) const
{
   if (!index.isValid())
      return Qt::NoItemFlags;

   return {Qt::ItemIsEnabled | Qt::ItemIsSelectable};
}

QVariant StreamModel::headerData(const int section, Qt::Orientation orientation, const int role) const
{
   switch (role)
   {
      case Qt::DisplayRole:
         return impl->headers.value(section);

      case Qt::ToolTipRole:
         return impl->tooltips.value(section);

      case Qt::UserRole:
         return impl->dataType(section);

      default:
         break;
   }

   return {};
}

QModelIndex StreamModel::index(const int row, const int column, const QModelIndex &parent) const
{
   if (!hasIndex(row, column, parent))
      return {};

   return createIndex(row, column, &impl->frames[row]);
}

bool StreamModel::canFetchMore(const QModelIndex &parent) const
{
   QReadLocker locker(&impl->lock);

   return impl->stream.size() > 0;
}

void StreamModel::fetchMore(const QModelIndex &parent)
{
   QReadLocker locker(&impl->lock);

   beginInsertRows(QModelIndex(), impl->frames.size(), impl->frames.size() + impl->stream.size() - 1);

   while (!impl->stream.isEmpty())
   {
      hce::Frame frame = impl->stream.dequeue();

      // find insertion point
      auto it = std::lower_bound(impl->frames.begin(), impl->frames.end(), frame);

      // insert frame sorted by time
      impl->frames.insert(it - impl->frames.begin(), frame);
   }

   endInsertRows();
}

void StreamModel::resetModel()
{
   beginResetModel();
   impl->frames.clear();
   endResetModel();
}

QModelIndexList StreamModel::modelRange(double from, double to)
{
   QModelIndexList list;

   for (int i = 0; i < impl->frames.size(); i++)
   {
      auto frame = impl->frames.at(i);

      if (frame.frameTime() < to && frame.frameTime() > from)
      {
         list.append(index(i, 0));
      }
   }

   return list;
}

void StreamModel::append(const hce::Frame &frame)
{
   QWriteLocker locker(&impl->lock);

   impl->stream.enqueue(frame);
}

const hce::Frame *StreamModel::frame(const QModelIndex &index) const
{
   if (!index.isValid())
      return nullptr;

   return static_cast<hce::Frame *>(index.internalPointer());
}

int StreamModel::timeSource() const
{
   return impl->timeSource;
}

void StreamModel::setTimeSource(TimeSource timeSource)
{
   impl->timeSource = timeSource;

   modelChanged();
}
