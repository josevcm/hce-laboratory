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

#ifndef RT_LOGGER_H
#define RT_LOGGER_H

#include <string>
#include <vector>

#include <rt/Variant.h>

#ifndef DEBUG_ALL
#define DEBUG_ALL 1
#endif

#if DEBUG_ALL
#define LOG_ERROR(logger, fmt, ...) (logger)->error(fmt, ##__VA_ARGS__)
#define LOG_WARN(logger, fmt, ...) (logger)->warn(fmt, ##__VA_ARGS__)
#define LOG_INFO(logger, fmt, ...) (logger)->info(fmt, ##__VA_ARGS__)
#define LOG_DEBUG(logger, fmt, ...) (logger)->debug(fmt, ##__VA_ARGS__)
#define LOG_TRACE(logger, fmt, ...) (logger)->trace(fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(logger, fmt, ...) ((void)0)
#define LOG_WARN(logger, fmt, ...) ((void)0)
#define LOG_INFO(logger, fmt, ...) ((void)0)
#define LOG_DEBUG(logger, fmt, ...) ((void)0)
#define LOG_TRACE(logger, fmt, ...) ((void)0)
#endif

namespace rt {

class Logger
{
   Logger(std::string name, int level);

   public:

      enum Level
      {
         NONE_LEVEL = 0,
         ERROR_LEVEL = 1,
         WARN_LEVEL = 2,
         INFO_LEVEL = 3,
         DEBUG_LEVEL = 4,
         TRACE_LEVEL = 5
      };

      void trace(const std::string &format, std::vector<Variant> params = {}) const;

      void debug(const std::string &format, std::vector<Variant> params = {}) const;

      void info(const std::string &format, std::vector<Variant> params = {}) const;

      void warn(const std::string &format, std::vector<Variant> params = {}) const;

      void error(const std::string &format, std::vector<Variant> params = {}) const;

      void print(int level, const std::string &format, std::vector<Variant> params = {}) const;

      int getLevel() const;

      void setLevel(int value);

      void setLevel(const std::string &level);

      bool isEnabled(int level) const;

      bool isTraceEnabled() const;

      bool isDebugEnabled() const;

      bool isInfoEnabled() const;

   public: // static methods

      static void init(std::ostream &stream, int level = WARN_LEVEL, bool buffered = true);

      static void shutdown();

      static void flush();

      static int getRootLevel();

      static void setRootLevel(int level);

      static void setRootLevel(const std::string &level);

      static void setLoggerLevel(const std::string &expr, int level);

      static void setLoggerLevel(const std::string &expr, const std::string &level);

      static Logger *getLogger(const std::string &name, int level = WARN_LEVEL);

      static std::map<std::string, std::shared_ptr<Logger>> &loggers();

   private: // private static methods

      static std::mutex &getMutex();

      static std::map<std::string, int> &getLevels();

   private:

      int level;

      std::string name;
};

}
#endif
