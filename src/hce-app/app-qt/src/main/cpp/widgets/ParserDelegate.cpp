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

#include <model/ParserModel.h>

#include <protocol/ProtocolFrame.h>

#include <styles/Theme.h>

#include "ParserDelegate.h"

struct ParserDelegate::Impl
{
   QRect type;
   QRect flag;

   Impl() : type(0, 2, 16, 16),
            flag(20, 2, 16, 16)
   {
   }
};

ParserDelegate::ParserDelegate(QObject *parent) : QStyledItemDelegate(parent), impl(new Impl())
{
}

ParserDelegate::~ParserDelegate() = default;

void ParserDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyledItemDelegate::paint(painter, option, index);

   if (!index.isValid() || index.column() != ParserModel::Columns::Flags)
      return;

   if (auto frame = static_cast<ProtocolFrame *>(index.internalPointer()))
   {
      QRect typeRect = impl->type.adjusted(option.rect.x(), option.rect.y(), option.rect.x(), option.rect.y());
      QRect flagRect = impl->flag.adjusted(option.rect.x(), option.rect.y(), option.rect.x(), option.rect.y());

      if (frame->childDeep() == 1)
      {
         if (frame->isStartupFrame())
            Theme::startupIcon.paint(painter, typeRect);
         else if (frame->isRequestFrame())
            Theme::requestIcon.paint(painter, typeRect);
         else if (frame->isResponseFrame())
            Theme::responseIcon.paint(painter, typeRect);
      }
   }
}

void ParserDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
   QStyledItemDelegate::initStyleOption(option, index);

   option->state &= ~QStyle::State_MouseOver;

   switch (index.column())
   {
      case ParserModel::Columns::Flags:
      {
         option->text = QString();
      }
   }
}
