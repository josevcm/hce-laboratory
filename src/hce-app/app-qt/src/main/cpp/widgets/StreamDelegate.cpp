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

#include <QPainter>
#include <QByteArray>
#include <QDateTime>
#include <QHeaderView>

#include <format/DataFormat.h>
#include <model/StreamModel.h>

#include <styles/Theme.h>

#include "StreamWidget.h"
#include "StreamDelegate.h"

struct StreamDelegate::Impl
{
   StreamWidget *streamWidget;

   QRect type;
   QRect flag;

   QHash<int, int> columnType;

   explicit Impl(StreamWidget *streamWidget) : streamWidget(streamWidget),
                                               type(0, 2, 16, 16),
                                               flag(20, 2, 16, 16)
   {
   }

   QString formatValue(int column, const QVariant &value)
   {
      if (!columnType.contains(column) || !value.isValid())
         return {};

      switch (columnType[column])
      {
         case StreamWidget::None:
            return {};

         case StreamWidget::Integer:
            return QString("%1").arg(value.toInt());

         case StreamWidget::Seconds:
            return QString("%1").arg(value.toDouble(), 9, 'f', 6);

         case StreamWidget::DateTime:
            return QDateTime::fromMSecsSinceEpoch(value.toLongLong()).toString("yy-MM-dd hh:mm:ss.zzz");

         case StreamWidget::Elapsed:
         {
            if (value.toLongLong() < 1000)
               return QString("%1 ms").arg(static_cast<double>(value.toLongLong()) / 1000.0f, 3, 'f', 0);

            return QString("%1 s").arg(static_cast<double>(value.toLongLong()) / 1000.0f, 3, 'f', 0);
         }

         case StreamWidget::Rate:
         {
            if (value.toInt() < 10E3)
               return QString("%1k").arg(value.toInt() / 1000.0f, 3, 'f', 1);

            return QString("%1k").arg(value.toInt() / 1000.0f, 3, 'f', 0);
         }

         case StreamWidget::String:
            return value.toString();

         case StreamWidget::Hex:
            return value.toByteArray().toHex(' ');
      }

      return {};
   }
};

StreamDelegate::StreamDelegate(StreamWidget *parent) : QStyledItemDelegate(parent), impl(new Impl(parent))
{
   setObjectName("StreamDelegate");
}

void StreamDelegate::setColumnType(int section, int format)
{
   impl->columnType[section] = format;
}

void StreamDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
   QStyledItemDelegate::initStyleOption(option, index);

   // override mouse over state
   option->state &= ~QStyle::State_MouseOver;

   // mark whole column as sorted
   if (impl->streamWidget->horizontalHeader()->sortIndicatorSection() == index.column())
      option->state |= QStyle::State_MouseOver;

   // set cell formatted value
   option->text = impl->formatValue(index.column(), index.data());
}

void StreamDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyledItemDelegate::paint(painter, option, index);

   if (!index.isValid())
      return;

   if (index.column() != StreamModel::Columns::Flags)
      return;

   if (index.data().userType() != QMetaType::QStringList)
      return;

   QStringList flags = index.data().toStringList();

   QRect typeRect = impl->type.adjusted(option.rect.x(), option.rect.y(), option.rect.x(), option.rect.y());
   QRect flagRect = impl->flag.adjusted(option.rect.x(), option.rect.y(), option.rect.x(), option.rect.y());

   if (flags.contains("activate"))
      Theme::carrierOnIcon.paint(painter, typeRect);
   else if (flags.contains("deactivate"))
      Theme::carrierOffIcon.paint(painter, typeRect);
   else if (flags.contains("request"))
      Theme::requestIcon.paint(painter, typeRect);
   else if (flags.contains("response"))
      Theme::responseIcon.paint(painter, typeRect);
}
