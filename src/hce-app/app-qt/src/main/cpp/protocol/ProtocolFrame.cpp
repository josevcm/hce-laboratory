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

#include "ProtocolFrame.h"

struct ProtocolFrame::Impl
{
   // frame flags
   int flags;

   // underline frame
   hce::Frame frame;

   // parent frame node
   ProtocolFrame *parent;

   // frame data contents
   QVector<QVariant> data;

   // frame childs
   QList<ProtocolFrame *> children;

   //
   int start;
   int end;

   Impl(int flags, const QVector<QVariant> &data, const hce::Frame &frame) : flags(flags), frame(frame), parent(nullptr), data(data), start(0), end(frame.limit())
   {
   }

   Impl(int flags, const QVector<QVariant> &data, ProtocolFrame *parent, int start, int end) : flags(flags), parent(parent), data(data), start(start), end(end)
   {
   }

   ~Impl()
   {
      qDeleteAll(children);
   }
};

ProtocolFrame::ProtocolFrame(const QVector<QVariant> &data, int flags, const hce::Frame &frame) :
   QObject(nullptr), impl(std::make_unique<Impl>(flags, data, frame))
{
}

ProtocolFrame::ProtocolFrame(const QVector<QVariant> &data, int flags, ProtocolFrame *parent, int start, int end) :
   QObject(parent), impl(std::make_unique<Impl>(flags, data, parent, start, end))
{
}

ProtocolFrame::~ProtocolFrame()
{
}

void ProtocolFrame::clearChilds()
{
   qDeleteAll(impl->children);
   impl->children.clear();
}

ProtocolFrame *ProtocolFrame::child(int row)
{
   if (row >= 0 && row < impl->children.count())
      return impl->children.at(row);

   return nullptr;
}

int ProtocolFrame::childDeep() const
{
   return impl->parent != nullptr ? impl->parent->childDeep() + 1 : 0;
}

int ProtocolFrame::childCount() const
{
   return impl->children.count();
}

int ProtocolFrame::columnCount() const
{
   return impl->data.count();
}

ProtocolFrame *ProtocolFrame::appendChild(ProtocolFrame *item)
{
   item->impl->parent = this;

   if (!impl->children.contains(item))
      impl->children.append(item);

   return item;
}

ProtocolFrame *ProtocolFrame::prependChild(ProtocolFrame *item)
{
   item->impl->parent = this;

   impl->children.prepend(item);

   return item;
}

bool ProtocolFrame::insertChild(int position, int count, int columns)
{
   if (position < 0 || position > impl->children.size())
      return false;

   for (int row = 0; row < count; ++row)
   {
      QVector<QVariant> data(columns);
      impl->children.insert(position, new ProtocolFrame(data, 0, this));
   }

   return true;
}

hce::Frame &ProtocolFrame::frame() const
{
   if (impl->frame.isValid())
      return impl->frame;

   return impl->parent ? impl->parent->frame() : impl->frame;
}

QVariant ProtocolFrame::data(int column) const
{
   return impl->data.value(column);
}

void ProtocolFrame::set(int column, const QVariant &value)
{
   impl->data[column] = value;
}

ProtocolFrame *ProtocolFrame::parent() const
{
   return impl->parent;
}

void ProtocolFrame::setParent(ProtocolFrame *parent)
{
   impl->parent = parent;
}

int ProtocolFrame::row() const
{
   if (impl->parent)
      return impl->parent->impl->children.indexOf(const_cast<ProtocolFrame *>(this));

   return -1;
}

int ProtocolFrame::rangeStart() const
{
   return impl->start;
}

int ProtocolFrame::rangeEnd() const
{
   return impl->end;
}

bool ProtocolFrame::isStartupFrame() const
{
   return impl->flags & StartupFrame || (impl->parent && impl->parent->isStartupFrame());
}

bool ProtocolFrame::isRequestFrame() const
{
   return impl->flags & RequestFrame || (impl->parent && impl->parent->isRequestFrame());
}

bool ProtocolFrame::isResponseFrame() const
{
   return impl->flags & ResponseFrame || (impl->parent && impl->parent->isResponseFrame());
}

bool ProtocolFrame::isFrameField() const
{
   return impl->flags & FrameField;
}

bool ProtocolFrame::isFieldInfo() const
{
   return impl->flags & FieldInfo;
}
