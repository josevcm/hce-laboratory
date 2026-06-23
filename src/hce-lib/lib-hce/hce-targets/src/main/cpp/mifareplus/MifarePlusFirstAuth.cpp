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
#include "Instance.h"
#include "MifarePlusFirstAuth.h"

namespace hce::targets::mifareplus {

MifarePlusFirstAuth::MifarePlusFirstAuth(Instance &i) : Command(i)
{
}

int MifarePlusFirstAuth::process(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   const unsigned int ins = request.get();

   if (ins == MFPLUS_CMD_FIRST_AUTH_P1)
      return step1(request, response);

   if (ins == MFPLUS_CMD_FIRST_AUTH_P2 && awaitingStep2)
      return step2(request, response);

   awaitingStep2 = false;
   response.put(MFPLUS_STATUS_ERR_CMD);
   return -1;
}

int MifarePlusFirstAuth::step1(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "firstAuth step1");

   if (request.remaining() != 2)
   {
      response.put(MFPLUS_STATUS_ERR_LENGTH);
      return -1;
   }

   const unsigned int keyNoLSB = request.get();
   const unsigned int keyNoMSB = request.get();
   const unsigned int keyBlockNo = keyNoLSB | (keyNoMSB << 8);
   const unsigned int sectorNo = keyBlockNo / 2;
   const unsigned int keyType = keyBlockNo & 0x01;

   LOG_INFO(log, "\tkeyBlockNo: 0x{04x} sector: {} keyType: {}", {keyBlockNo, sectorNo, keyType});

   if (sectorNo >= picc.sectorCount || picc.sectorMap.find(sectorNo) == picc.sectorMap.end())
   {
      response.put(MFPLUS_STATUS_ERR_PARAM);
      return -1;
   }

   picc.invalidateAuth();
   picc.auth.sectorNo = sectorNo;
   picc.auth.keyType = keyType;

   const auto &entry = picc.sectorMap.at(sectorNo);
   const rt::ByteBuffer &key = (keyType == MFPLUS_KEY_A) ? entry.keyA : entry.keyB;

   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   picc.aes.init(key, 0);

   rndB = rt::ByteBuffer::random(16);

   LOG_INFO(log, "\trndB: {x}", {rndB.copy()});

   response.put(0x90);
   response.put(picc.aes.encrypt(rndB, iv));

   awaitingStep2 = true;
   return 0;
}

int MifarePlusFirstAuth::step2(rt::ByteBuffer &request, rt::ByteBuffer &response)
{
   LOG_INFO(log, "firstAuth step2");

   if (request.remaining() != 32)
   {
      awaitingStep2 = false;
      response.put(MFPLUS_STATUS_ERR_LENGTH);
      return -1;
   }

   const auto &entry = picc.sectorMap.at(picc.auth.sectorNo);
   const rt::ByteBuffer &key = (picc.auth.keyType == MFPLUS_KEY_A) ? entry.keyA : entry.keyB;

   rt::ByteBuffer iv = rt::ByteBuffer::zero(16);
   picc.aes.init(key, 0);

   rt::ByteBuffer plain32 = picc.aes.decrypt(request.getBuffer(32), iv);

   const rt::ByteBuffer rndA = plain32.slice(0, 16);
   const rt::ByteBuffer rndBr = plain32.slice(16, 16);

   LOG_INFO(log, "\trndA: {x}", {rndA.copy()});

   if (rndBr != rt::ByteBuffer::rotateBytes(rndB, rt::ByteBuffer::Left))
   {
      awaitingStep2 = false;
      picc.invalidateAuth();
      response.put(MFPLUS_STATUS_ERR_AUTH);
      return -1;
   }

   const rt::ByteBuffer ti = rt::ByteBuffer::random(4);
   const rt::ByteBuffer rndAr = rt::ByteBuffer::rotateBytes(rndA, rt::ByteBuffer::Left);

   rt::ByteBuffer plain(32);
   plain.put(ti).put(rndAr).put(rt::ByteBuffer::zero(6)).put(rt::ByteBuffer::zero(6));
   plain.flip();

   iv = rt::ByteBuffer::zero(16);
   rt::ByteBuffer encResp = picc.aes.encrypt(plain, iv);

   rt::ByteBuffer sesEnc(16);
   sesEnc.put(rndA.slice(0, 4)).put(rndB.slice(0, 4)).put(rndA.slice(12, 4)).put(rndB.slice(12, 4));
   sesEnc.flip();

   rt::ByteBuffer sesMac(16);
   sesMac.put(rndA.slice(4, 4)).put(rndB.slice(4, 4)).put(rndA.slice(8, 4)).put(rndB.slice(8, 4));
   sesMac.flip();

   picc.auth.valid = true;
   picc.auth.ti = ti;
   picc.auth.rndA = rndA;
   picc.auth.sessionEncKey = sesEnc;
   picc.auth.sessionMacKey = sesMac;
   picc.auth.iv = rt::ByteBuffer::zero(16);

   LOG_INFO(log, "\tauth OK sector={} ti={x}", {picc.auth.sectorNo, ti.copy()});

   response.put(0x90);
   response.put(encResp);

   awaitingStep2 = false;
   return 0;
}

} // namespace hce::targets::mifareplus
