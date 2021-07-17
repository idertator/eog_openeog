#include <Arduino.h>

#include "ADS1299.h"
#include "Board.h"
#include "Definitions.h"
#include "Messages.h"
#include "SDCard.h"


EOGBoard::EOGBoard() {
    horizontalChannel = 1;
    verticalChannel = 2;

    position = Center;

    streaming = false;

    timestamp = 0;
    sampleNumber = 0;
    horizontalSample = 0;
    verticalSample = 0;

    command = 0;
    params = 0;
    selectedChannel = 0;
}

bool EOGBoard::processChar(char c) {
    char paramValue;

    if (params > 0) {
        paramValue = c;
    } else {
        command = c;
    }

    switch (command) {
        case EOG_SET_POSITION: {
            if (params == 1) {
                switch (paramValue) {
                    case 'l': {
                        position = Left;
                        break;
                    }
                    case 'r': {
                        position = Right;
                        break;
                    }
                    case 't': {
                        position = Top;
                        break;
                    }
                    case 'b': {
                        position = Bottom;
                        break;
                    }
                    case 'c': {
                        position = Center;
                        break;
                    }
                }
                params = 0;
                command = 0;
            } else {
                params = 1;
            }
            break;
        }

        case EOG_CHANNEL_CONFIG: {
            if (!streaming) {
                if (params > 0) {
                    switch (params) {
                        case 8: {
                            selectedChannel = paramValue - '1';
                            break;
                        }
                        case 7: {
                            ads.channelSettings[selectedChannel][POWER_DOWN] = paramValue - '0';
                            break;
                        }
                        case 6: {
                            ads.channelSettings[selectedChannel][GAIN_SET] = (paramValue - '0') << 4;
                            break;
                        }
                        case 5: {
                            ads.channelSettings[selectedChannel][INPUT_TYPE_SET] = paramValue - '0';
                            break;
                        }
                        case 4: {
                            ads.channelSettings[selectedChannel][BIAS_SET] = paramValue - '0';
                            break;
                        }
                        case 3: {
                            ads.channelSettings[selectedChannel][SRB2_SET] = paramValue - '0';
                            break;
                        }
                        case 2: {
                            ads.channelSettings[selectedChannel][SRB1_SET] = paramValue - '0';
                            break;
                        }
                        case 1: {
                            ads.writeChannelSettings(selectedChannel + 1);

                            msg.send("[MSG] Channel set for ");
                            msg.send((unsigned char)(selectedChannel + '1'));
                            msg.sendEOM();
                            break;
                        }
                    }
                    params --;

                    if (params == 0) {
                        command = 0;
                        selectedChannel = 0;
                    }
                } else {
                    params = 8;
                }
            }
            break;
        }

        case EOG_SET_CHANNEL: {
            if (!streaming) {
                if (params > 0) {
                    switch (params) {
                        case 1: {
                            if (paramValue < '1' || paramValue > '8') {
                                msg.send("[ERR] Channel not between 1-8$$$");
                            } else {
                                verticalChannel = paramValue - '0';
                            }
                            break;
                        }
                        case 2: {
                            if (paramValue < '1' || paramValue > '8') {
                                msg.send("[ERR] Channel not between 1-8$$$");
                            } else {
                                horizontalChannel = paramValue - '0';
                            }
                            break;
                        }
                    }
                    params --;
                    if (params == 0) {
                        msg.send("[MSG] Horizontal Channel is ");
                        msg.send((char)(horizontalChannel + '0'));
                        msg.send(" and Vertical Channel is ");
                        msg.send((char)(verticalChannel + '0'));
                        msg.sendEOM();

                        for (int i = 1; i <= 8; i ++) {
                            if (horizontalChannel == i || verticalChannel == i) {
                                ads.activateChannel(i);
                            } else {
                                ads.deactivateChannel(i);
                            }
                        }
                        command = 0;
                    }
                } else {
                    params = 2;
                }
            }
            break;
        }

        case EOG_START_COLLECTING: {
            params = 0;
            command = 0;
            start();
            sd.writeTestStarts();
            break;
        }

        case EOG_FINISH_COLLECTING: {
            params = 0;
            command = 0;
            stop();
            sd.writeTestEnds();
            break;
        }

        case EOG_SOFT_RESET: {
            params = 0;
            command = 0;
            stop();

            msg.sendVersion(ads.deviceID());
            break;
        }

        case EOG_SD_OPEN: {
            params = 0;
            command = 0;
            if (!sd.fileIsOpen) {
                sd.open();
            }
            break;
        }

        case EOG_SD_CLOSE: {
            params = 0;
            command = 0;
            if (sd.fileIsOpen) {
                sd.close();
            }
            break;
        }

        case EOG_SD_RESET_FILENAME: {
            sd.resetFileNumber();
        }

        default:
            return false;
    }

    return true;
}

void EOGBoard::updateChannelData() {
    ads.updateChannelData();

    timestamp = millis();
    sampleNumber ++;
    horizontalSample = ads.getChannel(horizontalChannel);
    verticalSample = ads.getChannel(verticalChannel);

    if (sd.fileIsOpen) {
        sd.writeSampleToSD(
            timestamp,
            sampleNumber,
            horizontalSample,
            verticalSample,
            position
        );
    }
}

void EOGBoard::sendChannelData() {
    if (sampleNumber % UPLOADING_STEP == 0) {
        // Write sample header
        buffer[bufferCursor ++] = 0;

        // Write Sample Number
        int i;
        unsigned long mask = 0x0000FF00;
        unsigned char shift = 8;
        for (i = 0; i < 2; i ++) {
            buffer[bufferCursor ++] = (sampleNumber & mask) >> shift;
            mask >>= 8;
            shift -= 8;
        }

        // Write Horizontal Channel
        mask = 0x00FF0000;
        shift = 16;
        for (i = 0; i < 3; i ++) {
            buffer[bufferCursor ++] = (horizontalSample & mask) >> shift;
            mask >>= 8;
            shift -= 8;
        }

        // Write Vertical Channel
        mask = 0x00FF0000;
        shift = 16;
        for (i = 0; i < 3; i ++) {
            buffer[bufferCursor ++] = (verticalSample & mask) >> shift;
            mask >>= 8;
            shift -= 8;
        }

        // Write Label
        buffer[bufferCursor ++] = (unsigned char)position;

        if (bufferCursor == BUFFER_SIZE) {
            msg.sendPackage(buffer, BUFFER_SIZE);
            bufferCursor = 0;
        }
    }
}

void EOGBoard::start() {
    if (!streaming) {
        msg.send("[MSG] Test started$$$");
        position = Center;
        delay(1000);

        streaming = true;
        sampleNumber = 0;
        bufferCursor = 0;
        ads.start();
    }
}

void EOGBoard::stop() {
    if (streaming) {
        ads.stop();
        streaming = false;
        msg.send("[MSG] Test ended$$$");
    }
}

EOGBoard eogBoard;
