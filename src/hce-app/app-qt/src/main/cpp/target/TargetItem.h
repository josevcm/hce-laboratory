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

#ifndef APP_TARGET_ITEM_H
#define APP_TARGET_ITEM_H

#include <QVector>
#include <QVariant>

class TargetItem : public QObject
{

   public:

      enum class State { Normal, Modified, Added };

      explicit TargetItem(const QVector<QVariant> &data, TargetItem *parent = nullptr);

      ~TargetItem() override;

      void appendChild(TargetItem *item);

      bool insertChild(int position, int count, int columns);

      void clearChilds();

      TargetItem *child(int row) const;

      int childCount() const;

      int columnCount() const;

      QVariant data(int column) const;

      void setData(int column, const QVariant &value);

      State state() const;

      void setState(State state);

      int row() const;

      TargetItem *parent() const;

      void replaceChild(int row, TargetItem *item);

      TargetItem *takeChild(int row);

      void removeChild(int row);

      void insertChildAt(int row, TargetItem *item);

   private:

      struct Impl;
      std::unique_ptr<Impl> impl;
};

#endif
