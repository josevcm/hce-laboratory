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

#include <filesystem>
#include <hce/crypto/CipherAES.h>
#include <hce/crypto/CipherDES.h>
#include <hce/crypto/CMAC.h>
#include <hce/crc/CRC.h>

#include <hce/cards/desfire/Desfire.h>

namespace hce::cards::desfire {

constexpr unsigned int maxDataPerApdu = 54;

struct Desfire::Impl
{
   rt::Logger *log = rt::Logger::getLogger("hce.cards.desfire.Desfire");

   Transport transport;

   AuthMode currentAuthMode = AUTH_NONE;
   int currentAuthKey = -1;

   rt::ByteBuffer sessionKey;
   rt::ByteBuffer sessionIv;

   crypto::CipherAES aes;
   crypto::CipherDES des;
   crypto::Cipher *cipher = nullptr;

   explicit Impl(Transport t) : transport(std::move(t))
   {
   }

   /*
    * Computes response MAC/CMAC according to the active authentication mode.
    */
   rt::ByteBuffer cmac(const int status, const rt::ByteBuffer &data)
   {
      rt::ByteBuffer temp(data.remaining() + 8);
      temp.put(data);

      switch (currentAuthMode)
      {
         case AUTH_LEGACY:
         {
            temp.padding(0, 8).flip();
            return cipher->encrypt(temp).slice(-8, 4);
         }
         case AUTH_ISO:
         {
            temp.put(status).flip();
            sessionIv = crypto::CMAC::cmac(sessionKey, temp, sessionIv, crypto::CMAC::CmacTDES);
            return sessionIv.copy();
         }
         case AUTH_AES:
         {
            temp.put(status).flip();
            sessionIv = crypto::CMAC::cmac(sessionKey, temp, sessionIv, crypto::CMAC::CmacAES128);
            return sessionIv.slice(0, 8);
         }

         default:
            return rt::ByteBuffer::empty();
      }
   }

   /**
    * Updates IV using CMAC
    */
   void updateIv(const int cmd, const rt::ByteBuffer &info = rt::ByteBuffer::empty(), const rt::ByteBuffer &data = rt::ByteBuffer::empty())
   {
      // Buffer for CMAC (cmd + info + data)
      rt::ByteBuffer tmp(1 + info.remaining() + data.remaining());
      tmp.put(static_cast<unsigned char>(cmd)).put(info).put(data).flip();

      // Compute CMAC according to authentication mode
      switch (currentAuthMode)
      {
         case AUTH_ISO:
            sessionIv = crypto::CMAC::cmac(sessionKey, tmp, sessionIv, crypto::CMAC::CmacTDES);
            break;

         case AUTH_AES:
            sessionIv = crypto::CMAC::cmac(sessionKey, tmp, sessionIv, crypto::CMAC::CmacAES128);
            break;

         default:
            break;
      }
   }

   /**
    * Encrypts data in legacy mode (EV0) with CRC16
    */
   rt::ByteBuffer encryptDataEV0(const rt::ByteBuffer &input, const unsigned int length) const
   {
      // Encryption buffer (data + CRC + padding)
      rt::ByteBuffer tmp(input.remaining() + 2 + 8);

      // Append data
      tmp.put(input);

      // Append ISO14443A CRC16
      tmp.putInt(crc::CRC::iso14443A(input), 2);

      // Append 0x80 when length is zero
      if (length == 0)
         tmp.put(0x80);

      // Add padding until 8-byte alignment
      tmp.padding(0x00, 8);
      tmp.flip();

      // Encrypt data (CBC mode avoid update IV)
      rt::ByteBuffer copyIv = sessionIv.copy();
      return cipher->encrypt(tmp, copyIv);
   }

   /**
    * Decrypts data in legacy mode (EV0) with CRC16
    */
   int decryptDataEV0(const rt::ByteBuffer &input, rt::ByteBuffer &output, const unsigned int length)
   {
      // Decrypt data (CBC mode avoid update IV)
      rt::ByteBuffer copyIv = sessionIv.copy();
      rt::ByteBuffer plain = cipher->decrypt(input, copyIv);

      // For unknown length, trim decrypted data up to 0x80 marker.
      if (length == 0)
      {
         while (plain.remaining() > 0)
         {
            if (const unsigned char b = plain.popInt(1); b == 0x80)
               break;
         }
      }
      else
      {
         plain.trim(plain.remaining() - length - 2);
      }

      // Verify CRC
      if (const unsigned short check = plain.popInt(2); check != crc::CRC::iso14443A(plain))
         return STATUS_INTEGRITY_ERROR;

      // Return decrypted data
      output.put(plain);

      return STATUS_OK;
   }

   /**
    * Encrypts data in standard mode (ISO/AES) with CRC32
    * CRC32 is computed over: cmd + info (parameters) + data
    * Encryption is applied to: data + CRC32 + padding
    */
   rt::ByteBuffer encryptDataEV1(const int cmd, const rt::ByteBuffer &params, const rt::ByteBuffer &input, const unsigned int length)
   {
      // Select block size by algorithm
      const unsigned int blockSize = (currentAuthMode == AUTH_AES) ? 16 : 8;

      // Buffer for CRC32 computation (cmd + info + data)
      rt::ByteBuffer crcBuffer(1 + params.remaining() + input.remaining());
      crcBuffer.put(static_cast<unsigned char>(cmd)).put(params).put(input).flip();

      // Compute CRC32
      const unsigned int crc = crc::CRC::ccitt32(crcBuffer);

      // Encryption buffer (data + CRC + padding)
      rt::ByteBuffer tmp(input.remaining() + 4 + blockSize);

      // Append data
      tmp.put(input);

      // Append CRC32
      tmp.putInt(crc, 4);

      // Append 0x80 when length is zero
      if (length == 0)
         tmp.put(0x80);

      // Add padding up to block size
      tmp.padding(0x00, blockSize);
      tmp.flip();

      // Encrypt data (chained IV)
      return cipher->encrypt(tmp, sessionIv);
   }

   /**
    * Decrypts EV1 protected data (ISO/AES) and verifies CRC32.
    */
   int decryptDataEV1(int status, const rt::ByteBuffer &params, const rt::ByteBuffer &input, rt::ByteBuffer &output, unsigned int length)
   {
      // save last ciphertext block before decryption for IV update
      rt::ByteBuffer lastIv = input.slice(input.remaining() - sessionIv.capacity(), sessionIv.capacity());

      // Decrypt with current sessionIv (correct CBC initial vector)
      rt::ByteBuffer plain = cipher->decrypt(input, sessionIv);

      // update IV to last ciphertext block
      sessionIv = lastIv;

      // for unknown length, trim data up to 0x80
      if (length == 0)
      {
         while (plain.remaining() > 0)
         {
            if (const unsigned char b = plain.popInt(1); b == 0x80)
               break;
         }
      }
      else
      {
         plain.trim(plain.remaining() - length - 4);
      }

      // pop CRC32 from end of decrypted buffer
      const unsigned int check = plain.popInt(4);

      // Verify CRC over [data + STATUS_OK(0x00)] matching server encryptDataEV1
      rt::ByteBuffer crcBuffer(plain.remaining() + 1);
      crcBuffer.put(plain);
      crcBuffer.put(0x00);
      crcBuffer.flip();

      if (check != crc::CRC::ccitt32(crcBuffer))
         return STATUS_INTEGRITY_ERROR;

      // Return decrypted data
      output.put(plain);

      return STATUS_OK;
   }

   /**
    * Encodes outbound payload according to communication mode (PLAIN, MACING or CRYPT).
    */
   int encodeData(int cmd, CommMode mode, const rt::ByteBuffer &params, const rt::ByteBuffer &data, rt::ByteBuffer &output, const unsigned int length)
   {
      switch (mode)
      {
         case COMM_PLAIN:
         {
            encodeDataPlain(cmd, params, data, output);
            break;
         }

         case COMM_MACING:
         {
            if (encodeDataMacing(cmd, params, data, output) != STATUS_OK)
               return STATUS_INTEGRITY_ERROR;
            break;
         }

         case COMM_CRYPT:
         {
            if (encodeDataCrypt(cmd, params, data, output, length) != STATUS_OK)
               return STATUS_INTEGRITY_ERROR;
            break;
         }

         default:
            LOG_ERROR(log, "unknown communication mode={}", {mode});
            return STATUS_PARAMETER_ERROR;
      }

      return STATUS_OK;
   }

   /**
    * Decode raw data from card and process it according to communication mode (PLAIN, MACING or CRYPT)
    */
   int decodeData(const rt::ByteBuffer &params, const rt::ByteBuffer &input, CommMode mode, rt::ByteBuffer &output, const unsigned int length)
   {
      // Process data based on communication mode
      switch (mode)
      {
         case COMM_PLAIN:
         {
            if (const int status = decodeDataPlain(STATUS_OK, input, output, length); status != STATUS_OK)
               return status;

            break;
         }

         case COMM_MACING:
         {
            if (const int status = decodeDataMacing(STATUS_OK, input, output, length); status != STATUS_OK)
               return status;

            break;
         }

         case COMM_CRYPT:
         {
            if (const int status = decodeDataCrypt(STATUS_OK, params, input, output, length); status != STATUS_OK)
               return status;

            break;
         }

         default:
            LOG_ERROR(log, "unknown communication mode={}", {mode});
            return STATUS_PARAMETER_ERROR;
      }

      return STATUS_OK;
   }

   /**
    * Processes plain responses; when authenticated, validates trailing MAC/CMAC.
    */
   int encodeDataPlain(const int cmd, const rt::ByteBuffer &info, const rt::ByteBuffer &data, rt::ByteBuffer &output)
   {
      output.put(data);

      return STATUS_OK;
   }

   /**
    * Processes plain responses; when authenticated, validates trailing MAC/CMAC.
    */
   int decodeDataPlain(const int status, const rt::ByteBuffer &data, rt::ByteBuffer &output, const unsigned int length)
   {
      output.put(data, length == 0 ? data.remaining() : length);

      return STATUS_OK;
   }

   /**
    * Appends MAC/CMAC to outbound payload.
    */
   int encodeDataMacing(const int cmd, const rt::ByteBuffer &info, const rt::ByteBuffer &data, rt::ByteBuffer &output)
   {
      if (currentAuthMode == AUTH_NONE)
      {
         LOG_ERROR(log, "MACING mode requires authentication");
         return STATUS_PERMISSION_DENIED;
      }

      // Update IV with CMAC
      updateIv(cmd, info, data);

      // Result buffer (data + MAC)
      output.put(data);
      output.put(sessionIv.slice(0, 8));

      return STATUS_OK;
   }

   /**
     * Verifies and strips MAC/CMAC from a protected response payload.
     */
   int decodeDataMacing(const int status, const rt::ByteBuffer &data, rt::ByteBuffer &output, const unsigned int length)
   {
      rt::ByteBuffer buffer = data.copy();

      switch (currentAuthMode)
      {
         case AUTH_NONE:
         {
            LOG_ERROR(log, "MACING mode requires authentication");
            return STATUS_PERMISSION_DENIED;
         }

         case AUTH_LEGACY:
         {
            if (buffer.remaining() < 4)
               return STATUS_INTEGRITY_ERROR;

            if (const rt::ByteBuffer receivedCmac = buffer.popBuffer(4); receivedCmac != cmac(status, buffer))
               return STATUS_INTEGRITY_ERROR;

            break;
         }

         case AUTH_ISO:
         case AUTH_AES:
         {
            if (buffer.remaining() < 8)
               return STATUS_INTEGRITY_ERROR;

            if (const rt::ByteBuffer receivedCmac = buffer.popBuffer(8); receivedCmac != cmac(status, buffer))
               return STATUS_INTEGRITY_ERROR;

            break;
         }
      }

      output.put(buffer);

      return STATUS_OK;
   }

   /**
    * Prepares encrypted data to send (legacy mode with CRC16 or standard mode with CRC32)
    */
   int encodeDataCrypt(const int cmd, const rt::ByteBuffer &params, const rt::ByteBuffer &data, rt::ByteBuffer &output, const unsigned int length)
   {
      switch (currentAuthMode)
      {
         case AUTH_NONE:
         {
            LOG_ERROR(log, "CRYPT mode requires authentication");
            return STATUS_PERMISSION_DENIED;
         }

         case AUTH_LEGACY:
            output.put(encryptDataEV0(data, length));
            break;

         default:
            output.put(encryptDataEV1(cmd, params, data, length));
      }

      return STATUS_OK;
   }

   /**
    * Decrypts received data (legacy mode with CRC16 or standard mode with CRC32)
    */
   int decodeDataCrypt(int status, const rt::ByteBuffer &params, const rt::ByteBuffer &input, rt::ByteBuffer &output, const unsigned int length)
   {
      switch (currentAuthMode)
      {
         case AUTH_NONE:
         {
            LOG_ERROR(log, "CRYPT mode requires authentication");
            return STATUS_PERMISSION_DENIED;
         }

         case AUTH_LEGACY:
            return decryptDataEV0(input, output, length);

         default:
            return decryptDataEV1(status, params, input, output, length);
      }
   }

   /**
    * Sends chained APDUs for long payloads; accumulates all response data chunks.
    */
   int sendChaining(int ins, const rt::ByteBuffer &payload, rt::ByteBuffer &accum) const
   {
      rt::ByteBuffer buffer = payload.copy();

      int status;

      do
      {
         // get next block to send
         rt::ByteBuffer block = buffer.getBuffer(std::min(maxDataPerApdu, buffer.remaining()));

         // send next block and check for data chaining
         if (status = sendWrapped(ins, block, accum); status != STATUS_OK && status != STATUS_ADDITIONAL_FRAME)
         {
            LOG_DEBUG(log, "sendWrapped -> status=0x{02x}", {status});
            return status;
         }

         ins = INS_CONTINUE;
      }
      while (status == STATUS_ADDITIONAL_FRAME && buffer.remaining() > 0);

      return STATUS_OK;
   }

   /**
    * Builds one native wrapped APDU and returns native status byte (or ISO SW for non-native responses).
    */
   int sendWrapped(const int ins, const rt::ByteBuffer &data, rt::ByteBuffer &accum) const
   {
      const unsigned int cmdSize = data.remaining() > 0 ? 6 + data.remaining() : 5;

      rt::ByteBuffer cmd(cmdSize);
      cmd.putInt(CLA_WRAPPER, 1);
      cmd.putInt(ins, 1);
      cmd.putInt(0x00, 1);
      cmd.putInt(0x00, 1);

      if (data.remaining() > 0)
      {
         cmd.putInt(data.remaining(), 1);
         cmd.put(data);
      }

      cmd.putInt(0x00, 1); // Le = 256
      cmd.flip();

      const int sw = sendCommand(cmd, accum);

      return (sw >> 8) == DESFIRE_SW1 ? (sw & 0xFF) : sw;
   }

   /**
    * Sends a raw APDU through transport and accumulates response payload bytes (without SW1/SW2).
    */
   int sendCommand(const rt::ByteBuffer &cmd, rt::ByteBuffer &accum) const
   {
      rt::ByteBuffer resp(256);

      LOG_TRACE(log, ">> 0x{x}", {cmd.copy()});

      const int sw = transport(cmd, resp);

      LOG_TRACE(log, "<< 0x{x}", {resp.copy()});

      // Append data bytes (all except the last 2 SW bytes) to the accumulator.
      if (resp.remaining() > 2)
      {
         resp.popInt(2);
         accum.put(resp);
      }

      return sw;
   }
};

/* ------------------------------------------------------------------ */
/* Constructor / destructor                                            */
/* ------------------------------------------------------------------ */

/**
 * Creates a DESFire client bound to a transport callback.
 * @param transport Callable that sends APDUs and returns SW.
 */
Desfire::Desfire(Transport transport) : impl(std::make_unique<Impl>(std::move(transport)))
{
}

/**
 * Destroys the DESFire client instance.
 */
Desfire::~Desfire() = default;

/* ------------------------------------------------------------------ */
/* PICC Level Commands                                                 */
/* ------------------------------------------------------------------ */

/**
 * Reads card version information (HW, SW and production fields).
 * @param info Output structure filled with parsed version data.
 * @return Native DESFire status word.
 */
int Desfire::getVersionInfo(VersionInfo &info)
{
   LOG_DEBUG(impl->log, "getVersionInfo()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_GET_VERSION, rt::ByteBuffer::empty());

   rt::ByteBuffer resp(64);

   int status = impl->sendWrapped(INS_GET_VERSION, rt::ByteBuffer::empty(), resp);

   while (status == STATUS_ADDITIONAL_FRAME)
      status = impl->sendWrapped(INS_CONTINUE, rt::ByteBuffer::empty(), resp);

   if (status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "getVersionInfo -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   if (resp.remaining() >= 7)
   {
      info.hwVendorId = resp.getInt(1);
      info.hwType = resp.getInt(1);
      info.hwSubtype = resp.getInt(1);
      info.hwMajorVersion = resp.getInt(1);
      info.hwMinorVersion = resp.getInt(1);
      info.hwStorageSize = resp.getInt(1);
      info.hwProtocol = resp.getInt(1);
   }

   if (resp.remaining() >= 7)
   {
      info.swVendorId = resp.getInt(1);
      info.swType = resp.getInt(1);
      info.swSubtype = resp.getInt(1);
      info.swMajorVersion = resp.getInt(1);
      info.swMinorVersion = resp.getInt(1);
      info.swStorageSize = resp.getInt(1);
      info.swProtocol = resp.getInt(1);
   }

   if (resp.remaining() >= 14)
   {
      info.uid = resp.getBuffer(7);
      info.batch = resp.getBuffer(5);

      unsigned int week = resp.getInt(1);
      unsigned int year = resp.getInt(1);

      // Both fields are BCD-encoded
      info.productionWeek = ((week >> 4) & 0xF) * 10 + (week & 0xF);
      info.productionYear = ((year >> 4) & 0xF) * 10 + (year & 0xF) + 2000;
   }

   LOG_DEBUG(impl->log, "getVersionInfo -> hw={}.{} sw={}.{} uid={x} batch={x} week={} year={}", {info.hwMajorVersion, info.hwMinorVersion, info.swMajorVersion, info.swMinorVersion, info.uid, info.batch, info.productionWeek, info.productionYear});

   return STATUS_OK;
}

/**
 * Reads current free memory from the card.
 * @param freeMemory Output free memory in bytes.
 * @return Native DESFire status word.
 */
int Desfire::getFreeMemory(unsigned int &freeMemory)
{
   LOG_DEBUG(impl->log, "getFreeMemory()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_GET_FREE_MEMORY, rt::ByteBuffer::empty());

   rt::ByteBuffer resp(8);

   if (const int status = impl->sendWrapped(INS_GET_FREE_MEMORY, rt::ByteBuffer::empty(), resp); status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "getFreeMemory -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   freeMemory = resp.remaining() >= 3 ? resp.getInt(3) : 0;

   LOG_DEBUG(impl->log, "getFreeMemory -> freeMemory={}", {freeMemory});

   return STATUS_OK;
}

/**
 * Formats the PICC.
 * @return Native DESFire status word.
 */
int Desfire::formatCard()
{
   LOG_DEBUG(impl->log, "formatCard()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_GET_FREE_MEMORY, rt::ByteBuffer::empty());

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_FORMAT_PICC, rt::ByteBuffer::empty(), resp);

   LOG_DEBUG(impl->log, "formatCard -> status=0x{02x}", {status});

   return status;
}

/**
 * Creates an application.
 * @param appId Application AID (3 bytes).
 * @param keySettings1 Key settings byte 1.
 * @param keySettings2 Key settings byte 2.
 * @return Native DESFire status word.
 */
int Desfire::createApplication(unsigned int appId, unsigned int keySettings1, unsigned int keySettings2)
{
   LOG_DEBUG(impl->log, "createApplication(appId=0x{06x} ks1=0x{02x} ks2=0x{02x})", {appId, keySettings1, keySettings2});

   rt::ByteBuffer params(5);
   params.putInt(appId, 3);
   params.putInt(keySettings1, 1);
   params.putInt(keySettings2, 1);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_CREATE_APPLICATION, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CREATE_APPLICATION, params, resp);

   LOG_DEBUG(impl->log, "createApplication -> status=0x{02x}", {status});

   return status;
}

/**
 * Creates an ISO-enabled application.
 * @param appId Application AID (3 bytes).
 * @param keySettings1 Key settings byte 1.
 * @param keySettings2 Key settings byte 2.
 * @param isoId ISO DF identifier.
 * @param isoName ISO DF name.
 * @return Native DESFire status word.
 */
int Desfire::createApplicationIso(unsigned int appId, unsigned int keySettings1, unsigned int keySettings2, unsigned int isoId, const rt::ByteBuffer &isoName)
{
   LOG_DEBUG(impl->log, "createApplicationIso(appId=0x{06x} ks1=0x{02x} ks2=0x{02x} isoId=0x{04x} isoName={x})", {appId, keySettings1, keySettings2, isoId, isoName});

   const unsigned int nameLen = isoName.remaining();
   rt::ByteBuffer params(7 + nameLen);
   params.putInt(appId, 3);
   params.putInt(keySettings1, 1);
   params.putInt(keySettings2, 1);
   params.putInt(isoId, 2);
   params.put(isoName);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_CREATE_APPLICATION, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CREATE_APPLICATION, params, resp);

   LOG_DEBUG(impl->log, "createApplicationIso -> status=0x{02x}", {status});

   return status;
}

/**
 * Deletes an application by AID.
 * @param appId Application AID (3 bytes).
 * @return Native DESFire status word.
 */
int Desfire::deleteApplication(unsigned int appId)
{
   LOG_DEBUG(impl->log, "deleteApplication(appId=0x{06x})", {appId});

   rt::ByteBuffer params(3);
   params.putInt(appId, 3);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_DELETE_APPLICATION, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_DELETE_APPLICATION, params, resp);

   LOG_DEBUG(impl->log, "deleteApplication -> status=0x{02x}", {status});

   return status;
}

/**
 * Selects an application and resets authentication state on success.
 * @param appId Application AID (3 bytes).
 * @return Native DESFire status word.
 */
int Desfire::selectApplication(unsigned int appId)
{
   LOG_DEBUG(impl->log, "selectApplication(appId=0x{06x})", {appId});

   rt::ByteBuffer params(3);
   params.putInt(appId, 3);
   params.flip();

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_SELECT_APPLICATION, params, resp);

   if (status == STATUS_OK)
   {
      impl->currentAuthMode = AUTH_NONE;
      impl->currentAuthKey = -1;
   }

   LOG_DEBUG(impl->log, "selectApplication -> status=0x{02x}", {status});

   return status;
}

/**
 * Lists card applications.
 * @param appIds Output list of AIDs.
 * @return Native DESFire status word.
 */
int Desfire::listApplications(std::vector<unsigned int> &appIds)
{
   LOG_DEBUG(impl->log, "listApplications()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_LIST_APPLICATIONS, rt::ByteBuffer::empty());

   rt::ByteBuffer resp(128);

   int status = impl->sendWrapped(INS_LIST_APPLICATIONS, rt::ByteBuffer::empty(), resp);

   while (status == STATUS_ADDITIONAL_FRAME)
      status = impl->sendWrapped(INS_CONTINUE, rt::ByteBuffer::empty(), resp);

   if (status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "listApplications -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   while (resp.remaining() >= 3)
      appIds.push_back(resp.getInt(3));

   LOG_DEBUG(impl->log, "listApplications -> count={}", {static_cast<int>(appIds.size())});

   return STATUS_OK;
}

/**
 * Lists ISO DF names.
 * @param dfs Output list of returned DF name frames.
 * @return Native DESFire status word.
 */
int Desfire::listDFNames(std::vector<rt::ByteBuffer> &dfs)
{
   LOG_DEBUG(impl->log, "listDFNames()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_LIST_APPLICATIONS, rt::ByteBuffer::empty());

   rt::ByteBuffer resp(256);

   int sw = impl->sendWrapped(INS_LIST_DF_NAMES, rt::ByteBuffer::empty(), resp);

   resp.flip();

   if (resp.remaining() > 0)
      dfs.push_back(resp);

   while (sw == STATUS_ADDITIONAL_FRAME)
   {
      rt::ByteBuffer next(256);

      sw = impl->sendWrapped(INS_CONTINUE, rt::ByteBuffer::empty(), next);

      next.flip();

      if (next.remaining() > 0)
         dfs.push_back(next);
   }

   LOG_DEBUG(impl->log, "listDFNames -> count={} sw=0x{04x}", {static_cast<int>(dfs.size()), sw});

   return sw;
}

/**
 * Reads card UID and decrypts it when a session cipher is active.
 * @param uid Output UID bytes.
 * @return Native DESFire status word.
 */
int Desfire::getCardUID(rt::ByteBuffer &uid)
{
   LOG_DEBUG(impl->log, "getCardUID()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_GET_CARD_UID, rt::ByteBuffer::empty());

   rt::ByteBuffer resp(32);

   if (const int status = impl->sendWrapped(INS_GET_CARD_UID, rt::ByteBuffer::empty(), resp); status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "getCardUID -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   if (impl->cipher != nullptr && resp.remaining() >= 7)
   {
      rt::ByteBuffer plain = impl->cipher->decrypt(resp, impl->sessionIv);
      uid = plain.getBuffer(7);
   }

   LOG_DEBUG(impl->log, "getCardUID -> uid={x}", {uid});

   return STATUS_OK;
}

/* ------------------------------------------------------------------ */
/* Security Commands                                                   */
/* ------------------------------------------------------------------ */

/**
 * Performs legacy mutual authentication.
 * @param keyId Key number.
 * @param keyData Key material.
 * @return Native DESFire status word.
 */
int Desfire::authenticateLegacy(unsigned int keyId, const rt::ByteBuffer &keyData)
{
   LOG_DEBUG(impl->log, "authenticateLegacy(keyId={} keySize={})", {keyId, keyData.remaining()});

   // Step 1: send key number, card responds with ek(RndB)
   rt::ByteBuffer keyParam(1);
   keyParam.putInt(keyId, 1);
   keyParam.flip();

   rt::ByteBuffer step1Data(32);

   int status = impl->sendWrapped(INS_AUTHENTICATE_LEGACY, keyParam, step1Data);

   if (status != STATUS_ADDITIONAL_FRAME)
   {
      LOG_DEBUG(impl->log, "authenticateLegacy -> status=0x{02x}", {status});
      return status;
   }

   step1Data.flip(); // 8 bytes: ek(RndB)

   // Init cipher in Legacy (ECB-like) mode
   impl->des.init(keyData, crypto::CipherDES::CBCRecv);

   // Decrypt ek(RndB) -> RndB
   rt::ByteBuffer rndB = impl->des.decrypt(step1Data);

   // Generate our RndA
   rt::ByteBuffer rndA = rt::ByteBuffer::random(8);

   // Rotate RndB left -> RndBr (what the server sent us was RndB; server expects RndBr)
   rt::ByteBuffer rndBr = rt::ByteBuffer::rotateBytes(rndB, rt::ByteBuffer::Left);

   // Build 16-byte plaintext: RndA || RndBr
   rt::ByteBuffer plainPayload(16);
   plainPayload.put(rndA);
   plainPayload.put(rndBr);
   plainPayload.flip();

   // Encrypt RndA || RndBr (Legacy mode, no IV)
   rt::ByteBuffer encPayload = impl->des.encrypt(plainPayload);

   // Step 2: send INS_CONTINUE with encrypted payload
   rt::ByteBuffer step2Data(32);

   status = impl->sendWrapped(INS_CONTINUE, encPayload, step2Data);

   if (status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "authenticateLegacy -> status=0x{02x}", {status});
      return status;
   }

   step2Data.flip(); // 8 bytes: ek(RndAr)

   // Decrypt ek(RndAr) -> RndAr
   rt::ByteBuffer rndAr = impl->des.decrypt(step2Data);

   // Rotate RndAr right -> should equal our RndA
   rt::ByteBuffer rndACheck = rt::ByteBuffer::rotateBytes(rndAr, rt::ByteBuffer::Right);

   if (rndACheck != rndA)
   {
      LOG_DEBUG(impl->log, "authenticateLegacy -> RndA mismatch, sw=0x{04x}", {STATUS_AUTHENTICATION_ERROR});
      return STATUS_AUTHENTICATION_ERROR;
   }

   // Build session key (depends on key length)
   rt::ByteBuffer sessionKey(16);
   sessionKey.put(rndA.slice(0, 4));
   sessionKey.put(rndB.slice(0, 4));

   if (keyData.remaining() == 8)
   {
      // Single DES: repeat first 8 bytes
      sessionKey.put(rndA.slice(0, 4));
      sessionKey.put(rndB.slice(0, 4));
   }
   else
   {
      // 2K3DES: check if both halves identical
      if (keyData.slice(0, 8) == keyData.slice(8, 8))
      {
         sessionKey.put(rndA.slice(0, 4));
         sessionKey.put(rndB.slice(0, 4));
      }
      else
      {
         sessionKey.put(rndA.slice(4, 4));
         sessionKey.put(rndB.slice(4, 4));
      }
   }

   sessionKey.flip();

   impl->currentAuthMode = AUTH_LEGACY;
   impl->currentAuthKey = static_cast<int>(keyId);
   impl->sessionKey = sessionKey;
   impl->sessionIv = rt::ByteBuffer::zero(8);
   impl->des.init(sessionKey, crypto::CipherDES::CBCSend);
   impl->cipher = &impl->des;

   LOG_DEBUG(impl->log, "authenticateLegacy -> AUTH_LEGACY keyId={} sessionKey={x}", {keyId, sessionKey.copy()});

   return STATUS_OK;
}

/**
 * Performs ISO mutual authentication.
 * @param keyId Key number.
 * @param keyData Key material.
 * @return Native DESFire status word.
 */
int Desfire::authenticateISO(unsigned int keyId, const rt::ByteBuffer &keyData)
{
   LOG_DEBUG(impl->log, "authenticateISO(keyId={} keySize={})", {keyId, keyData.remaining()});

   // 3K3DES (24-byte key) uses 16-byte randoms; DES/2K3DES use 8-byte randoms
   unsigned int rndSize = (keyData.remaining() == 24) ? 16 : 8;

   // Step 1: send key number, card responds with ek(RndB)
   rt::ByteBuffer keyParam(1);
   keyParam.putInt(keyId, 1);
   keyParam.flip();

   rt::ByteBuffer step1Data(32);

   int status = impl->sendWrapped(INS_AUTHENTICATE_ISO, keyParam, step1Data);

   if (status != STATUS_ADDITIONAL_FRAME)
   {
      LOG_DEBUG(impl->log, "authenticateISO -> status=0x{02x}", {status});
      return status;
   }

   step1Data.flip();

   // Init DES in ISO (CBC) mode, IV = zeros(8)
   impl->des.init(keyData, crypto::CipherDES::Iso);
   impl->sessionIv = rt::ByteBuffer::zero(8);

   // Decrypt ek(RndB) -> RndB (IV chained)
   rt::ByteBuffer rndB = impl->des.decrypt(step1Data, impl->sessionIv);

   // Generate RndA and rotate RndB left -> RndBr
   rt::ByteBuffer rndA = rt::ByteBuffer::random(rndSize);
   rt::ByteBuffer rndBr = rt::ByteBuffer::rotateBytes(rndB, rt::ByteBuffer::Left);

   // Build plaintext RndA || RndBr and encrypt (IV chained)
   rt::ByteBuffer plainPayload(rndSize * 2);
   plainPayload.put(rndA);
   plainPayload.put(rndBr);
   plainPayload.flip();

   rt::ByteBuffer encPayload = impl->des.encrypt(plainPayload, impl->sessionIv);

   // Step 2: send INS_CONTINUE, card responds with ek(RndAr)
   rt::ByteBuffer step2Data(32);

   status = impl->sendWrapped(INS_CONTINUE, encPayload, step2Data);

   if (status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "authenticateISO -> status=0x{02x}", {status});
      return status;
   }

   step2Data.flip();

   // Decrypt ek(RndAr) and verify
   rt::ByteBuffer rndAr = impl->des.decrypt(step2Data, impl->sessionIv);

   if (rt::ByteBuffer::rotateBytes(rndAr, rt::ByteBuffer::Right) != rndA)
   {
      LOG_DEBUG(impl->log, "authenticateISO -> RndA mismatch, sw=0x{04x}", {STATUS_AUTHENTICATION_ERROR});
      return STATUS_AUTHENTICATION_ERROR;
   }

   // Build session key
   rt::ByteBuffer sessionKey;

   switch (keyData.remaining())
   {
      case 8:
      {
         sessionKey = rt::ByteBuffer(16);
         sessionKey.put(rndA.slice(0, 4));
         sessionKey.put(rndB.slice(0, 4));
         sessionKey.put(rndA.slice(0, 4));
         sessionKey.put(rndB.slice(0, 4));
         break;
      }
      case 16:
      {
         sessionKey = rt::ByteBuffer(16);
         sessionKey.put(rndA.slice(0, 4));
         sessionKey.put(rndB.slice(0, 4));

         if (keyData.slice(0, 8) == keyData.slice(8, 8))
         {
            sessionKey.put(rndA.slice(0, 4));
            sessionKey.put(rndB.slice(0, 4));
         }
         else
         {
            sessionKey.put(rndA.slice(4, 4));
            sessionKey.put(rndB.slice(4, 4));
         }
         break;
      }
      case 24:
      {
         sessionKey = rt::ByteBuffer(24);
         sessionKey.put(rndA.slice(0, 4));
         sessionKey.put(rndB.slice(0, 4));
         sessionKey.put(rndA.slice(6, 4));
         sessionKey.put(rndB.slice(6, 4));
         sessionKey.put(rndA.slice(12, 4));
         sessionKey.put(rndB.slice(12, 4));
         break;
      }
      default:
         LOG_DEBUG(impl->log, "authenticateISO -> invalid key size={}", {keyData.remaining()});
         return STATUS_AUTHENTICATION_ERROR;
   }

   sessionKey.flip();

   impl->currentAuthMode = AUTH_ISO;
   impl->currentAuthKey = static_cast<int>(keyId);
   impl->sessionKey = sessionKey;
   impl->sessionIv = rt::ByteBuffer::zero(8);
   impl->des.init(sessionKey, crypto::CipherDES::Iso);
   impl->cipher = &impl->des;

   LOG_DEBUG(impl->log, "authenticateISO -> AUTH_ISO keyId={} sessionKey={x}", {keyId, sessionKey.copy()});

   return STATUS_OK;
}

/**
 * Performs AES mutual authentication.
 * @param keyId Key number.
 * @param keyData AES key material.
 * @return Native DESFire status word.
 */
int Desfire::authenticateAES(unsigned int keyId, const rt::ByteBuffer &keyData)
{
   LOG_DEBUG(impl->log, "authenticateAES(keyId={} keySize={})", {keyId, keyData.remaining()});

   // Step 1: send key number, card responds with ek(RndB)
   rt::ByteBuffer keyParam(1);
   keyParam.putInt(keyId, 1);
   keyParam.flip();

   rt::ByteBuffer step1Data(32);

   int status = impl->sendWrapped(INS_AUTHENTICATE_AES, keyParam, step1Data);

   if (status != STATUS_ADDITIONAL_FRAME)
   {
      LOG_DEBUG(impl->log, "authenticateAES -> status=0x{02x}", {status});
      return status;
   }

   step1Data.flip(); // 16 bytes: ek(RndB)

   // Init AES with provided key; start IV at zero
   impl->aes.init(keyData, 0);
   impl->sessionIv = rt::ByteBuffer::zero(16);

   // Decrypt ek(RndB) -> RndB (CBC, IV updated in-place)
   rt::ByteBuffer rndB = impl->aes.decrypt(step1Data, impl->sessionIv);

   // Generate our RndA
   rt::ByteBuffer rndA = rt::ByteBuffer::random(16);

   // Rotate RndB left -> RndBr (server expects RndBr to reconstruct RndB)
   rt::ByteBuffer rndBr = rt::ByteBuffer::rotateBytes(rndB, rt::ByteBuffer::Left);

   // Build 32-byte plaintext: RndA || RndBr
   rt::ByteBuffer plainPayload(32);
   plainPayload.put(rndA);
   plainPayload.put(rndBr);
   plainPayload.flip();

   // Encrypt with current sessionIv (chained from step 1 decrypt)
   rt::ByteBuffer encPayload = impl->aes.encrypt(plainPayload, impl->sessionIv);

   // Step 2: send INS_CONTINUE with 32-byte encrypted payload
   rt::ByteBuffer step2Data(32);

   status = impl->sendWrapped(INS_CONTINUE, encPayload, step2Data);

   if (status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "authenticateAES -> status=0x{02x}", {status});
      return status;
   }

   step2Data.flip(); // 16 bytes: ek(RndAr)

   // Decrypt ek(RndAr) -> RndAr (CBC, IV still chained)
   rt::ByteBuffer rndAr = impl->aes.decrypt(step2Data, impl->sessionIv);

   // Rotate RndAr right -> should equal our RndA
   rt::ByteBuffer rndACheck = rt::ByteBuffer::rotateBytes(rndAr, rt::ByteBuffer::Right);

   if (rndACheck != rndA)
   {
      LOG_DEBUG(impl->log, "authenticateAES -> RndA mismatch, sw=0x{04x}", {STATUS_AUTHENTICATION_ERROR});
      return STATUS_AUTHENTICATION_ERROR;
   }

   // Session key: RndA[0..3] | RndB[0..3] | RndA[12..15] | RndB[12..15]
   rt::ByteBuffer sessionKey(16);
   sessionKey.put(rndA.slice(0, 4));
   sessionKey.put(rndB.slice(0, 4));
   sessionKey.put(rndA.slice(12, 4));
   sessionKey.put(rndB.slice(12, 4));
   sessionKey.flip();

   impl->currentAuthMode = AUTH_AES;
   impl->currentAuthKey = static_cast<int>(keyId);
   impl->sessionKey = sessionKey;
   impl->sessionIv = rt::ByteBuffer::zero(16);
   impl->aes.init(sessionKey, 0);
   impl->cipher = &impl->aes;

   LOG_DEBUG(impl->log, "authenticateAES -> AUTH_AES keyId={} sessionKey={x}", {keyId, sessionKey.copy()});

   return STATUS_OK;
}

/**
 * Reads current key settings and key count.
 * @param keySettings Output key settings byte.
 * @param numKeys Output number of keys.
 * @return Native DESFire status word.
 */
int Desfire::getKeySettings(unsigned int &keySettings, unsigned int &numKeys)
{
   LOG_DEBUG(impl->log, "getKeySettings()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_GET_KEY_SETTINGS, rt::ByteBuffer::empty());

   rt::ByteBuffer resp(8);

   if (const int status = impl->sendWrapped(INS_GET_KEY_SETTINGS, rt::ByteBuffer::empty(), resp); status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "getKeySettings -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   if (resp.remaining() >= 2)
   {
      keySettings = resp.getInt(1);
      numKeys = resp.getInt(1);
   }

   LOG_DEBUG(impl->log, "getKeySettings -> keySettings=0x{02x} numKeys={}", {keySettings, numKeys});

   return STATUS_OK;
}

/**
 * Reads version byte for a key.
 * @param keyId Key number.
 * @param keyVersion Output key version.
 * @return Native DESFire status word.
 */
int Desfire::getKeyVersion(unsigned int keyId, unsigned int &keyVersion)
{
   LOG_DEBUG(impl->log, "getKeyVersion(keyId={})", {keyId});

   rt::ByteBuffer params(1);
   params.putInt(keyId, 1);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_GET_KEY_VERSION, params);

   rt::ByteBuffer resp(8);

   if (int status = impl->sendWrapped(INS_GET_KEY_VERSION, params, resp); status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "getKeyVersion -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   keyVersion = resp.remaining() >= 1 ? resp.getInt(1) : 0;

   LOG_DEBUG(impl->log, "getKeyVersion -> keyVersion=0x{02x}", {keyVersion});

   return STATUS_OK;
}

/**
 * Changes a key in the selected application/PICC.
 * @param keyNo Key number to change.
 * @param newKey New key value.
 * @param oldKey Old key value (used when required by protocol).
 * @return Native DESFire status word.
 */
int Desfire::changeKey(unsigned int keyNo, const rt::ByteBuffer &newKey, const rt::ByteBuffer &oldKey)
{
   LOG_DEBUG(impl->log, "changeKey(keyNo={} newKeySize={} oldKeySize={})", {keyNo, newKey.remaining(), oldKey.remaining()});

   // Determine key type from auth mode and new key size
   unsigned int keyType;

   if (impl->currentAuthMode == AUTH_AES)
      keyType = 2;
   else if (newKey.remaining() == 24)
      keyType = 1;
   else
      keyType = 0;

   bool sameKey = static_cast<int>(keyNo) == impl->currentAuthKey;
   unsigned int keyId = (keyType << 6) | keyNo;
   unsigned int keySize = newKey.remaining();

   rt::ByteBuffer plain(64);

   if (impl->currentAuthMode == AUTH_LEGACY)
   {
      // EV0: 24-byte payload, CRC16, Legacy cipher (no IV)
      if (sameKey)
      {
         rt::ByteBuffer crcBuf(keySize);
         crcBuf.put(newKey);
         crcBuf.flip();

         plain.put(newKey);
         plain.putInt(crc::CRC::iso14443A(crcBuf), 2);
      }
      else
      {
         // XOR new key with old key, then two CRC16s
         rt::ByteBuffer xorKey = newKey ^ oldKey;

         plain.put(xorKey);
         plain.putInt(crc::CRC::iso14443A(xorKey), 2);
         plain.putInt(crc::CRC::iso14443A(newKey), 2);
      }

      plain.padding(0x00, 8);
      plain.flip();
   }
   else
   {
      // EV1: CBC encrypt, CRC32
      bool isAes = (impl->currentAuthMode == AUTH_AES);

      if (sameKey)
      {
         // CRC32 over: INS | keyId_byte | newKey | [version=0 for AES]
         unsigned int crcBufSize = 2 + keySize + (isAes ? 1 : 0);

         rt::ByteBuffer crcBuf(crcBufSize);
         crcBuf.putInt(INS_CHANGE_KEY, 1);
         crcBuf.putInt(keyId, 1);
         crcBuf.put(newKey);

         if (isAes)
            crcBuf.putInt(0, 1);

         crcBuf.flip();

         unsigned int crc = crc::CRC::ccitt32(crcBuf);

         plain.put(newKey);

         if (isAes)
            plain.putInt(0, 1);

         plain.putInt(crc, 4);
         plain.padding(0x00, isAes ? 16 : 8);
         plain.flip();
      }
      else
      {
         // XOR new key with old key
         rt::ByteBuffer xorKey = newKey ^ oldKey;

         // CRC32 over: INS | keyNo (without type bits) | xorKey | [version]
         rt::ByteBuffer crcBuf(2 + keySize + (isAes ? 1 : 0));
         crcBuf.put(INS_CHANGE_KEY);
         crcBuf.put(keyNo);
         crcBuf.put(xorKey);

         if (isAes)
            crcBuf.putInt(0, 1);

         crcBuf.flip();

         unsigned int crc1 = crc::CRC::ccitt32(crcBuf);
         unsigned int crc2 = crc::CRC::ccitt32(newKey);

         plain.put(xorKey);

         if (isAes)
            plain.putInt(0, 1);

         plain.putInt(crc1, 4);
         plain.putInt(crc2, 4);
         plain.padding(0x00, isAes ? 16 : 8);
         plain.flip();
      }
   }

   rt::ByteBuffer enc = impl->cipher->encrypt(plain, impl->sessionIv);

   rt::ByteBuffer params(1 + enc.remaining());
   params.putInt(keyId, 1);
   params.put(enc);
   params.flip();

   rt::ByteBuffer data(8);

   const int status = impl->sendWrapped(INS_CHANGE_KEY, params, data);

   LOG_DEBUG(impl->log, "changeKey -> status=0x{02x}", {status});

   return status;
}

/**
 * Changes master/application key settings.
 * @param newSettings New key settings byte.
 * @return Native DESFire status word.
 */
int Desfire::changeKeySettings(unsigned int newSettings)
{
   LOG_DEBUG(impl->log, "changeKeySettings(newSettings=0x{02x})", {newSettings});

   // EV0 (Legacy): CRC16 over data, Legacy cipher. EV1 (ISO/AES): CRC32 over INS|data, CBC cipher.
   const bool isLegacy = impl->currentAuthMode == AUTH_LEGACY;
   const bool isAes = impl->currentAuthMode == AUTH_AES;

   rt::ByteBuffer plain(16);
   plain.putInt(newSettings, 1);

   if (isLegacy)
   {
      rt::ByteBuffer crcBuf(1);
      crcBuf.putInt(newSettings, 1);
      crcBuf.flip();

      plain.putInt(crc::CRC::iso14443A(crcBuf), 2);
   }
   else
   {
      rt::ByteBuffer crcBuf(2);
      crcBuf.putInt(INS_CHANGE_KEY_SETTINGS, 1);
      crcBuf.putInt(newSettings, 1);
      crcBuf.flip();

      plain.putInt(crc::CRC::ccitt32(crcBuf), 4);
   }

   plain.padding(0x00, isAes ? 16 : 8);
   plain.flip();

   const rt::ByteBuffer enc = impl->cipher->encrypt(plain, impl->sessionIv);

   rt::ByteBuffer data(8);

   const int status = impl->sendWrapped(INS_CHANGE_KEY_SETTINGS, enc, data);

   LOG_DEBUG(impl->log, "changeKeySettings -> status=0x{02x}", {status});

   return status;
}

/* ------------------------------------------------------------------ */
/* Application-Level File Management                                   */
/* ------------------------------------------------------------------ */

/**
 * Lists file identifiers in the selected application.
 * @param fileIds Output file IDs.
 * @return Native DESFire status word.
 */
int Desfire::listFiles(std::vector<unsigned int> &fileIds)
{
   LOG_DEBUG(impl->log, "listFiles()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_LIST_FILES, rt::ByteBuffer::empty());

   rt::ByteBuffer resp(32);

   if (int status = impl->sendWrapped(INS_LIST_FILES, rt::ByteBuffer::empty(), resp); status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "listFiles -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   while (resp.remaining() >= 1)
      fileIds.push_back(resp.getInt(1));

   LOG_DEBUG(impl->log, "listFiles -> count={}", {(int)fileIds.size()});

   return STATUS_OK;
}

/**
 * Lists ISO file IDs in the selected directory.
 * @param ids Output ISO file IDs.
 * @return Native DESFire status word.
 */
int Desfire::getIsoFileIDs(std::vector<unsigned int> &ids)
{
   LOG_DEBUG(impl->log, "getIsoFileIDs()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_GET_ISO_FILE_IDS, rt::ByteBuffer::empty());

   rt::ByteBuffer resp(128);

   int status = impl->sendWrapped(INS_GET_ISO_FILE_IDS, rt::ByteBuffer::empty(), resp);

   while (status == STATUS_ADDITIONAL_FRAME)
      status = impl->sendWrapped(INS_CONTINUE, rt::ByteBuffer::empty(), resp);

   if (status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "getIsoFileIDs -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   while (resp.remaining() >= 2)
      ids.push_back(resp.getInt(2));

   LOG_DEBUG(impl->log, "getIsoFileIDs -> count={}", {static_cast<int>(ids.size())});

   return STATUS_OK;
}

/**
 * Reads settings for a specific file.
 * @param fileId File identifier.
 * @param settings Output parsed settings.
 * @return Native DESFire status word.
 */
int Desfire::getFileSettings(unsigned int fileId, FileSettings &settings)
{
   LOG_DEBUG(impl->log, "getFileSettings(fileId=0x{02x})", {fileId});

   rt::ByteBuffer params(1);
   params.putInt(fileId, 1);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_GET_FILE_SETTINGS, params);

   rt::ByteBuffer resp(32);

   if (const int status = impl->sendWrapped(INS_GET_FILE_SETTINGS, params, resp); status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "getFileSettings -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   if (resp.remaining() < 4)
      return STATUS_INTEGRITY_ERROR;

   settings.fileType = resp.getInt(1);
   settings.commSettings = resp.getInt(1);
   settings.accessRights = resp.getInt(2);

   switch (settings.fileType)
   {
      case FILE_STANDARD:
      case FILE_BACKUP:
      {
         settings.fileSize = resp.remaining() >= 3 ? resp.getInt(3) : 0;
         LOG_DEBUG(impl->log, "getFileSettings -> type={} comm={} access=0x{04x} size={}", {settings.fileType, settings.commSettings, settings.accessRights, settings.fileSize});
         break;
      }

      case FILE_VALUE:
      {
         if (resp.remaining() >= 13)
         {
            settings.lowerLimit = static_cast<int>(resp.getInt(4));
            settings.upperLimit = static_cast<int>(resp.getInt(4));
            settings.limitedCreditValue = static_cast<int>(resp.getInt(4));
            settings.limitedCreditEnabled = resp.getInt(1) != 0;
         }

         LOG_DEBUG(impl->log, "getFileSettings -> type=VALUE comm={} access=0x{04x} lo={} hi={} limitedCredit={}", {settings.commSettings, settings.accessRights, settings.lowerLimit, settings.upperLimit, (int)settings.limitedCreditEnabled});
         break;
      }

      case FILE_LINEAR_RECORD:
      case FILE_CYCLIC_RECORD:
      {
         if (resp.remaining() >= 9)
         {
            settings.recordSize = resp.getInt(3);
            settings.maxRecords = resp.getInt(3);
            settings.currentRecords = resp.getInt(3);
         }
         LOG_DEBUG(impl->log, "getFileSettings -> type={} comm={} access=0x{04x} recSize={} maxRec={} curRec={}", {settings.fileType, settings.commSettings, settings.accessRights, settings.recordSize, settings.maxRecords, settings.currentRecords});
         break;
      }

      default:

         LOG_DEBUG(impl->log, "getFileSettings -> type={} comm={} access=0x{04x}", {settings.fileType, settings.commSettings, settings.accessRights});
         break;
   }

   return STATUS_OK;
}

/**
 * Changes communication and access settings of a file.
 * @param fileId File identifier.
 * @param commSettings Communication settings.
 * @param readKey Read access key number.
 * @param writeKey Write access key number.
 * @param readWriteKey Read/Write access key number.
 * @param changeKey Change access key number.
 * @return Native DESFire status word.
 */
int Desfire::changeFileSettings(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey)
{
   unsigned int accessRights = ((readKey & 0xF) << 12) | ((writeKey & 0xF) << 8) | ((readWriteKey & 0xF) << 4) | (changeKey & 0xF);

   LOG_DEBUG(impl->log, "changeFileSettings(fileId=0x{02x} comm=0x{02x} access=0x{04x})", {fileId, commSettings, accessRights});

   // read file settings to check ChangeAccessRights flag
   FileSettings fileSettings;

   if (const int sw = getFileSettings(fileId, fileSettings); sw != STATUS_OK)
      return sw;

   rt::ByteBuffer params(1 + 16);

   // if change rights key is set to FREE, send command as plain
   if (fileSettings.changeRightsKey() == 0x0E)
   {
      params.putInt(fileId, 1);
      params.putInt(commSettings, 1);
      params.putInt(accessRights, 2);
   }
   else
   {
      bool isAes = impl->currentAuthMode == AUTH_AES;
      bool isLegacy = impl->currentAuthMode == AUTH_LEGACY;

      rt::ByteBuffer plain(isAes ? 16 : 8);
      plain.putInt(commSettings, 1);
      plain.putInt(accessRights, 2);

      if (isLegacy)
      {
         rt::ByteBuffer crcBuf(3);
         crcBuf.putInt(commSettings, 1);
         crcBuf.putInt(accessRights, 2);
         crcBuf.flip();
         plain.putInt(crc::CRC::iso14443A(plain), 2);
      }
      else
      {
         rt::ByteBuffer crcBuf(5);
         crcBuf.putInt(INS_CHANGE_FILE_SETTINGS, 1);
         crcBuf.putInt(fileId, 1);
         crcBuf.putInt(commSettings, 1);
         crcBuf.putInt(accessRights, 2);
         crcBuf.flip();
         plain.putInt(crc::CRC::ccitt32(crcBuf), 4);
      }

      plain.padding(0x00, isAes ? 16 : 8);
      plain.flip();

      rt::ByteBuffer enc = impl->cipher->encrypt(plain, impl->sessionIv);
      params.putInt(fileId, 1);
      params.put(enc);
   }

   params.flip();

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CHANGE_FILE_SETTINGS, params, resp);

   LOG_DEBUG(impl->log, "changeFileSettings -> status=0x{02x}", {status});

   return status;
}

/**
 * Creates a standard data file.
 * @param fileId File identifier.
 * @param commSettings Communication settings.
 * @param readKey Read access key number.
 * @param writeKey Write access key number.
 * @param readWriteKey Read/Write access key number.
 * @param changeKey Change access key number.
 * @param fileSize File size in bytes.
 * @return Native DESFire status word.
 */
int Desfire::createStandardFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int fileSize)
{
   unsigned int accessRights = ((readKey & 0xF) << 12) | ((writeKey & 0xF) << 8) | ((readWriteKey & 0xF) << 4) | (changeKey & 0xF);

   LOG_DEBUG(impl->log, "createStandardFile(fileId=0x{02x} comm=0x{02x} access=0x{04x} size={})", {fileId, commSettings, accessRights, fileSize});

   rt::ByteBuffer params(7);
   params.putInt(fileId, 1);
   params.putInt(commSettings, 1);
   params.putInt(accessRights, 2);
   params.putInt(fileSize, 3);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_CREATE_STD_FILE, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CREATE_STD_FILE, params, resp);

   LOG_DEBUG(impl->log, "createStandardFile -> status=0x{02x}", {status});

   return status;
}

/**
 * Creates an ISO-mapped standard data file.
 * @param fileId Native file identifier.
 * @param isoFileId ISO file identifier.
 * @param commSettings Communication settings.
 * @param readKey Read access key number.
 * @param writeKey Write access key number.
 * @param readWriteKey Read/Write access key number.
 * @param changeKey Change access key number.
 * @param fileSize File size in bytes.
 * @return Native DESFire status word.
 */
int Desfire::createStandardFileIso(unsigned int fileId, unsigned int isoFileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int fileSize)
{
   unsigned int accessRights = ((readKey & 0xF) << 12) | ((writeKey & 0xF) << 8) | ((readWriteKey & 0xF) << 4) | (changeKey & 0xF);

   LOG_DEBUG(impl->log, "createStandardFileIso(fileId=0x{02x} isoFileId=0x{04x} comm=0x{02x} access=0x{04x} size={})", {fileId, isoFileId, commSettings, accessRights, fileSize});

   rt::ByteBuffer params(32);
   params.putInt(fileId, 1);
   params.putInt(isoFileId, 2);
   params.putInt(commSettings, 1);
   params.putInt(accessRights, 2);
   params.putInt(fileSize, 3);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_CREATE_STD_FILE, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CREATE_STD_FILE, params, resp);

   LOG_DEBUG(impl->log, "createStandardFileIso -> status=0x{02x}", {status});

   return status;
}

/**
 * Creates a backup data file.
 * @param fileId File identifier.
 * @param commSettings Communication settings.
 * @param readKey Read access key number.
 * @param writeKey Write access key number.
 * @param readWriteKey Read/Write access key number.
 * @param changeKey Change access key number.
 * @param fileSize File size in bytes.
 * @return Native DESFire status word.
 */
int Desfire::createBackupFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int fileSize)
{
   unsigned int accessRights = ((readKey & 0xF) << 12) | ((writeKey & 0xF) << 8) | ((readWriteKey & 0xF) << 4) | (changeKey & 0xF);

   LOG_DEBUG(impl->log, "createBackupFile(fileId=0x{02x} comm=0x{02x} access=0x{04x} size={})", {fileId, commSettings, accessRights, fileSize});

   rt::ByteBuffer params(7);
   params.putInt(fileId, 1);
   params.putInt(commSettings, 1);
   params.putInt(accessRights, 2);
   params.putInt(fileSize, 3);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_CREATE_BACKUP_FILE, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CREATE_BACKUP_FILE, params, resp);

   LOG_DEBUG(impl->log, "createBackupFile -> status=0x{02x}", {status});

   return status;
}

/**
 * Creates a value file.
 * @param fileId File identifier.
 * @param commSettings Communication settings.
 * @param readKey Read access key number.
 * @param writeKey Write access key number.
 * @param readWriteKey Read/Write access key number.
 * @param changeKey Change access key number.
 * @param lowerLimit Minimum value.
 * @param upperLimit Maximum value.
 * @param initialValue Initial value.
 * @param limitedCredit Enables/disables limited credit.
 * @return Native DESFire status word.
 */
int Desfire::createValueFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, int lowerLimit, int upperLimit, int initialValue, bool limitedCredit)
{
   unsigned int accessRights = ((readKey & 0xF) << 12) | ((writeKey & 0xF) << 8) | ((readWriteKey & 0xF) << 4) | (changeKey & 0xF);

   LOG_DEBUG(impl->log, "createValueFile(fileId=0x{02x} comm=0x{02x} access=0x{04x} lo={} hi={} init={} limitedCredit={})", {fileId, commSettings, accessRights, lowerLimit, upperLimit, initialValue, (int)limitedCredit});

   rt::ByteBuffer params(17);
   params.putInt(fileId, 1);
   params.putInt(commSettings, 1);
   params.putInt(accessRights, 2);
   params.putInt(static_cast<unsigned int>(lowerLimit), 4);
   params.putInt(static_cast<unsigned int>(upperLimit), 4);
   params.putInt(static_cast<unsigned int>(initialValue), 4);
   params.putInt(limitedCredit ? 1 : 0, 1);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_CREATE_VALUE_FILE, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CREATE_VALUE_FILE, params, resp);

   LOG_DEBUG(impl->log, "createValueFile -> status=0x{02x}", {status});

   return status;
}

/**
 * Creates a linear record file.
 * @param fileId File identifier.
 * @param commSettings Communication settings.
 * @param readKey Read access key number.
 * @param writeKey Write access key number.
 * @param readWriteKey Read/Write access key number.
 * @param changeKey Change access key number.
 * @param recordSize Record size in bytes.
 * @param maxRecords Maximum number of records.
 * @return Native DESFire status word.
 */
int Desfire::createLinearRecordFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int recordSize, unsigned int maxRecords)
{
   unsigned int accessRights = ((readKey & 0xF) << 12) | ((writeKey & 0xF) << 8) | ((readWriteKey & 0xF) << 4) | (changeKey & 0xF);

   LOG_DEBUG(impl->log, "createLinearRecordFile(fileId=0x{02x} comm=0x{02x} access=0x{04x} recSize={} maxRec={})", {fileId, commSettings, accessRights, recordSize, maxRecords});

   rt::ByteBuffer params(10);
   params.putInt(fileId, 1);
   params.putInt(commSettings, 1);
   params.putInt(accessRights, 2);
   params.putInt(recordSize, 3);
   params.putInt(maxRecords, 3);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_CREATE_LINEAR_FILE, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CREATE_LINEAR_FILE, params, resp);

   LOG_DEBUG(impl->log, "createLinearRecordFile -> status=0x{02x}", {status});

   return status;
}

/**
 * Creates a cyclic record file.
 * @param fileId File identifier.
 * @param commSettings Communication settings.
 * @param readKey Read access key number.
 * @param writeKey Write access key number.
 * @param readWriteKey Read/Write access key number.
 * @param changeKey Change access key number.
 * @param recordSize Record size in bytes.
 * @param maxRecords Maximum number of records.
 * @return Native DESFire status word.
 */
int Desfire::createCyclicRecordFile(unsigned int fileId, unsigned int commSettings, unsigned int readKey, unsigned int writeKey, unsigned int readWriteKey, unsigned int changeKey, unsigned int recordSize, unsigned int maxRecords)
{
   unsigned int accessRights = ((readKey & 0xF) << 12) | ((writeKey & 0xF) << 8) | ((readWriteKey & 0xF) << 4) | (changeKey & 0xF);

   LOG_DEBUG(impl->log, "createCyclicRecordFile(fileId=0x{02x} comm=0x{02x} access=0x{04x} recSize={} maxRec={})", {fileId, commSettings, accessRights, recordSize, maxRecords});

   rt::ByteBuffer params(10);
   params.putInt(fileId, 1);
   params.putInt(commSettings, 1);
   params.putInt(accessRights, 2);
   params.putInt(recordSize, 3);
   params.putInt(maxRecords, 3);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_CREATE_CYCLIC_FILE, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CREATE_CYCLIC_FILE, params, resp);

   LOG_DEBUG(impl->log, "createCyclicRecordFile -> status=0x{02x}", {status});

   return status;
}

/**
 * Deletes a file in the selected application.
 * @param fileId File identifier.
 * @return Native DESFire status word.
 */
int Desfire::deleteFile(unsigned int fileId)
{
   LOG_DEBUG(impl->log, "deleteFile(fileId=0x{02x})", {fileId});

   rt::ByteBuffer params(1);
   params.putInt(fileId, 1);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_DELETE_FILE, params);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_DELETE_FILE, params, resp);

   LOG_DEBUG(impl->log, "deleteFile -> status=0x{02x}", {status});

   return status;
}

/* ------------------------------------------------------------------ */
/* Data Manipulation Commands                                          */
/* ------------------------------------------------------------------ */

/**
 * Reads bytes from a data file.
 * @param fileId File identifier.
 * @param offset Byte offset.
 * @param length Number of bytes to read (0 means all remaining bytes).
 * @param mode Communication mode used for this operation.
 * @param data Output buffer with decoded payload.
 * @return Native DESFire status word.
 */
int Desfire::readData(unsigned int fileId, unsigned int offset, unsigned int length, CommMode mode, rt::ByteBuffer &data)
{
   LOG_DEBUG(impl->log, "readData(fileId=0x{02x} offset={} length={} mode={})", {fileId, offset, length, (int)mode});

   rt::ByteBuffer params(7);
   params.putInt(fileId, 1);
   params.putInt(offset, 3);
   params.putInt(length, 3);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_READ_DATA, params);

   rt::ByteBuffer resp(256);
   int status = impl->sendWrapped(INS_READ_DATA, params, resp);

   while (status == STATUS_ADDITIONAL_FRAME)
      status = impl->sendWrapped(INS_CONTINUE, rt::ByteBuffer::empty(), resp);

   if (status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "readData -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   // Process data based on communication mode
   if (status = impl->decodeData(params, resp, mode, data, length); status != STATUS_OK)
      return status;

   data.flip();

   LOG_DEBUG(impl->log, "readData -> bytes={}", {data.remaining()});

   return STATUS_OK;
}

/**
 * Writes bytes to a data file.
 * @param fileId File identifier.
 * @param offset Byte offset.
 * @param length Number of bytes to write.
 * @param mode Communication mode used for this operation.
 * @param data Input payload to write.
 * @return Native DESFire status word.
 */
int Desfire::writeData(unsigned int fileId, unsigned int offset, unsigned int length, CommMode mode, const rt::ByteBuffer &data)
{
   LOG_DEBUG(impl->log, "writeData(fileId=0x{02x} offset={} length={} mode={} data={x})", {fileId, offset, length, static_cast<int>(mode), data});

   // Build header: fileId (1) + offset (3) + length (3) = 7 bytes
   rt::ByteBuffer params(7);
   params.putInt(fileId, 1);
   params.putInt(offset, 3);
   params.putInt(length, 3);
   params.flip();

   // Build complete payload based on communication mode
   rt::ByteBuffer payload(params.remaining() + data.remaining() + 16); // Extra space for MAC/encryption
   payload.put(params);

   if (const int status = impl->encodeData(INS_WRITE_DATA, mode, params, data, payload, length); status != STATUS_OK)
      return status;

   payload.flip();

   rt::ByteBuffer resp(8); // Extra space for MAC/encryption

   if (int status = impl->sendChaining(INS_WRITE_DATA, payload, resp); status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "writeData -> status=0x{02x}", {status});
      return status;
   }

   LOG_DEBUG(impl->log, "writeData -> completed with chaining");

   return STATUS_OK;
}

/**
 * Reads the current value from a value file.
 * @param fileId File identifier.
 * @param mode Communication mode used for this operation.
 * @param value Output value.
 * @return Native DESFire status word.
 */
int Desfire::getValue(unsigned int fileId, CommMode mode, int &value)
{
   LOG_DEBUG(impl->log, "getValue(fileId=0x{02x} mode={})", {fileId, (int)mode});

   rt::ByteBuffer params(1);
   params.putInt(fileId, 1);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_GET_VALUE, params);

   rt::ByteBuffer resp(32);

   if (const int status = impl->sendWrapped(INS_GET_VALUE, params, resp); status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "getValue -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   rt::ByteBuffer data(32);

   // Process data based on communication mode
   if (const int status = impl->decodeData(params, resp, mode, data, 4); status != STATUS_OK)
      return status;

   data.flip();

   value = data.remaining() >= 4 ? static_cast<int>(data.getInt(4)) : 0;

   LOG_DEBUG(impl->log, "getValue -> value={}", {value});

   return STATUS_OK;
}

/**
 * Credits a value file by an amount.
 * @param fileId File identifier.
 * @param amount Amount to add.
 * @param mode Communication mode used for this operation.
 * @return Native DESFire status word.
 */
int Desfire::credit(unsigned int fileId, int amount, CommMode mode)
{
   LOG_DEBUG(impl->log, "credit(fileId=0x{02x} amount={} mode={})", {fileId, amount, (int)mode});

   rt::ByteBuffer params(1);
   params.putInt(fileId, 1);
   params.flip();

   rt::ByteBuffer data(4);
   data.putInt(static_cast<unsigned int>(amount), 4);
   data.flip();

   // Build complete payload based on communication mode
   rt::ByteBuffer payload(params.remaining() + data.remaining() + 16); // Extra space for MAC/encryption
   payload.put(params);

   if (const int status = impl->encodeData(INS_CREDIT, mode, params, data, payload, 4); status != STATUS_OK)
      return status;

   payload.flip();

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CREDIT, payload, resp);

   LOG_DEBUG(impl->log, "credit -> status=0x{02x}", {status});

   return status;
}

/**
 * Debits a value file by an amount.
 * @param fileId File identifier.
 * @param amount Amount to subtract.
 * @param mode Communication mode used for this operation.
 * @return Native DESFire status word.
 */
int Desfire::debit(unsigned int fileId, int amount, CommMode mode)
{
   LOG_DEBUG(impl->log, "debit(fileId=0x{02x} amount={} mode={})", {fileId, amount, (int)mode});

   rt::ByteBuffer params(1);
   params.putInt(fileId, 1);
   params.flip();

   rt::ByteBuffer data(4);
   data.putInt(static_cast<unsigned int>(amount), 4);
   data.flip();

   // Build complete payload based on communication mode
   rt::ByteBuffer payload(params.remaining() + data.remaining() + 16); // Extra space for MAC/encryption
   payload.put(params);

   if (const int status = impl->encodeData(INS_DEBIT, mode, params, data, payload, 4); status != STATUS_OK)
      return status;

   payload.flip();

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_DEBIT, payload, resp);

   LOG_DEBUG(impl->log, "debit -> status=0x{02x}", {status});

   return status;
}

/**
 * Applies limited credit to a value file.
 * @param fileId File identifier.
 * @param amount Amount to add.
 * @param mode Communication mode used for this operation.
 * @return Native DESFire status word.
 */
int Desfire::limitedCredit(unsigned int fileId, int amount, CommMode mode)
{
   LOG_DEBUG(impl->log, "limitedCredit(fileId=0x{02x} amount={} mode={})", {fileId, amount, (int)mode});

   rt::ByteBuffer params(1);
   params.putInt(fileId, 1);
   params.flip();

   rt::ByteBuffer data(4);
   data.putInt(static_cast<unsigned int>(amount), 4);
   data.flip();

   // Build complete payload based on communication mode
   rt::ByteBuffer payload(params.remaining() + data.remaining() + 16); // Extra space for MAC/encryption
   payload.put(params);

   if (const int status = impl->encodeData(INS_LIMITED_CREDIT, mode, params, data, payload, 4); status != STATUS_OK)
      return status;

   payload.flip();

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_LIMITED_CREDIT, payload, resp);

   LOG_DEBUG(impl->log, "limitedCredit -> status=0x{02x}", {status});

   return status;
}

/**
 * Reads records from a record file.
 * @param fileId File identifier.
 * @param offset Record offset.
 * @param count Number of records (0 means all available records).
 * @param mode Communication mode used for this operation.
 * @param data Output buffer with decoded record payload.
 * @return Native DESFire status word.
 */
int Desfire::readRecords(unsigned int fileId, unsigned int offset, unsigned int count, CommMode mode, rt::ByteBuffer &data)
{
   LOG_DEBUG(impl->log, "readRecords(fileId=0x{02x} offset={} count={} mode={})", {fileId, offset, count, (int)mode});

   rt::ByteBuffer params(7);
   params.putInt(fileId, 1);
   params.putInt(offset, 3);
   params.putInt(count, 3);
   params.flip();

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_READ_RECORDS, params);

   rt::ByteBuffer resp(1024);

   int status = impl->sendWrapped(INS_READ_RECORDS, params, resp);

   while (status == STATUS_ADDITIONAL_FRAME)
      status = impl->sendWrapped(INS_CONTINUE, rt::ByteBuffer::empty(), resp);

   if (status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "readRecords -> status=0x{02x}", {status});
      return status;
   }

   resp.flip();

   // Process data based on communication mode
   if (status = impl->decodeData(params, resp, mode, data, 0); status != STATUS_OK)
      return status;

   data.flip();

   LOG_DEBUG(impl->log, "readRecords -> bytes={}", {data.remaining()});

   return STATUS_OK;
}

/**
 * Writes record bytes into a record file.
 * @param fileId File identifier.
 * @param offset Record offset.
 * @param length Number of bytes to write.
 * @param mode Communication mode used for this operation.
 * @param data Input payload to write.
 * @return Native DESFire status word.
 */
int Desfire::writeRecord(unsigned int fileId, unsigned int offset, unsigned int length, CommMode mode, const rt::ByteBuffer &data)
{
   LOG_DEBUG(impl->log, "writeRecord(fileId=0x{02x} offset={} length={} mode={} data={x})", {fileId, offset, length, (int)mode, data});

   // Build header: fileId (1) + offset (3) + length (3) = 7 bytes
   rt::ByteBuffer params(7);
   params.putInt(fileId, 1);
   params.putInt(offset, 3);
   params.putInt(length, 3);
   params.flip();

   // Build complete payload based on communication mode
   rt::ByteBuffer payload(params.remaining() + data.remaining() + 16); // Extra space for MAC/encryption
   payload.put(params);

   if (const int status = impl->encodeData(INS_WRITE_RECORD, mode, params, data, payload, length); status != STATUS_OK)
      return status;

   payload.flip();

   rt::ByteBuffer resp(8); // Extra space for MAC/encryption

   if (int status = impl->sendChaining(INS_WRITE_RECORD, payload, resp); status != STATUS_OK)
   {
      LOG_DEBUG(impl->log, "writeRecord -> status=0x{02x}", {status});
      return status;
   }

   LOG_DEBUG(impl->log, "writeRecord -> completed with chaining");

   return STATUS_OK;
}

/**
 * Clears all records in a record file.
 * @param fileId File identifier.
 * @return Native DESFire status word.
 */
int Desfire::clearRecords(unsigned int fileId)
{
   LOG_DEBUG(impl->log, "clearRecords(fileId=0x{02x})", {fileId});

   rt::ByteBuffer params(1);
   params.putInt(fileId, 1);
   params.flip();

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_CLEAR_RECORD_FILE, params, resp);

   LOG_DEBUG(impl->log, "clearRecords -> status=0x{02x}", {status});

   return status;
}

/**
 * Commits pending transactional changes.
 * @return Native DESFire status word.
 */
int Desfire::commitTransaction()
{
   LOG_DEBUG(impl->log, "commitTransaction()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_COMMIT_TRANSACTION);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_COMMIT_TRANSACTION, rt::ByteBuffer::empty(), resp);

   LOG_DEBUG(impl->log, "commitTransaction -> status=0x{02x}", {status});

   return status;
}

/**
 * Aborts pending transactional changes.
 * @return Native DESFire status word.
 */
int Desfire::abortTransaction()
{
   LOG_DEBUG(impl->log, "abortTransaction()");

   // sync sessionIv for ISO / AES auth modes
   impl->updateIv(INS_ABORT_TRANSACTION);

   rt::ByteBuffer resp(8);

   const int status = impl->sendWrapped(INS_ABORT_TRANSACTION, rt::ByteBuffer::empty(), resp);

   LOG_DEBUG(impl->log, "abortTransaction -> status=0x{02x}", {status});

   return status;
}

/**
 * Performs ISO SELECT FILE by DF name.
 * @param isoName ISO DF name.
 * @return ISO status word.
 */
int Desfire::isoSelectByName(const rt::ByteBuffer &isoName)
{
   LOG_DEBUG(impl->log, "isoSelectByName(isoName={x})", {isoName});

   // ISO SELECT DIRECTORY BY NAME (CLA=0x00, INS=0xA4, P1=0x04 select-by-DF-name, P2=0x0C no-FCI)
   rt::ByteBuffer cmd(5 + isoName.remaining() + 1);
   cmd.putInt(CLA_ISO, 1); // 0x00
   cmd.putInt(INS_ISO_SELECT, 1); // 0xA4
   cmd.putInt(0x04, 1); // P1: select by DF name
   cmd.putInt(0x0C, 1); // P2: first or only, no FCI
   cmd.putInt(isoName.remaining(), 1); // Lc
   cmd.put(isoName); // DF name bytes
   cmd.putInt(0x00, 1); // Le
   cmd.flip();

   rt::ByteBuffer resp(64);

   const int sw = impl->sendCommand(cmd, resp);

   if (sw == ISO_STATUS_OK)
   {
      impl->currentAuthMode = AUTH_NONE;
      impl->currentAuthKey = -1;
   }

   LOG_DEBUG(impl->log, "isoSelectByName -> sw=0x{04x}", {sw});

   return sw;
}

/**
 * Performs ISO SELECT FILE by identifier (MF/DF/EF).
 * @param isoId ISO file identifier.
 * @return ISO status word.
 */
int Desfire::isoSelectById(int isoId)
{
   LOG_DEBUG(impl->log, "isoSelectById(isoId=0x{x})", {isoId});

   // ISO SELECT FILE BY ID (CLA=0x00, INS=0xA4, P1=0x00 select by identifier, P2=0x0C no-FCI)
   rt::ByteBuffer cmd(5 + 2 + 1);
   cmd.putInt(CLA_ISO, 1); // 0x00
   cmd.putInt(INS_ISO_SELECT, 1); // 0xA4
   cmd.putInt(0x00, 1); // P1: select by MF, DF or EF identifier
   cmd.putInt(0x0C, 1); // P2: first or only, no FCI
   cmd.putInt(2, 1); // Lc
   cmd.putInt(isoId, 2, rt::ByteBuffer::BigEndian); // id bytes
   cmd.putInt(0x00, 1); // Le
   cmd.flip();

   rt::ByteBuffer resp(64);

   const int sw = impl->sendCommand(cmd, resp);

   LOG_DEBUG(impl->log, "isoSelectById -> sw=0x{04x}", {sw});

   return sw;
}

/**
 * Executes ISO READ BINARY.
 * @param p1 ISO P1 byte.
 * @param p2 ISO P2 byte.
 * @param le Expected length.
 * @param data Output response payload.
 * @return ISO status word.
 */
int Desfire::isoReadBinary(unsigned int p1, unsigned int p2, unsigned int le, rt::ByteBuffer &data)
{
   LOG_DEBUG(impl->log, "isoReadBinary(p1=0x{02x} p2=0x{02x} le={})", {p1, p2, le});

   rt::ByteBuffer cmd(5);
   cmd.putInt(CLA_ISO, 1);
   cmd.putInt(INS_ISO_READ_BINARY, 1);
   cmd.putInt(p1, 1);
   cmd.putInt(p2, 1);
   cmd.putInt(le, 1);
   cmd.flip();

   rt::ByteBuffer resp(256);

   if (const int sw = impl->sendCommand(cmd, resp); sw != ISO_STATUS_OK)
   {
      LOG_DEBUG(impl->log, "isoReadBinary -> sw=0x{04x}", {sw});
      return sw;
   }

   resp.flip();
   data.put(resp);
   data.flip();

   LOG_DEBUG(impl->log, "isoReadBinary -> bytes={}", {data.remaining()});

   return ISO_STATUS_OK;
}

/**
 * Executes ISO UPDATE BINARY.
 * @param p1 ISO P1 byte.
 * @param p2 ISO P2 byte.
 * @param data Input payload bytes.
 * @return ISO status word.
 */
int Desfire::isoUpdateBinary(unsigned int p1, unsigned int p2, const rt::ByteBuffer &data)
{
   LOG_DEBUG(impl->log, "isoUpdateBinary(p1=0x{02x} p2=0x{02x} data={x})", {p1, p2, data});

   const unsigned int dataLen = data.remaining();

   rt::ByteBuffer cmd(5 + dataLen);
   cmd.putInt(CLA_ISO, 1);
   cmd.putInt(INS_ISO_UPDATE_BINARY, 1);
   cmd.putInt(p1, 1);
   cmd.putInt(p2, 1);
   cmd.putInt(dataLen, 1);
   cmd.put(data);
   cmd.flip();

   rt::ByteBuffer resp(16);

   const int sw = impl->sendCommand(cmd, resp);

   LOG_DEBUG(impl->log, "isoUpdateBinary -> sw=0x{04x}", {sw});

   return sw;
}

/* ------------------------------------------------------------------ */
/* Session state accessors                                             */
/* ------------------------------------------------------------------ */

/**
 * Returns current authentication mode.
 * @return Active authentication mode.
 */
AuthMode Desfire::authMode() const
{
   return impl->currentAuthMode;
}

/**
 * Returns current authenticated key ID.
 * @return Current key ID or -1 when unauthenticated.
 */
int Desfire::authKeyId() const
{
   return impl->currentAuthKey;
}

} // namespace hce::cards::desfire
