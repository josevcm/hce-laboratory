/*

  This file is part of NFC-EMULATION.

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
  along with NFC-EMULATION. If not, see <http://www.gnu.org/licenses/>.

*/

#include <iostream>

#include <rt/Executor.h>
#include <rt/Logger.h>

#include <hce/tasks/TargetListenerTask.h>

#include <hce/targets/Desfire.h>
//
#include <hw/ic/PN7160.h>

int main(int argc, char* argv[])
{
    // send logging events to stderr
    rt::Logger::init(std::cout);

    // disable logging at all (can be enabled with -v option)
    rt::Logger::setRootLevel(rt::Logger::WARN_LEVEL);
    rt::Logger::setLoggerLevel("app.*", rt::Logger::INFO_LEVEL);
    rt::Logger::setLoggerLevel("hw.MPSSE", rt::Logger::ERROR_LEVEL);
    rt::Logger::setLoggerLevel("hw.PN7160", rt::Logger::INFO_LEVEL);
    rt::Logger::setLoggerLevel("hce.core.crypto.*", rt::Logger::INFO_LEVEL);
    rt::Logger::setLoggerLevel("hce.targets.desfire.*", rt::Logger::DEBUG_LEVEL);

    const rt::Logger* log = rt::Logger::getLogger("app.main");

    unsigned char uid[7] = {0x04, 0x3b, 0x4f, 0x5A, 0x74, 0x43, 0x80};

    hce::targets::Desfire card;

    const std::vector<hw::PN7160::Parameter> parameters({

        // Listen Mode – NFC-A Discovery Parameters
        {hw::PN7160::PARAM_LA_BIT_FRAME_SDD, {0x44}}, // first byte of ATQA
        {hw::PN7160::PARAM_LA_PLATFORM_CONFIG, {0x03}}, // second byte of ATQA
        {hw::PN7160::PARAM_LA_SEL_INFO, {0x20}}, // SAK, support ISO-DEP
        {hw::PN7160::PARAM_LA_NFCID1, rt::ByteBuffer(uid, 7)}, // UID

        // Listen Mode – ISO-DEP Discovery Parameters
        {hw::PN7160::PARAM_LI_A_BIT_RATE, {0x00}}, // only 106Kbps
        {hw::PN7160::PARAM_LI_A_HIST_BY, {0x80}}, // historical bytes
        {hw::PN7160::PARAM_LI_A_RATS_TB1, {0x81}}, //
        {hw::PN7160::PARAM_LI_A_RATS_TC1, {0x02}}, //

        // Other Parameters
        {hw::PN7160::PARAM_RF_FIELD_INFO, {0x00}}, // notify when external field is detected
        {hw::PN7160::PARAM_RF_NFCEE_ACTION, {0x01}}, // notify activation / deactivation
    });

    hw::PN7160 pn7160(hw::PN7160::SPI);
    // hw::PN7160 pn7160(hw::PN7160::I2C);

    log->info("open device");

    if (pn7160.open())
    {
        log->info("starting discovery");

        if (pn7160.startDiscovery(parameters, hw::PN7160::DISCOVERY_LISTEN))
        {
            rt::ByteBuffer request(256);
            rt::ByteBuffer response(256);

            log->info("waiting for reader...");

            while (const int event = pn7160.waitEvent(request, 30000))
            {
                // process received event
                if (event == hw::PN7160::EVENT_DATA)
                {
                    // clear previous response
                    response.clear();

                    // process event
                    card.process(request, response);

                    // send response to reader
                    if (!pn7160.sendData(response))
                    {
                        break;
                    }
                }
                else if (event == hw::PN7160::EVENT_ACTIVATED)
                {
                    // log->info("card ACTIVATED **************************************");
                    card.select();
                }
                else if (event == hw::PN7160::EVENT_DEACTIVATED)
                {
                    // log->info("card DEACTIVATED ************************************");
                    card.deselect();
                }

                // reset buffer for next read
                request.clear();
            }

            log->info("stop discovery");

            if (!pn7160.stopDiscovery())
                log->info("stop discovery failed");
        }
    }

    // flush all pending log messages
    rt::Logger::flush();

    return 0;
}
