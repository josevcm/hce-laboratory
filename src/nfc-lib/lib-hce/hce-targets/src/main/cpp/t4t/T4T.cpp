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

#include <hce/targets/t4t/T4T.h>

namespace hce::targets {

struct T4T::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hce.targets.T4T");

   // target parameters
   unsigned short targetATQA = 0x4403;
   unsigned char targetSAK = 0x20;
   unsigned char targetTB1 = 0x81;
   unsigned char targetTC1 = 0x02;
   rt::ByteBuffer targetHB = {0x80};
   rt::ByteBuffer targetUID = rt::ByteBuffer::random(7);

   explicit Impl()
   {
   }

   rt::Variant getParam(int id)
   {
      switch (id)
      {
         case PARAM_ATQA:
            return targetATQA;

         case PARAM_SAK:
            return targetSAK;

         case PARAM_UID:
            return targetUID;

         case PARAM_RATS_TB1:
            return targetTB1;

         case PARAM_RATS_TC1:
            return targetTC1;

         case PARAM_RATS_HB:
            return targetHB;

         default:
            return {};
      }
   }

   bool setParam(int id, const rt::Variant &value)
   {
      switch (id)
      {
         case PARAM_ATQA:
         {
            if (const auto v = std::get_if<unsigned short>(&value))
            {
               targetATQA = *v;
               return true;
            }

            log->error("invalid value type for PARAM_ATQA");
            return false;
         }
         case PARAM_SAK:
         {
            if (const auto v = std::get_if<unsigned char>(&value))
            {
               targetSAK = *v;
               return true;
            }

            log->error("invalid value type for PARAM_SAK");
            return false;
         }
         case PARAM_UID:
         {
            if (const auto v = std::get_if<rt::Buffer<unsigned char>>(&value))
            {
               targetUID = rt::ByteBuffer(v->ptr(), v->size());
               return true;
            }

            log->error("invalid value type for PARAM_UID");
            return false;
         }
         case PARAM_RATS_TB1:
         {
            if (const auto v = std::get_if<unsigned char>(&value))
            {
               targetTB1 = *v;
               return true;
            }

            log->error("invalid value type for PARAM_RATS_TB1");
            return false;
         }
         case PARAM_RATS_TC1:
         {
            if (const auto v = std::get_if<unsigned char>(&value))
            {
               targetTC1 = *v;
               return true;
            }

            log->error("invalid value type for PARAM_RATS_TC1");
            return false;
         }
         case PARAM_RATS_HB:
         {
            if (const auto v = std::get_if<rt::Buffer<unsigned char>>(&value))
            {
               targetHB = rt::ByteBuffer(v->ptr(), v->size());
               return true;
            }

            log->error("invalid value type for PARAM_RATS_HIST");
            return false;
         }
         default:
            log->warn("unknown or unsupported configuration id {}", {id});
            return false;
      }
   }

   void selectCard()
   {
   }

   void deselectCard()
   {
   }

   // command processor
   int process(rt::ByteBuffer request, rt::ByteBuffer &response)
   {
      response.put({0x6E, 0x00}); // Class not supported
      return 0;
   }
};

T4T::T4T() : impl(std::make_unique<Impl>())
{
}

rt::Variant T4T::get(const int id)
{
   return impl->getParam(id);
}

bool T4T::set(const int id, const rt::Variant &value)
{
   return impl->setParam(id, value);
}

void T4T::select()
{
   impl->selectCard();
}

void T4T::deselect()
{
   impl->deselectCard();
}

int T4T::process(const rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_DEBUG(impl->log, "T4T >> {x}", {request});

   const auto startTime = std::chrono::high_resolution_clock::now();
   const int res = impl->process(request, response);
   const auto endTime = std::chrono::high_resolution_clock::now();

   response.flip();

   const auto time = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

   LOG_DEBUG(impl->log, "T4T << {x} [{}]", {response, time});

   return res;
}

}
