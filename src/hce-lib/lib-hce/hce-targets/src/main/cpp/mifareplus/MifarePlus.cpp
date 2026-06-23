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
#include <fstream>

#include <nlohmann/json.hpp>
#include <rt/Logger.h>
#include <rt/Format.h>

#include <hce/targets/MifarePlus.h>

#include "Instance.h"
#include "Command.h"
#include "MifarePlusResetAuth.h"
#include "MifarePlusFirstAuth.h"
#include "MifarePlusFollowingAuth.h"
#include "MifarePlusRead.h"
#include "MifarePlusReadEncrypted.h"
#include "MifarePlusWrite.h"
#include "MifarePlusWriteEncrypted.h"
#include "MifarePlusIncrement.h"
#include "MifarePlusDecrement.h"
#include "MifarePlusRestore.h"
#include "MifarePlusTransfer.h"
#include "MifarePlusIncrementTransfer.h"
#include "MifarePlusDecrementTransfer.h"
#include "MifarePlusRestoreTransfer.h"
#include "MifarePlusGetUID.h"

using json = nlohmann::json;

namespace hce::targets::mifareplus {

struct MifarePlus::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hce.targets.mifareplus.MifarePlus");

   MifarePlusResetAuth resetAuth;
   MifarePlusFirstAuth firstAuth;
   MifarePlusFollowingAuth followingAuth;
   MifarePlusRead read;
   MifarePlusReadEncrypted readEncrypted;
   MifarePlusWrite write;
   MifarePlusWriteEncrypted writeEncrypted;
   MifarePlusIncrement increment;
   MifarePlusDecrement decrement;
   MifarePlusRestore restore;
   MifarePlusTransfer transfer;
   MifarePlusIncrementTransfer incrementTransfer;
   MifarePlusDecrementTransfer decrementTransfer;
   MifarePlusRestoreTransfer restoreTransfer;
   MifarePlusGetUID getUID;

   Instance instance;

   unsigned short targetATQA = 0x0042;
   unsigned char targetSAK = 0x20;
   unsigned char targetTB1 = 0x81;
   unsigned char targetTC1 = 0x02;
   rt::ByteBuffer targetHB = rt::ByteBuffer::fromHex("C1");
   rt::ByteBuffer targetUID = rt::ByteBuffer::random(7);

   explicit Impl() :
      resetAuth(instance),
      firstAuth(instance),
      followingAuth(instance),
      read(instance),
      readEncrypted(instance),
      write(instance),
      writeEncrypted(instance),
      increment(instance),
      decrement(instance),
      restore(instance),
      transfer(instance),
      incrementTransfer(instance),
      decrementTransfer(instance),
      restoreTransfer(instance),
      getUID(instance)
   {
      instance.uid = targetUID;
      instance.sectorCount = MFPLUS_SIZE_2K;
      initMemory();
   }

   void initMemory()
   {
      const unsigned int total = instance.blockCount();
      instance.blocks.clear();
      instance.blocks.resize(total, rt::ByteBuffer::zero(16));
      instance.sectorMap.clear();

      for (unsigned int s = 0; s < instance.sectorCount; ++s)
      {
         SectorEntry entry;
         entry.keyA = rt::ByteBuffer::zero(16);
         entry.keyB = rt::ByteBuffer::zero(16);
         instance.sectorMap.emplace(s, entry);
      }
   }

   rt::Variant get(int id) const
   {
      switch (id)
      {
         case PARAM_ATQA: return targetATQA;
         case PARAM_SAK: return targetSAK;
         case PARAM_UID: return targetUID;
         case PARAM_RATS_TB1: return targetTB1;
         case PARAM_RATS_TC1: return targetTC1;
         case PARAM_RATS_HB: return targetHB;
         default: return {};
      }
   }

   bool set(int id, const rt::Variant &value)
   {
      switch (id)
      {
         case PARAM_ATQA:
            if (const auto v = std::get_if<unsigned short>(&value))
            {
               targetATQA = *v;
               return true;
            }
            break;
         case PARAM_SAK:
            if (const auto v = std::get_if<unsigned char>(&value))
            {
               targetSAK = *v;
               return true;
            }
            break;
         case PARAM_UID:
            if (const auto v = std::get_if<rt::ByteBuffer>(&value))
            {
               instance.uid = targetUID = v->copy();
               return true;
            }
            break;
         case PARAM_RATS_TB1:
            if (const auto v = std::get_if<unsigned char>(&value))
            {
               targetTB1 = *v;
               return true;
            }
            break;

         case PARAM_RATS_TC1:
            if (const auto v = std::get_if<unsigned char>(&value))
            {
               targetTC1 = *v;
               return true;
            }
            break;

         case PARAM_RATS_HB:
            if (const auto v = std::get_if<rt::ByteBuffer>(&value))
            {
               targetHB = v->copy();
               return true;
            }
            break;

         default: break;
      }

      log->warn("unknown or unsupported param id {}", {id});
      return false;
   }

   void select()
   {
      instance.invalidateAuth();
   }

   void deselect()
   {
   }

   std::string raw() const
   {
      return {};
   }

   bool isDirty() const
   {
      return instance.dirty;
   }

   void clearDirty()
   {
      instance.dirty = false;
   }

   int process(rt::ByteBuffer &request, rt::ByteBuffer &response)
   {
      if (request.remaining() < 1)
      {
         response.put(MFPLUS_STATUS_ERR_LENGTH);
         return -1;
      }

      const unsigned int ins = *request.ptr();

      LOG_DEBUG(log, "MifarePlus INS 0x{02x}", {ins});

      switch (ins)
      {
         case MFPLUS_CMD_FIRST_AUTH_P1:
         case MFPLUS_CMD_FIRST_AUTH_P2:
            return firstAuth.process(request, response);

         case MFPLUS_CMD_FOLLOWING_AUTH_P1:
         case MFPLUS_CMD_FOLLOWING_AUTH_P2:
            return followingAuth.process(request, response);

         case MFPLUS_CMD_RESET_AUTH:
            return resetAuth.process(request, response);

         case MFPLUS_CMD_READ:
            return read.process(request, response);

         case MFPLUS_CMD_READ_ENCRYPTED:
            return readEncrypted.process(request, response);

         case MFPLUS_CMD_WRITE:
            return write.process(request, response);

         case MFPLUS_CMD_WRITE_ENCRYPTED:
            return writeEncrypted.process(request, response);

         case MFPLUS_CMD_INCREMENT:
            return increment.process(request, response);

         case MFPLUS_CMD_DECREMENT:
            return decrement.process(request, response);

         case MFPLUS_CMD_RESTORE:
            return restore.process(request, response);

         case MFPLUS_CMD_TRANSFER:
            return transfer.process(request, response);

         case MFPLUS_CMD_INCREMENT_TRANSFER:
            return incrementTransfer.process(request, response);

         case MFPLUS_CMD_DECREMENT_TRANSFER:
            return decrementTransfer.process(request, response);

         case MFPLUS_CMD_RESTORE_TRANSFER:
            return restoreTransfer.process(request, response);

         case MFPLUS_CMD_GET_UID:
            return getUID.process(request, response);

         default:
            response.put(MFPLUS_STATUS_ERR_CMD);
            return -1;
      }
   }

   int load(const std::string &raw)
   {
      json target;

      try
      {
         std::ifstream f(raw);

         if (f.good())
            target = json::parse(f);
         else
            target = json::parse(raw);
      }
      catch (const std::exception &e)
      {
         log->error("failed to parse target: {}", {std::string(e.what())});
         return -1;
      }

      if (!target.contains("type") || target["type"] != "mifareplus")
      {
         log->error("invalid target type, must be 'mifareplus'");
         return -1;
      }

      if (!target.contains("version") || target["version"] != 1)
      {
         log->error("invalid target version, must be 1");
         return -1;
      }

      if (target.contains("discovery"))
      {
         auto discovery = target["discovery"];

         if (discovery.contains("ATQA")) targetATQA = discovery["ATQA"];
         if (discovery.contains("SAK")) targetSAK = static_cast<unsigned char>(static_cast<int>(discovery["SAK"]));
         if (discovery.contains("UID")) instance.uid = targetUID = rt::ByteBuffer::fromHex(discovery["UID"]);

         if (discovery.contains("ATS"))
         {
            auto ats = discovery["ATS"];
            if (ats.contains("TB1")) targetTB1 = ats["TB1"];
            if (ats.contains("TC1")) targetTC1 = ats["TC1"];
            if (ats.contains("HB")) targetHB = rt::ByteBuffer::fromHex(ats["HB"]);
         }
      }

      if (target.contains("payload"))
      {
         auto payload = target["payload"];

         if (payload.contains("size"))
         {
            std::string sz = payload["size"];
            instance.sectorCount = (sz == "4K") ? MFPLUS_SIZE_4K : MFPLUS_SIZE_2K;
         }

         initMemory();

         if (payload.contains("sectors") && payload["sectors"].is_array())
         {
            for (const auto &sec: payload["sectors"])
            {
               unsigned int s = sec["sector"];

               if (s >= instance.sectorCount)
               {
                  log->warn("sector {} out of range, skipping", {s});
                  continue;
               }

               auto &[keyA, keyB, accessBits] = instance.sectorMap[s];

               if (sec.contains("keyA")) keyA = rt::ByteBuffer::fromHex(sec["keyA"]);
               if (sec.contains("keyB")) keyB = rt::ByteBuffer::fromHex(sec["keyB"]);

               if (sec.contains("accessBits"))
               {
                  rt::ByteBuffer ab = rt::ByteBuffer::fromHex(sec["accessBits"]);
                  for (int i = 0; i < 4 && i < static_cast<int>(ab.size()); ++i)
                     accessBits[i] = ab.ptr()[i];
               }

               if (sec.contains("blocks") && sec["blocks"].is_array())
               {
                  for (const auto &blk: sec["blocks"])
                  {
                     unsigned int blkNo = blk["block"];

                     if (!instance.isValidBlock(blkNo))
                     {
                        log->warn("block {} out of range, skipping", {blkNo});
                        continue;
                     }

                     instance.blocks[blkNo] = rt::ByteBuffer::fromHex(blk["data"]);
                  }
               }
            }
         }
      }

      return 0;
   }
};

// --- MifarePlus public methods ---

MifarePlus::MifarePlus() : impl(std::make_shared<Impl>())
{
}

MifarePlus::MifarePlus(const std::string &tag) : impl(std::make_shared<Impl>())
{
   impl->load(tag);
}

rt::Variant MifarePlus::get(int id) const
{
   return impl->get(id);
}

bool MifarePlus::set(int id, const rt::Variant &value) { return impl->set(id, value); }

void MifarePlus::select()
{
   impl->select();
}

void MifarePlus::deselect()
{
   impl->deselect();
}

std::string MifarePlus::raw() const
{
   return impl->raw();
}

bool MifarePlus::isDirty() const
{
   return impl->isDirty();
}

void MifarePlus::clearDirty()
{
   impl->clearDirty();
}

int MifarePlus::process(const rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_DEBUG(impl->log, "MifarePlus >> {x}", {request});

   auto start = std::chrono::high_resolution_clock::now();

   rt::ByteBuffer req = request.copy();
   int res = impl->process(req, response);

   response.flip();

   auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start);

   LOG_DEBUG(impl->log, "MifarePlus << {x} [{}us]", {response, elapsed.count()});

   return res;
}

} // namespace hce::targets::mifareplus
