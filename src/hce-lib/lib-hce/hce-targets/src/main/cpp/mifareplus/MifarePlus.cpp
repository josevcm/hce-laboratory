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

#include <chrono>

#include <rt/Logger.h>

#include <hce/targets/MifarePlus.h>

#include "Instance.h"

namespace hce::targets {

struct MifarePlus::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hce.targets.mifareplus.MifarePlus");

   // card bundle
   Instance instance;

   explicit Impl(const unsigned char *uid)
   {
      memcpy(instance.uid, uid, 7);
   }

   // command processor
   int process(rt::ByteBuffer request, rt::ByteBuffer &response)
   {
      return 0;
   }
};

MifarePlus::MifarePlus(unsigned char uid[7]) : impl(std::make_unique<Impl>(uid))
{
}

int MifarePlus::process(const rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_DEBUG(impl->log, "MifarePlus >> {x}", {request});

   auto startTime = std::chrono::high_resolution_clock::now();
   int res = impl->process(request, response);
   auto endTime = std::chrono::high_resolution_clock::now();

   response.flip();

   auto time = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

   LOG_DEBUG(impl->log, "MifarePlus << {x} [{}]", {response, time});

   return res;
}

}
