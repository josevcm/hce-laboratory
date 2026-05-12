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

#include <QColor>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <target/TargetItem.h>

#include "TargetModel.h"
#include "TargetParser.h"

struct TargetModel::Impl
{
   TargetItem *root;

   QFont headerFont;
   QFont defaultFont;

   int activeTargetRow = -1;

   Impl()
   {
      root = new TargetItem({tr("Target"), tr("Type")});

      headerFont.setPointSize(11);
      defaultFont.setPointSize(11);
   }

   ~Impl()
   {
      delete root;
   }
};

TargetModel::TargetModel(QObject *parent) : QAbstractItemModel(parent), impl(std::make_unique<Impl>())
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

      case BytesRole:
         return item->data(2);

      case InfoRole:
         return item->data(3);

      case JsonRole:
         return item->data(4);

      case Qt::ForegroundRole:

         switch (item->state())
         {
            case TargetItem::State::Modified: return QColor("#FFA726");
            case TargetItem::State::Added:    return QColor("#66BB6A");
            default: break;
         }

         break;

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

Qt::ItemFlags TargetModel::flags(const QModelIndex &index) const
{
   if (!index.isValid())
      return Qt::NoItemFlags;

   if (impl->activeTargetRow >= 0)
   {
      QModelIndex root = index;
      while (root.parent().isValid())
         root = root.parent();

      if (root.row() != impl->activeTargetRow)
         return Qt::NoItemFlags;
   }

   return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void TargetModel::setActiveTarget(int row)
{
   impl->activeTargetRow = row;
   emit layoutAboutToBeChanged();
   emit layoutChanged();
}

static void resetItemStates(TargetItem *item)
{
   item->setState(TargetItem::State::Normal);
   for (int i = 0; i < item->childCount(); ++i)
      resetItemStates(item->child(i));
}

void TargetModel::clearActiveTarget()
{
   impl->activeTargetRow = -1;
   resetItemStates(impl->root);
   emit layoutAboutToBeChanged();
   emit layoutChanged();
}

bool TargetModel::load(const QString &fileName)
{
   QFile file(fileName);

   if (!file.open(QIODevice::ReadOnly))
      return false;

   QJsonParseError parseError;
   QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);

   if (doc.isNull() || !doc.isObject())
      return false;

   QJsonObject root = doc.object();

   if (root["type"].toString() != "desfire")
      return false;

   TargetItem *card = TargetParser().parse(root);

   int row = impl->root->childCount();
   beginInsertRows(QModelIndex(), row, row);
   impl->root->appendChild(card);
   endInsertRows();

   return true;
}

void TargetModel::resetModel()
{
   beginResetModel();
   impl->root->clearChilds();
   endResetModel();
}

void TargetModel::updateContent(int row, const QJsonObject &root)
{
   if (row < 0 || row >= impl->root->childCount())
      return;

   TargetItem *newCard = TargetParser().parse(root);
   TargetItem *cardItem = impl->root->child(row);
   QModelIndex cardIndex = createIndex(row, 0, cardItem);

   // Update card's own data columns in-place
   bool changed = false;
   for (int col = 0; col < cardItem->columnCount() && col < newCard->columnCount(); ++col)
   {
      if (cardItem->data(col) != newCard->data(col))
      {
         cardItem->setData(col, newCard->data(col));
         changed = true;
      }
   }
   if (changed)
      emit dataChanged(cardIndex, createIndex(row, columnCount({}) - 1, cardItem));

   // Recursively merge children without resetting the model
   mergeTree(cardIndex, cardItem, newCard);

   delete newCard;
}

static void markSubtreeAdded(TargetItem *item)
{
   item->setState(TargetItem::State::Added);
   for (int i = 0; i < item->childCount(); ++i)
      markSubtreeAdded(item->child(i));
}

static QString mergeKey(const QString &name)
{
   int bracket = name.indexOf('[');
   return bracket >= 0 ? name.left(bracket).trimmed() : name;
}

void TargetModel::mergeTree(const QModelIndex &parentIndex, TargetItem *existing, TargetItem *newItem)
{
   // Phase 1: remove children that no longer exist in the new tree (backwards to keep indices stable)
   QSet<QString> newNames;
   for (int i = 0; i < newItem->childCount(); ++i)
      newNames.insert(mergeKey(newItem->child(i)->data(0).toString()));

   for (int i = existing->childCount() - 1; i >= 0; --i)
   {
      if (!newNames.contains(mergeKey(existing->child(i)->data(0).toString())))
      {
         beginRemoveRows(parentIndex, i, i);
         existing->removeChild(i);
         endRemoveRows();
      }
   }

   // Phase 2: iterate new children in order — update existing ones, insert new ones
   int insertPos = 0;

   for (int ni = 0; ni < newItem->childCount(); ni++)
   {
      TargetItem *newChild = newItem->child(ni);
      const QString key = mergeKey(newChild->data(0).toString());

      // Search for a matching child in existing (from insertPos onward)
      int existingRow = -1;

      for (int i = insertPos; i < existing->childCount(); ++i)
      {
         if (mergeKey(existing->child(i)->data(0).toString()) == key)
         {
            existingRow = i;
            break;
         }
      }

      if (existingRow >= 0)
      {
         TargetItem *existingChild = existing->child(existingRow);

         // Update data columns in-place
         bool changed = false;

         for (int col = 0; col < existingChild->columnCount() && col < newChild->columnCount(); ++col)
         {
            if (existingChild->data(col) != newChild->data(col))
            {
               existingChild->setData(col, newChild->data(col));
               changed = true;
            }
         }

         // Only update state when data changed; preserve Added/Modified across events
         if (changed)
         {
            existingChild->setState(TargetItem::State::Modified);
            emit dataChanged(
               createIndex(existingRow, 0, existingChild),
               createIndex(existingRow, columnCount({}) - 1, existingChild));
         }

         // Recurse into children
         mergeTree(createIndex(existingRow, 0, existingChild), existingChild, newChild);

         insertPos = existingRow + 1;
      }
      else
      {
         // New child: steal the fully-built subtree from newItem and insert it
         TargetItem *orphan = newItem->takeChild(ni);
         ni--;  // next iteration re-checks position ni (now occupied by the following child)

         markSubtreeAdded(orphan);

         beginInsertRows(parentIndex, insertPos, insertPos);
         existing->insertChildAt(insertPos, orphan);
         endInsertRows();

         insertPos++;
      }
   }
}
