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

#include <QFont>
#include <QFontDatabase>

#include <target/TargetItem.h>

#include "TargetModel.h"

struct TargetModel::Impl
{
   TargetItem *root;

   // fonts
   QFont headerFont;
   QFont defaultFont;

   Impl()
   {
      QVector<QVariant> rootData;

      rootData << tr("Target") << tr("Type");

      root = new TargetItem(rootData);

      auto target1 = new TargetItem({"04568F4A70E080", "Desfire"});
      root->appendChild(target1);

      auto discoveryTarget1 = new TargetItem({"Discovery"});
      target1->appendChild(discoveryTarget1);

      auto infoTarget1 = new TargetItem({"Manufacturer"});
      target1->appendChild(infoTarget1);

      auto applicationsTarget1 = new TargetItem({"Applications"});
      target1->appendChild(applicationsTarget1);

      auto application1Target1 = new TargetItem({"000001"});
      applicationsTarget1->appendChild(application1Target1);

      auto application1Keys = new TargetItem({"Keys"});
      application1Target1->appendChild(application1Keys);

      auto application1Key00 = new TargetItem({"00", "AES"});
      application1Keys->appendChild(application1Key00);

      auto application1Key01 = new TargetItem({"01", "AES"});
      application1Keys->appendChild(application1Key01);

      auto application1Files = new TargetItem({"Files"});
      application1Target1->appendChild(application1Files);

      auto application1File01 = new TargetItem({"01", "Backup"});
      application1Files->appendChild(application1File01);

      auto application1File02 = new TargetItem({"02", "Backup"});
      application1Files->appendChild(application1File02);

      auto application1File03 = new TargetItem({"03", "CyclicRecord"});
      application1Files->appendChild(application1File03);

      auto application1File04 = new TargetItem({"04", "CyclicRecord"});
      application1Files->appendChild(application1File04);

      auto application1File05 = new TargetItem({"05", "Value"});
      application1Files->appendChild(application1File05);

      // setup fonts
      // headerFont.setFamily("Segoe UI");
      headerFont.setPointSize(11);
      // headerFont.setBold(true);

      // dataFont.setFamily("Segoe UI");
      defaultFont.setPointSize(11);
   }

   ~Impl()
   {
      delete root;
   }
};

TargetModel::TargetModel(QObject *parent) : QAbstractItemModel(parent), impl(new Impl)
{
}

TargetModel::~TargetModel() = default;

QModelIndex TargetModel::index(int row, int column, const QModelIndex &parent) const
{
   if (!hasIndex(row, column, parent))
      return {};

   const TargetItem *parentItem = parent.isValid() ? static_cast<TargetItem *>(parent.internalPointer()) : impl->root;

   const TargetItem *childItem = parentItem->child(row);

   return childItem ? createIndex(row, column, childItem) : QModelIndex();
}

QModelIndex TargetModel::parent(const QModelIndex &index) const
{
   if (!index.isValid())
      return {};

   const auto childItem = static_cast<TargetItem *>(index.internalPointer());

   const TargetItem *parentItem = childItem->parent();

   if (parentItem == impl->root || !parentItem)
      return {};

   return createIndex(parentItem->row(), 0, parentItem);
}

int TargetModel::rowCount(const QModelIndex &parent) const
{
   const TargetItem *parentItem = parent.isValid() ? static_cast<TargetItem *>(parent.internalPointer()) : impl->root;

   return parentItem->childCount();
}

int TargetModel::columnCount(const QModelIndex &) const
{
   return impl->root->columnCount();
}

bool TargetModel::hasChildren(const QModelIndex &parent) const
{
   if (!parent.isValid())
      return impl->root->childCount() > 0;

   return static_cast<TargetItem *>(parent.internalPointer())->childCount() > 0;
}

bool TargetModel::insertRows(int position, int rows, const QModelIndex &parent)
{
   auto parentTarget = !parent.isValid() ? impl->root : static_cast<TargetItem *>(parent.internalPointer());

   beginInsertRows(parent, position, position + rows - 1);
   bool success = parentTarget->insertChild(position, rows, impl->root->columnCount());
   endInsertRows();

   return success;
}

QVariant TargetModel::data(const QModelIndex &index, int role) const
{
   if (!index.isValid())
      return {};

   const auto item = static_cast<TargetItem *>(index.internalPointer());

   switch (role)
   {
      case Qt::DisplayRole:
         return item->data(index.column());

      case Qt::FontRole:
         return impl->defaultFont;
   }

   return {};
}

QVariant TargetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return impl->root->data(section);

   if (role == Qt::FontRole)
      return impl->headerFont;

   return {};
}

void TargetModel::resetModel()
{
   beginResetModel();
   impl->root->clearChilds();
   endResetModel();
}
