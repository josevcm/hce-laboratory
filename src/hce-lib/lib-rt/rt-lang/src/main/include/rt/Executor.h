/*

  This file is part of HCE-LABORATORY.

  Copyright (C) 2025 Jose Vicente Campos Martinez, <josevcm@gmail.com>

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

#ifndef RT_EXECUTOR_H
#define RT_EXECUTOR_H

#include <memory>

#include <rt/Task.h>

namespace rt {

class Executor
{
   struct Impl;

   public:

      enum Priority
      {
         PRIORITY_LOWEST = 0,
         PRIORITY_NORMAL = 1,
         PRIORITY_HIGHEST = 2,
         PRIORITY_REALTIME = 3,
      };

   public:

      explicit Executor(int poolSize = 100, int coreSize = 4);

      ~Executor();

      void submit(Task *task, Priority priority = PRIORITY_NORMAL);

      void shutdown();

   private:

      std::shared_ptr<Impl> impl;
};

}

#endif
