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
#include <hce/crypto/CipherAES.h>
#include <hce/crypto/CMAC.h>

#include <hce/cards/mifareplus/MifarePlus.h>

namespace hce::cards::mifareplus {

struct MifarePlus::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hce.cards.mifareplus.MifarePlus");

   Transport transport;

   bool authenticated = false;
   unsigned int authSectorNo = 0;

   rt::ByteBuffer sessionEncKey;
   rt::ByteBuffer sessionMacKey;
   rt::ByteBuffer ti;
   rt::ByteBuffer iv;

   crypto::CipherAES aes;

   explicit Impl(Transport t) : transport(std::move(t))
   {
   }

   rt::ByteBuffer computeMac(const rt::ByteBuffer &input)
   {
      rt::ByteBuffer zeroIv = rt::ByteBuffer::zero(16);
      return crypto::CMAC::cmac(sessionMacKey, input, zeroIv, crypto::CMAC::CmacAES128).slice(0, 8);
   }

   rt::ByteBuffer encryptValue(int32_t value)
   {
      rt::ByteBuffer plain = rt::ByteBuffer::zero(16);
      unsigned char *d = plain.data();
      d[0] = static_cast<unsigned char>(value & 0xFF);
      d[1] = static_cast<unsigned char>((value >> 8) & 0xFF);
      d[2] = static_cast<unsigned char>((value >> 16) & 0xFF);
      d[3] = static_cast<unsigned char>((value >> 24) & 0xFF);

      aes.init(sessionEncKey, 0);
      rt::ByteBuffer encIv = rt::ByteBuffer::zero(16);
      return aes.encrypt(plain, encIv);
   }

   int authenticate(unsigned int sector, KeyType keyType, const rt::ByteBuffer &key)
   {
      authenticated = false;

      const unsigned int keyBlockNo = sector * 2 + static_cast<unsigned int>(keyType);

      rt::ByteBuffer step1(3);
      step1.put(static_cast<unsigned char>(0x70))
           .put(static_cast<unsigned char>(keyBlockNo & 0xFF))
           .put(static_cast<unsigned char>((keyBlockNo >> 8) & 0xFF));
      step1.flip();

      rt::ByteBuffer resp1(64);
      const int sw1 = transport(step1, resp1);

      if (sw1 != STATUS_OK) return sw1;

      if (resp1.remaining() < 17) return STATUS_ERR_LENGTH;

      resp1.skip(1);
      rt::ByteBuffer encRndB = resp1.getBuffer(16);

      aes.init(key, 0);
      rt::ByteBuffer zeroIv = rt::ByteBuffer::zero(16);
      rt::ByteBuffer rndB = aes.decrypt(encRndB, zeroIv);

      rt::ByteBuffer rndA = rt::ByteBuffer::random(16);
      rt::ByteBuffer rndBr = rt::ByteBuffer::rotateBytes(rndB, rt::ByteBuffer::Left);

      rt::ByteBuffer plain(32);
      plain.put(rndA).put(rndBr).flip();

      aes.init(key, 0);
      rt::ByteBuffer zeroIv2 = rt::ByteBuffer::zero(16);
      rt::ByteBuffer enc32 = aes.encrypt(plain, zeroIv2);

      rt::ByteBuffer step2(33);
      step2.put(static_cast<unsigned char>(0x72)).put(enc32).flip();

      rt::ByteBuffer resp2(64);
      const int sw2 = transport(step2, resp2);

      if (sw2 != STATUS_OK) return sw2;

      if (resp2.remaining() < 33) return STATUS_ERR_LENGTH;

      resp2.skip(1);
      rt::ByteBuffer enc32resp = resp2.getBuffer(32);

      aes.init(key, 0);
      rt::ByteBuffer zeroIv3 = rt::ByteBuffer::zero(16);
      rt::ByteBuffer plain32 = aes.decrypt(enc32resp, zeroIv3);

      rt::ByteBuffer tiRecv = plain32.slice(0, 4);
      rt::ByteBuffer rndAr = plain32.slice(4, 16);

      if (rndAr != rt::ByteBuffer::rotateBytes(rndA, rt::ByteBuffer::Left))
         return STATUS_ERR_AUTH;

      rt::ByteBuffer sesEnc(16);
      sesEnc.put(rndA.slice(0, 4)).put(rndB.slice(0, 4)).put(rndA.slice(12, 4)).put(rndB.slice(12, 4)).flip();

      rt::ByteBuffer sesMac(16);
      sesMac.put(rndA.slice(4, 4)).put(rndB.slice(4, 4)).put(rndA.slice(8, 4)).put(rndB.slice(8, 4)).flip();

      ti = tiRecv.copy();
      sessionEncKey = sesEnc;
      sessionMacKey = sesMac;
      iv = rt::ByteBuffer::zero(16);
      authSectorNo = sector;
      authenticated = true;

      return STATUS_OK;
   }

   int authenticateFollowing(unsigned int sector, KeyType keyType, const rt::ByteBuffer &key)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      const unsigned int keyBlockNo = sector * 2 + static_cast<unsigned int>(keyType);

      rt::ByteBuffer step1(3);
      step1.put(static_cast<unsigned char>(0x76))
           .put(static_cast<unsigned char>(keyBlockNo & 0xFF))
           .put(static_cast<unsigned char>((keyBlockNo >> 8) & 0xFF));
      step1.flip();

      rt::ByteBuffer resp1(64);
      const int sw1 = transport(step1, resp1);

      if (sw1 != STATUS_OK) return sw1;

      if (resp1.remaining() < 17) return STATUS_ERR_LENGTH;

      resp1.skip(1);
      rt::ByteBuffer encRndB = resp1.getBuffer(16);

      aes.init(key, 0);
      rt::ByteBuffer zeroIv = rt::ByteBuffer::zero(16);
      rt::ByteBuffer rndB = aes.decrypt(encRndB, zeroIv);

      rt::ByteBuffer rndA = rt::ByteBuffer::random(16);
      rt::ByteBuffer rndBr = rt::ByteBuffer::rotateBytes(rndB, rt::ByteBuffer::Left);

      rt::ByteBuffer plain(32);
      plain.put(rndA).put(rndBr).flip();

      aes.init(key, 0);
      rt::ByteBuffer zeroIv2 = rt::ByteBuffer::zero(16);
      rt::ByteBuffer enc32 = aes.encrypt(plain, zeroIv2);

      rt::ByteBuffer step2(33);
      step2.put(static_cast<unsigned char>(0x77)).put(enc32).flip();

      rt::ByteBuffer resp2(64);
      const int sw2 = transport(step2, resp2);

      if (sw2 != STATUS_OK) return sw2;

      if (resp2.remaining() < 33) return STATUS_ERR_LENGTH;

      resp2.skip(1);
      rt::ByteBuffer enc32resp = resp2.getBuffer(32);

      aes.init(key, 0);
      rt::ByteBuffer zeroIv3 = rt::ByteBuffer::zero(16);
      rt::ByteBuffer plain32 = aes.decrypt(enc32resp, zeroIv3);

      rt::ByteBuffer tiRecv = plain32.slice(0, 4);
      rt::ByteBuffer rndAr = plain32.slice(4, 16);

      if (rndAr != rt::ByteBuffer::rotateBytes(rndA, rt::ByteBuffer::Left))
         return STATUS_ERR_AUTH;

      rt::ByteBuffer sesEnc(16);
      sesEnc.put(rndA.slice(0, 4)).put(rndB.slice(0, 4)).put(rndA.slice(12, 4)).put(rndB.slice(12, 4)).flip();

      rt::ByteBuffer sesMac(16);
      sesMac.put(rndA.slice(4, 4)).put(rndB.slice(4, 4)).put(rndA.slice(8, 4)).put(rndB.slice(8, 4)).flip();

      ti = tiRecv.copy();
      sessionEncKey = sesEnc;
      sessionMacKey = sesMac;
      iv = rt::ByteBuffer::zero(16);
      authSectorNo = sector;

      return STATUS_OK;
   }

   int resetAuthentication()
   {
      rt::ByteBuffer req(1);
      req.put(static_cast<unsigned char>(0x78)).flip();

      rt::ByteBuffer resp(8);
      const int sw = transport(req, resp);

      authenticated = false;

      return sw;
   }

   int readBlock(unsigned int blockAddr, unsigned int count, rt::ByteBuffer &data)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer req(3);
      req.put(static_cast<unsigned char>(0x30))
         .put(static_cast<unsigned char>(blockAddr))
         .put(static_cast<unsigned char>(count));
      req.flip();

      rt::ByteBuffer resp(count * 16 + 1 + 8 + 8);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < static_cast<int>(1 + count * 16 + 8)) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer respData = resp.getBuffer(count * 16);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer macInput(4 + 1 + 1 + count * 16);
      macInput.put(ti)
              .put(static_cast<unsigned char>(blockAddr))
              .put(static_cast<unsigned char>(count))
              .put(respData);
      macInput.flip();

      rt::ByteBuffer expectedMac = computeMac(macInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      data.put(respData);

      return STATUS_OK;
   }

   int readBlockEncrypted(unsigned int blockAddr, unsigned int count, rt::ByteBuffer &data)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer req(3);
      req.put(static_cast<unsigned char>(0x31))
         .put(static_cast<unsigned char>(blockAddr))
         .put(static_cast<unsigned char>(count));
      req.flip();

      rt::ByteBuffer resp(count * 16 + 1 + 8 + 8);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < static_cast<int>(1 + count * 16 + 8)) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer encData = resp.getBuffer(count * 16);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer macInput(4 + 1 + 1 + count * 16);
      macInput.put(ti)
              .put(static_cast<unsigned char>(blockAddr))
              .put(static_cast<unsigned char>(count))
              .put(encData);
      macInput.flip();

      rt::ByteBuffer expectedMac = computeMac(macInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      aes.init(sessionEncKey, 0);
      rt::ByteBuffer decIv = rt::ByteBuffer::zero(16);
      rt::ByteBuffer plain = aes.decrypt(encData, decIv);
      data.put(plain);

      return STATUS_OK;
   }

   int writeBlock(unsigned int blockAddr, const rt::ByteBuffer &blockData)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer macInput(4 + 1 + 16);
      macInput.put(ti)
              .put(static_cast<unsigned char>(blockAddr))
              .put(blockData);
      macInput.flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(26);
      req.put(static_cast<unsigned char>(0xA0))
         .put(static_cast<unsigned char>(blockAddr))
         .put(blockData)
         .put(mac);
      req.flip();

      rt::ByteBuffer resp(16);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 9) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(5);
      respInput.put(ti).put(static_cast<unsigned char>(0x90)).flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      return STATUS_OK;
   }

   int writeBlockEncrypted(unsigned int blockAddr, const rt::ByteBuffer &blockData)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      aes.init(sessionEncKey, 0);
      rt::ByteBuffer encIv = rt::ByteBuffer::zero(16);
      rt::ByteBuffer encData = aes.encrypt(blockData, encIv);

      rt::ByteBuffer macInput(4 + 1 + 16);
      macInput.put(ti)
              .put(static_cast<unsigned char>(blockAddr))
              .put(encData);
      macInput.flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(26);
      req.put(static_cast<unsigned char>(0xA1))
         .put(static_cast<unsigned char>(blockAddr))
         .put(encData)
         .put(mac);
      req.flip();

      rt::ByteBuffer resp(16);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 9) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(5);
      respInput.put(ti).put(static_cast<unsigned char>(0x90)).flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      return STATUS_OK;
   }

   int increment(unsigned int blockAddr, int32_t value)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer encValue = encryptValue(value);

      rt::ByteBuffer macInput(4 + 1 + 16);
      macInput.put(ti)
              .put(static_cast<unsigned char>(blockAddr))
              .put(encValue);
      macInput.flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(26);
      req.put(static_cast<unsigned char>(0xC0))
         .put(static_cast<unsigned char>(blockAddr))
         .put(encValue)
         .put(mac);
      req.flip();

      rt::ByteBuffer resp(16);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 9) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(5);
      respInput.put(ti).put(static_cast<unsigned char>(0x90)).flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      return STATUS_OK;
   }

   int decrement(unsigned int blockAddr, int32_t value)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer encValue = encryptValue(value);

      rt::ByteBuffer macInput(4 + 1 + 16);
      macInput.put(ti)
              .put(static_cast<unsigned char>(blockAddr))
              .put(encValue);
      macInput.flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(26);
      req.put(static_cast<unsigned char>(0xC1))
         .put(static_cast<unsigned char>(blockAddr))
         .put(encValue)
         .put(mac);
      req.flip();

      rt::ByteBuffer resp(16);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 9) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(5);
      respInput.put(ti).put(static_cast<unsigned char>(0x90)).flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      return STATUS_OK;
   }

   int restore(unsigned int blockAddr)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer macInput(5);
      macInput.put(ti)
              .put(static_cast<unsigned char>(blockAddr));
      macInput.flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(10);
      req.put(static_cast<unsigned char>(0xC2))
         .put(static_cast<unsigned char>(blockAddr))
         .put(mac);
      req.flip();

      rt::ByteBuffer resp(16);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 9) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(5);
      respInput.put(ti).put(static_cast<unsigned char>(0x90)).flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      return STATUS_OK;
   }

   int transfer(unsigned int blockAddr)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer macInput(5);
      macInput.put(ti)
              .put(static_cast<unsigned char>(blockAddr));
      macInput.flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(10);
      req.put(static_cast<unsigned char>(0xB0))
         .put(static_cast<unsigned char>(blockAddr))
         .put(mac);
      req.flip();

      rt::ByteBuffer resp(16);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 9) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(5);
      respInput.put(ti).put(static_cast<unsigned char>(0x90)).flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      return STATUS_OK;
   }

   int incrementTransfer(unsigned int srcAddr, unsigned int dstAddr, int32_t value)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer encValue = encryptValue(value);

      rt::ByteBuffer macInput(4 + 1 + 1 + 16);
      macInput.put(ti)
              .put(static_cast<unsigned char>(srcAddr))
              .put(static_cast<unsigned char>(dstAddr))
              .put(encValue);
      macInput.flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(27);
      req.put(static_cast<unsigned char>(0x35))
         .put(static_cast<unsigned char>(srcAddr))
         .put(static_cast<unsigned char>(dstAddr))
         .put(encValue)
         .put(mac);
      req.flip();

      rt::ByteBuffer resp(16);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 9) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(5);
      respInput.put(ti).put(static_cast<unsigned char>(0x90)).flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      return STATUS_OK;
   }

   int decrementTransfer(unsigned int srcAddr, unsigned int dstAddr, int32_t value)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer encValue = encryptValue(value);

      rt::ByteBuffer macInput(4 + 1 + 1 + 16);
      macInput.put(ti)
              .put(static_cast<unsigned char>(srcAddr))
              .put(static_cast<unsigned char>(dstAddr))
              .put(encValue);
      macInput.flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(27);
      req.put(static_cast<unsigned char>(0x36))
         .put(static_cast<unsigned char>(srcAddr))
         .put(static_cast<unsigned char>(dstAddr))
         .put(encValue)
         .put(mac);
      req.flip();

      rt::ByteBuffer resp(16);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 9) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(5);
      respInput.put(ti).put(static_cast<unsigned char>(0x90)).flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      return STATUS_OK;
   }

   int restoreTransfer(unsigned int srcAddr, unsigned int dstAddr)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer macInput(6);
      macInput.put(ti)
              .put(static_cast<unsigned char>(srcAddr))
              .put(static_cast<unsigned char>(dstAddr));
      macInput.flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(11);
      req.put(static_cast<unsigned char>(0x37))
         .put(static_cast<unsigned char>(srcAddr))
         .put(static_cast<unsigned char>(dstAddr))
         .put(mac);
      req.flip();

      rt::ByteBuffer resp(16);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 9) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(5);
      respInput.put(ti).put(static_cast<unsigned char>(0x90)).flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      return STATUS_OK;
   }

   int getUID(rt::ByteBuffer &uid)
   {
      if (!authenticated) return STATUS_ERR_NOTAUTH;

      rt::ByteBuffer macInput(5);
      macInput.put(ti).put(static_cast<unsigned char>(0x56)).flip();

      rt::ByteBuffer mac = computeMac(macInput);

      rt::ByteBuffer req(9);
      req.put(static_cast<unsigned char>(0x56)).put(mac).flip();

      rt::ByteBuffer resp(32);
      const int sw = transport(req, resp);

      if (sw != STATUS_OK) return sw;

      if (resp.remaining() < 25) return STATUS_ERR_LENGTH;

      resp.skip(1);
      const rt::ByteBuffer encUID = resp.getBuffer(16);
      const rt::ByteBuffer rxMac = resp.getBuffer(8);

      rt::ByteBuffer respInput(4 + 1 + 16);
      respInput.put(ti)
               .put(static_cast<unsigned char>(0x90))
               .put(encUID);
      respInput.flip();

      rt::ByteBuffer expectedMac = computeMac(respInput);

      if (rxMac != expectedMac) return STATUS_ERR_AUTH;

      aes.init(sessionEncKey, 0);
      rt::ByteBuffer decIv = rt::ByteBuffer::zero(16);
      rt::ByteBuffer plain = aes.decrypt(encUID, decIv);
      uid.put(plain);

      return STATUS_OK;
   }
};

// --- MifarePlus public methods ---

MifarePlus::MifarePlus(Transport transport) : impl(std::make_unique<Impl>(std::move(transport)))
{
}

MifarePlus::~MifarePlus() = default;

int MifarePlus::authenticate(unsigned int sector, KeyType keyType, const rt::ByteBuffer &key)
{
   return impl->authenticate(sector, keyType, key);
}

int MifarePlus::authenticateFollowing(unsigned int sector, KeyType keyType, const rt::ByteBuffer &key)
{
   return impl->authenticateFollowing(sector, keyType, key);
}

int MifarePlus::resetAuthentication()
{
   return impl->resetAuthentication();
}

int MifarePlus::readBlock(unsigned int blockAddr, unsigned int count, rt::ByteBuffer &data)
{
   return impl->readBlock(blockAddr, count, data);
}

int MifarePlus::readBlockEncrypted(unsigned int blockAddr, unsigned int count, rt::ByteBuffer &data)
{
   return impl->readBlockEncrypted(blockAddr, count, data);
}

int MifarePlus::writeBlock(unsigned int blockAddr, const rt::ByteBuffer &data)
{
   return impl->writeBlock(blockAddr, data);
}

int MifarePlus::writeBlockEncrypted(unsigned int blockAddr, const rt::ByteBuffer &data)
{
   return impl->writeBlockEncrypted(blockAddr, data);
}

int MifarePlus::increment(unsigned int blockAddr, int32_t value)
{
   return impl->increment(blockAddr, value);
}

int MifarePlus::decrement(unsigned int blockAddr, int32_t value)
{
   return impl->decrement(blockAddr, value);
}

int MifarePlus::restore(unsigned int blockAddr)
{
   return impl->restore(blockAddr);
}

int MifarePlus::transfer(unsigned int blockAddr)
{
   return impl->transfer(blockAddr);
}

int MifarePlus::incrementTransfer(unsigned int srcAddr, unsigned int dstAddr, int32_t value)
{
   return impl->incrementTransfer(srcAddr, dstAddr, value);
}

int MifarePlus::decrementTransfer(unsigned int srcAddr, unsigned int dstAddr, int32_t value)
{
   return impl->decrementTransfer(srcAddr, dstAddr, value);
}

int MifarePlus::restoreTransfer(unsigned int srcAddr, unsigned int dstAddr)
{
   return impl->restoreTransfer(srcAddr, dstAddr);
}

int MifarePlus::getUID(rt::ByteBuffer &uid)
{
   return impl->getUID(uid);
}

bool MifarePlus::isAuthenticated() const
{
   return impl->authenticated;
}

unsigned int MifarePlus::authSector() const
{
   return impl->authSectorNo;
}

} // namespace hce::cards::mifareplus
