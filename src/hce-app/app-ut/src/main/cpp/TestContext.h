#pragma once

#include <iostream>
#include <string>

struct TestContext
{
   rt::Logger *log = rt::Logger::getLogger("hce.tests");

   int passed = 0;
   int failed = 0;

   void check(const std::string &desc, bool condition)
   {
      if (condition)
      {
         LOG_INFO(log, " PASS: {}", { desc});
         ++passed;
      }
      else
      {
         LOG_WARN(log, " FAIL: {}", { desc});
         ++failed;
      }
   }
};

// Default master-key hex strings for virgin DESFire cards
static constexpr const char *MASTER_KEY_AES_HEX = "00000000000000000000000000000000";
static constexpr const char *MASTER_KEY_DES_HEX = "0000000000000000";
