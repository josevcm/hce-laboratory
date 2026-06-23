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

#include <string>
#include <algorithm>
#include <vector>

#include <rt/Logger.h>
#include <rt/ByteBuffer.h>

#include <hw/dev/PCSC.h>

#include <hce/Target.h>
#include <hce/cards/desfire/Desfire.h>
#include <hce/targets/Desfire.h>
#include <hce/cards/mifareplus/MifarePlus.h>
#include <hce/targets/MifarePlus.h>

#include <TestContext.h>
#include "tests/desfire/TestGetVersionInfo.h"
#include "tests/desfire/TestSelectApplication.h"
#include "tests/desfire/TestListApplications.h"
#include "tests/desfire/TestGetFreeMemory.h"
#include "tests/desfire/TestAuthenticateAES.h"
#include "tests/desfire/TestAuthenticate.h"
#include "tests/desfire/TestGetKeySettings.h"
#include "tests/desfire/TestGetKeyVersion.h"
#include "tests/desfire/TestFormatCard.h"
#include "tests/desfire/TestCreateApplication.h"
#include "tests/desfire/TestAuthenticateISO.h"
#include "tests/desfire/TestChangeKeySettings.h"
#include "tests/desfire/TestChangeKey.h"
#include "tests/desfire/TestStandardFile.h"
#include "tests/desfire/TestBackupFile.h"
#include "tests/desfire/TestCyclicRecordFile.h"
#include "tests/desfire/TestValueFile.h"
#include "tests/desfire/TestLinearRecordFile.h"
#include "tests/desfire/TestIsoSelect.h"
#include "tests/desfire/TestChangeFileSettings.h"
#include "tests/desfire/TestLimitedCredit.h"
#include "tests/desfire/TestGetCardUID.h"
#include "tests/desfire/TestListDFNames.h"
#include "tests/desfire/TestGetIsoFileIDs.h"
#include "tests/desfire/TestIsoReadWrite.h"
#include "tests/desfire/TestCommitTransaction.h"
#include "tests/desfire/TestAbortTransaction.h"
#include "tests/desfire/TestTransactionChaining.h"
#include "tests/desfire/TestDeleteFile.h"
#include "tests/desfire/TestCyclicRecordWrap.h"
#include "tests/desfire/TestMultiKeyApplication.h"
#include "tests/desfire/TestCreditBoundary.h"
#include "tests/desfire/TestValueFileValidation.h"
#include "tests/desfire/TestDeleteApplicationAutoSelect.h"
#include "tests/desfire/TestIsoSelectById.h"
#include "tests/desfire/TestClearRecordBehavior.h"
#include "tests/desfire/TestAuthenticationErrors.h"
#include "tests/desfire/TestAccessControl.h"
#include "tests/desfire/TestDuplicateErrors.h"
#include "tests/desfire/TestEncipheredMode.h"
#include "tests/desfire/TestNoChangesTransaction.h"
#include "tests/desfire/TestIsoAbsoluteOffset.h"
#include "tests/desfire/TestMacingMode.h"
#include "tests/desfire/TestAESEncipheredMode.h"
#include "tests/desfire/TestCmacPlain.h"
#include "tests/desfire/TestUnknownCommand.h"
#include "tests/desfire/TestIsoSFI.h"
#include "tests/desfire/TestValueFileLimits.h"
#include "tests/desfire/TestSessionKey2K3DES.h"
#include "tests/mifareplus/TestMFPAuthenticate.h"
#include "tests/mifareplus/TestMFPAuthErrors.h"
#include "tests/mifareplus/TestMFPRead.h"
#include "tests/mifareplus/TestMFPReadEncrypted.h"
#include "tests/mifareplus/TestMFPWrite.h"
#include "tests/mifareplus/TestMFPWriteEncrypted.h"
#include "tests/mifareplus/TestMFPValueBlock.h"
#include "tests/mifareplus/TestMFPGetUID.h"
#include "tests/mifareplus/TestMFPMultiSector.h"

using namespace hce::cards::desfire;

static Transport makePcscTransport(hw::PCSC &pcsc)
{
   return [&pcsc](const rt::ByteBuffer &cmd, rt::ByteBuffer &resp) -> int {

      if (pcsc.transmit(cmd, resp) != 0)
         return -1;

      if (resp.remaining() < 2)
         return -1;

      const unsigned int sw1 = resp.data()[resp.limit() - 2];
      const unsigned int sw2 = resp.data()[resp.limit() - 1];

      return static_cast<int>((sw1 << 8) | sw2);
   };
}

// Wraps the emulated DESFire target as a Transport for the client-side Desfire API.
// The server's process() already calls response.flip() before returning, so the
// buffer is in read position (position=0, limit=bytes_written) when we inspect SW1/SW2.
inline Transport makeLoopbackTransport(hce::targets::desfire::Desfire &server)
{
   return [&server](const rt::ByteBuffer &request, rt::ByteBuffer &response) -> int {
      server.process(request, response);

      if (response.remaining() < 2)
         return -1;

      const unsigned int sw1 = response.data()[response.limit() - 2];
      const unsigned int sw2 = response.data()[response.limit() - 1];

      return static_cast<int>((sw1 << 8) | sw2);
   };
}

// Wraps the emulated MifarePlus target as a Transport.
// MifarePlus responses start with a status byte (0x90 = OK, others = error).
inline hce::cards::mifareplus::Transport makeMFPLoopbackTransport(hce::targets::mifareplus::MifarePlus &server)
{
   return [&server](const rt::ByteBuffer &request, rt::ByteBuffer &response) -> int {
      server.process(request, response);

      if (response.remaining() < 1)
         return -1;

      return static_cast<int>(response.data()[0]);
   };
}

static int runSecurityTests(Desfire &card)
{
   TestContext ctx;

   LOG_INFO(ctx.log, "=== Security Commands Tests ===");

   testAuthenticate(card, ctx);
   testAuthenticateISO(card, ctx);
   testAuthenticateAES(card, ctx);
   testGetKeySettings(card, ctx);
   testGetKeyVersion(card, ctx);
   testChangeKey(card, ctx);
   testMultiKeyApplication(card, ctx);
   testChangeKeySettings(card, ctx);
   testAuthenticationErrors(card, ctx);
   testUnknownCommand(card, ctx);
   testSessionKey2K3DES(card, ctx);

   LOG_INFO(ctx.log, "=== Security Tests Results: {} passed, {} failed ===", {ctx.passed, ctx.failed});

   return ctx.failed > 0 ? 1 : 0;
}

static int runPiccTests(Desfire &card)
{
   TestContext ctx;

   LOG_INFO(ctx.log, "=== PICC Level Commands Tests ===");

   testGetVersionInfo(card, ctx);
   testGetFreeMemory(card, ctx);
   testGetCardUID(card, ctx);
   testCreateApplication(card, ctx);
   testSelectApplication(card, ctx);
   testListApplications(card, ctx);
   testDeleteApplicationAutoSelect(card, ctx);
   testAccessControl(card, ctx);
   testListDFNames(card, ctx);
   testFormatCard(card, ctx);

   LOG_INFO(ctx.log, "=== PICC Tests Results: {} passed, {} failed ===", {ctx.passed, ctx.failed});

   return ctx.failed > 0 ? 1 : 0;
}

static int runApplicationTests(Desfire &card)
{
   TestContext ctx;

   LOG_INFO(ctx.log, "=== Application Level Commands Tests ===");

   testStandardFile(card, ctx);
   testDeleteFile(card, ctx);
   testDuplicateErrors(card, ctx);
   testBackupFile(card, ctx);
   testValueFile(card, ctx);
   testCreditBoundary(card, ctx);
   testValueFileValidation(card, ctx);
   testValueFileLimits(card, ctx);
   testEncipheredMode(card, ctx);
   testLinearRecordFile(card, ctx);
   testCyclicRecordFile(card, ctx);
   testCyclicRecordWrap(card, ctx);
   testLimitedCredit(card, ctx);
   testChangeFileSettings(card, ctx);

   LOG_INFO(ctx.log, "=== Application Tests Results: {} passed, {} failed ===", {ctx.passed, ctx.failed});

   return ctx.failed > 0 ? 1 : 0;
}

static int runIsoTests(Desfire &card)
{
   TestContext ctx;

   LOG_INFO(ctx.log, "=== ISO7816-4 Commands Tests ===");

   testIsoSelect(card, ctx);
   testIsoSelectById(card, ctx);
   testIsoAbsoluteOffset(card, ctx);
   testIsoGetFileIDs(card, ctx);
   testIsoReadWrite(card, ctx);
   testIsoSFI(card, ctx);

   LOG_INFO(ctx.log, "=== ISO Tests Results: {} passed, {} failed ===", {ctx.passed, ctx.failed});

   return ctx.failed > 0 ? 1 : 0;
}

static int runTransactionTests(Desfire &card)
{
   TestContext ctx;

   LOG_INFO(ctx.log, "=== Transaction Control Tests ===");

   testCommitTransaction(card, ctx);
   testAbortTransaction(card, ctx);
   testClearRecordBehavior(card, ctx);
   testNoChangesTransaction(card, ctx);
   testTransactionChaining(card, ctx);
   testMacingMode(card, ctx);
   testAESEncipheredMode(card, ctx);
   testCmacPlain(card, ctx);

   LOG_INFO(ctx.log, "=== Transaction Tests Results: {} passed, {} failed ===", {ctx.passed, ctx.failed});

   return ctx.failed > 0 ? 1 : 0;
}

static int runAllTests(Desfire &card)
{
   TestContext ctx;

   LOG_INFO(ctx.log, "=== Running All Tests ===");

   // --- Security Commands Tests ---
   testAuthenticate(card, ctx);
   testAuthenticateISO(card, ctx);
   testAuthenticateAES(card, ctx);
   testGetKeySettings(card, ctx);
   testGetKeyVersion(card, ctx);
   testChangeKey(card, ctx);
   testMultiKeyApplication(card, ctx);
   testChangeKeySettings(card, ctx);
   testAuthenticationErrors(card, ctx);
   testUnknownCommand(card, ctx);
   testSessionKey2K3DES(card, ctx);

   // --- PICC Level Commands Tests ---
   testGetVersionInfo(card, ctx);
   testGetFreeMemory(card, ctx);
   testGetCardUID(card, ctx);
   testCreateApplication(card, ctx);
   testSelectApplication(card, ctx);
   testListApplications(card, ctx);
   testDeleteApplicationAutoSelect(card, ctx);
   testAccessControl(card, ctx);
   testListDFNames(card, ctx);
   testFormatCard(card, ctx);

   // --- Application Level Commands Tests ---
   testStandardFile(card, ctx);
   testDeleteFile(card, ctx);
   testDuplicateErrors(card, ctx);
   testBackupFile(card, ctx);
   testValueFile(card, ctx);
   testCreditBoundary(card, ctx);
   testValueFileValidation(card, ctx);
   testValueFileLimits(card, ctx);
   testEncipheredMode(card, ctx);
   testLinearRecordFile(card, ctx);
   testCyclicRecordFile(card, ctx);
   testCyclicRecordWrap(card, ctx);
   testLimitedCredit(card, ctx);
   testChangeFileSettings(card, ctx);

   // --- ISO7816-4 Commands Tests ---
   testIsoSelect(card, ctx);
   testIsoSelectById(card, ctx);
   testIsoAbsoluteOffset(card, ctx);
   testIsoGetFileIDs(card, ctx);
   testIsoReadWrite(card, ctx);
   testIsoSFI(card, ctx);

   // --- Transaction Control Tests ---
   testCommitTransaction(card, ctx);
   testAbortTransaction(card, ctx);
   testClearRecordBehavior(card, ctx);
   testNoChangesTransaction(card, ctx);
   testTransactionChaining(card, ctx);
   testMacingMode(card, ctx);
   testAESEncipheredMode(card, ctx);
   testCmacPlain(card, ctx);

   LOG_INFO(ctx.log, "=== All Tests Results: {} passed, {} failed ===", {ctx.passed, ctx.failed});

   return ctx.failed > 0 ? 1 : 0;
}

static int runMFPTests(hce::cards::mifareplus::MifarePlus &card)
{
   TestContext ctx;

   LOG_INFO(ctx.log, "=== Mifare Plus Tests ===");

   testMFPAuthenticate(card, ctx);
   testMFPAuthErrors(card, ctx);
   testMFPRead(card, ctx);
   testMFPReadEncrypted(card, ctx);
   testMFPWrite(card, ctx);
   testMFPWriteEncrypted(card, ctx);
   testMFPValueBlock(card, ctx);
   testMFPGetUID(card, ctx);
   testMFPMultiSector(card, ctx);

   LOG_INFO(ctx.log, "=== Mifare Plus Tests Results: {} passed, {} failed ===", {ctx.passed, ctx.failed});

   return ctx.failed > 0 ? 1 : 0;
}

static void printUsage(const char *programName)
{
   std::cerr << "Usage: " << programName << " [options]\n\n"
      << "Test Groups:\n"
      << "  --test-desfire            Run all DESFire tests\n"
      << "  --test-mfp                Run all Mifare Plus tests\n"
      << "  --test-all                Run all test groups (default)\n"
      << "\nDESFire Sub-categories:\n"
      << "  --test-security           Run Security Commands Tests\n"
      << "  --test-picc               Run PICC Level Commands Tests\n"
      << "  --test-application        Run Application Level Commands Tests\n"
      << "  --test-iso                Run ISO7816-4 Commands Tests\n"
      << "  --test-transaction        Run Transaction Control Tests\n"
      << "\nOptions:\n"
      << "  --pcsc                    Use physical PCSC reader (default: emulated loopback)\n"
      << "  --verbose                 Enable TRACE_LEVEL logging for DESFire client\n"
      << "  --help                    Show this help message\n";
}

int main(int argc, char *argv[])
{
   rt::Logger::init(std::cout);
   rt::Logger::setPattern("%m%n");
   rt::Logger::setRootLevel(rt::Logger::WARN_LEVEL);

   const rt::Logger *log = rt::Logger::getLogger("app.main");

   rt::Logger::setLoggerLevel("app.main", rt::Logger::INFO_LEVEL);
   rt::Logger::setLoggerLevel("hce.tests", rt::Logger::INFO_LEVEL);

   bool pcscMode = false;
   bool verboseMode = false;
   std::string testCategory = "all"; // default: run all tests

   for (int i = 1; i < argc; ++i)
   {
      auto value = std::string(argv[i]);

      if (value == "--help" || value == "-h")
      {
         printUsage(argv[0]);
         return 0;
      }
      else if (value == "--pcsc")
         pcscMode = true;
      else if (value == "--verbose")
         verboseMode = true;
      else if (value == "--test-all")
         testCategory = "all";
      else if (value == "--test-desfire")
         testCategory = "desfire";
      else if (value == "--test-mfp")
         testCategory = "mfp";
      else if (value == "--test-security")
         testCategory = "security";
      else if (value == "--test-picc")
         testCategory = "picc";
      else if (value == "--test-application")
         testCategory = "application";
      else if (value == "--test-iso")
         testCategory = "iso";
      else if (value == "--test-transaction")
         testCategory = "transaction";
      else
      {
         LOG_ERROR(log, "Unknown option: {}", {value});
         printUsage(argv[0]);
         return 1;
      }
   }

   if (verboseMode)
      rt::Logger::setLoggerLevel("hce.cards.desfire.Desfire", rt::Logger::TRACE_LEVEL);

   if (pcscMode)
   {
      hw::PCSC pcsc;

      LOG_INFO(log, "=== hce-ut: DESFire unit tests (PCSC) ===");
      LOG_INFO(log, "Test category: {}", {testCategory});

      const auto readers = pcsc.listReaders();

      if (readers.empty())
      {
         LOG_ERROR(log, "No PCSC readers found — connect a card reader and retry.");
         return 1;
      }

      LOG_INFO(log, "Available PCSC readers:");

      for (const auto &r: readers)
         LOG_INFO(log, "\t{}", {r});

      const std::string &reader = readers[0];
      LOG_INFO(log, "Connecting to: {}", {reader});

      if (pcsc.connect(reader) != 0)
      {
         LOG_ERROR(log, "Failed to connect, is a DESFire card on the reader?");
         return 1;
      }

      Desfire card(makePcscTransport(pcsc));

      int result;
      if (testCategory == "security")
         result = runSecurityTests(card);
      else if (testCategory == "picc")
         result = runPiccTests(card);
      else if (testCategory == "application")
         result = runApplicationTests(card);
      else if (testCategory == "iso")
         result = runIsoTests(card);
      else if (testCategory == "transaction")
         result = runTransactionTests(card);
      else // "desfire" or "all" — PCSC only supports DESFire
         result = runAllTests(card);

      pcsc.disconnect();

      return result;
   }

   LOG_INFO(log, "=== hce-ut: DESFire unit tests (emulated) ===");
   LOG_INFO(log, "Test category: {}", {testCategory});
   LOG_INFO(log, "Run with --pcsc to test against a physical DESFire card via PCSC.");

   // --- Mifare Plus emulated tests ---
   auto runMFP = [&]() -> int {
      hce::targets::mifareplus::MifarePlus mfpServer;
      mfpServer.set(hce::Target::PARAM_UID, rt::ByteBuffer {0x04, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01});
      mfpServer.select();
      hce::cards::mifareplus::MifarePlus mfpCard(makeMFPLoopbackTransport(mfpServer));
      return runMFPTests(mfpCard);
   };

   if (testCategory == "mfp")
      return runMFP();

   // --- DESFire emulated tests ---
   hce::targets::desfire::Desfire server(hce::targets::desfire::Desfire4K);

   // Provide a fixed 7-byte UID so testGetVersionInfo passes the length check
   server.set(hce::Target::PARAM_UID, rt::ByteBuffer {0x04, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01});

   // Simulate NFC card selection before the first APDU exchange
   server.select();

   Desfire card(makeLoopbackTransport(server));

   if (testCategory == "security")
      return runSecurityTests(card);

   if (testCategory == "picc")
      return runPiccTests(card);

   if (testCategory == "application")
      return runApplicationTests(card);

   if (testCategory == "iso")
      return runIsoTests(card);

   if (testCategory == "transaction")
      return runTransactionTests(card);

   if (testCategory == "desfire")
      return runAllTests(card);

   // "all" — DESFire tests + MFP tests
   int desfireResult = runAllTests(card);
   int mfpResult = runMFP();
   return (desfireResult != 0 || mfpResult != 0) ? 1 : 0;
}
