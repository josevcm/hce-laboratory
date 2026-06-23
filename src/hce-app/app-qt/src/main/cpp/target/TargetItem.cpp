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

#include "TargetItem.h"

struct TargetItem::Impl
{
   TargetItem *parent;
   QVector<QVariant> data;
   QVector<TargetItem *> children;
   State state = State::Normal;

   Impl(const QVector<QVariant> &data, TargetItem *parent) : parent(parent), data(data)
   {
   }

   ~Impl()
   {
      qDeleteAll(children);
   }
};

TargetItem::TargetItem(const QVector<QVariant> &data, TargetItem *parent) : QObject(nullptr), impl(std::make_unique<Impl>(data, parent))
{
}

TargetItem::~TargetItem() = default;

void TargetItem::appendChild(TargetItem *item)
{
   item->impl->parent = this;

   if (!impl->children.contains(item))
      impl->children.append(item);
}

bool TargetItem::insertChild(int position, int count, int columns)
{
   if (position < 0 || position > impl->children.size())
      return false;

   for (int row = 0; row < count; ++row)
   {
      QVector<QVariant> data(columns);
      impl->children.insert(position, new TargetItem(data, this));
   }

   return true;
}

void TargetItem::clearChilds()
{
   qDeleteAll(impl->children);
   impl->children.clear();
}

TargetItem *TargetItem::child(int row) const
{
   return impl->children.value(row);
}

int TargetItem::childCount() const
{
   return impl->children.size();
}

int TargetItem::columnCount() const
{
   return impl->data.size();
}

QVariant TargetItem::data(int column) const
{
   return impl->data.value(column);
}

int TargetItem::row() const
{
   return impl->parent ? impl->parent->impl->children.indexOf(const_cast<TargetItem *>(this)) : 0;
}

TargetItem *TargetItem::parent() const
{
   return impl->parent;
}

void TargetItem::setData(int column, const QVariant &value)
{
   if (column >= 0 && column < impl->data.size())
      impl->data[column] = value;
}

TargetItem::State TargetItem::state() const
{
   return impl->state;
}

void TargetItem::setState(State s)
{
   impl->state = s;
}

TargetItem *TargetItem::takeChild(int row)
{
   if (row < 0 || row >= impl->children.size())
      return nullptr;

   TargetItem *item = impl->children.takeAt(row);
   item->impl->parent = nullptr;
   return item;
}

void TargetItem::removeChild(int row)
{
   if (row < 0 || row >= impl->children.size())
      return;

   delete impl->children.takeAt(row);
}

void TargetItem::insertChildAt(int row, TargetItem *item)
{
   item->impl->parent = this;
   impl->children.insert(row, item);
}

void TargetItem::replaceChild(int row, TargetItem *item)
{
   if (row < 0 || row >= impl->children.size())
   {
      delete item;
      return;
   }

   delete impl->children[row];
   item->impl->parent = this;
   impl->children[row] = item;
}
