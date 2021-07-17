#include "SDCard.h"

#include <EEPROM.h>
#include <string.h>

#include "Messages.h"
#include "BoardSPI.h"

#define HEX_DIGITS "0123456789ABCDEF"


SDCard::SDCard() {
    fileIsOpen = false;
    cardInit = false;

    strcpy(currentFileName, "000000.EOG");

    card = Sd2Card(&bSPI.spi, SD_SS);
}

void SDCard::setFileName() {
    fileNumber = 0;
    fileNumber = EEPROM.read(0);
    fileNumber <<= 8;
    fileNumber |= EEPROM.read(1);
    fileNumber <<= 8;
    fileNumber |= EEPROM.read(2);
    fileNumber ++;

    unsigned long mask = 0x00F00000;


    for (short i = 5; i >= 0; i --) {
        currentFileName[5 - i] = HEX_DIGITS[(fileNumber & mask) >> (i * 4)];
        mask >>= 4;
    }

    EEPROM.write(0, (fileNumber & 0xFF0000) >> 16);
    EEPROM.write(1, (fileNumber & 0x00FF00) >> 8);
    EEPROM.write(2, (fileNumber & 0x0000FF));
}


void SDCard::resetFileNumber () {
    EEPROM.write(0, 0);
    EEPROM.write(1, 0);
    EEPROM.write(2, 0);
}


bool SDCard::open() {
    if (!cardInit) {
        if (!card.init(SPI_FULL_SPEED, SD_SS)) {
            msg.send("[ERR] SDCard Initialization failed$$$");
        } else {
            cardInit = true;
        }
        if (!volume.init(card)) {
            msg.send("[ERR] Could not find FAT16/FAT32 partition$$$");
            return fileIsOpen;
        }
    }

    setFileName();

    openvol = root.openRoot(volume);
    openfile.remove(root, currentFileName);

    if (!openfile.createContiguous(root, currentFileName, BLOCK_COUNT * 512UL)) {
        msg.send("[ERR] Create Contiguous File fail$$$");
        cardInit = false;
    }

    if (!openfile.contiguousRange(&bgnBlock, &endBlock)) {
        msg.send("[ERR] Get Contiguous Range fail$$$");
        cardInit = false;
    }

    pCache = (unsigned char*)volume.cacheClear();

    if (!card.erase(bgnBlock, endBlock)){
        msg.send("[ERR] Erase block fail$$$");
        cardInit = false;
    }

    if (!card.writeStart(bgnBlock, BLOCK_COUNT)){
        msg.send("[ERR] Write Start fail$$$");
        cardInit = false;
    } else {
        fileIsOpen = true;
        delay(1);
    }
    bSPI.deactivateSD();

    byteCounter = 0;
    blockCounter = 0;

    if (fileIsOpen) {
        msg.send("[MSG] SD File: ");
        msg.send(currentFileName);
        msg.sendEOM();
    }

    return fileIsOpen;
}


bool SDCard::close() {
    if (fileIsOpen) {
        writeFooter();

        bSPI.activateSD();
        card.writeStop();
        openfile.close();
        bSPI.deactivateSD();

        fileIsOpen = false;

        msg.send("[MSG] file closed successfully$$$");
    } else {
        msg.send("[ERR] No open file to close$$$");
    }
    return fileIsOpen;
}


void SDCard::writeTimestamp(unsigned long timestamp) {
    if (fileIsOpen) {
        unsigned long mask = 0xFF000000;
        unsigned char shift = 24;
        for (int i = 0; i < 4; i ++) {
            pCache[byteCounter + i] = (timestamp & mask) >> shift;
            mask >>= 8;
            shift -= 8;
        }
        byteCounter += 4;
    }
}


void SDCard::writeSampleToSD(
    unsigned long timestamp,
    unsigned long sampleNumber,
    int horizontalChannel,
    int verticalChannel,
    unsigned short label
) {
    if (fileIsOpen) {
        // Write Sample Header
        pCache[byteCounter] = 0;
        byteCounter ++;

        // Write Time Stamp
        writeTimestamp(timestamp);

        // Write Sample Number
        int i;
        unsigned long mask = 0x00FF0000;
        unsigned char shift = 16;
        for (i = 0; i < 3; i ++) {
            pCache[byteCounter + i] = (sampleNumber & mask) >> shift;
            mask >>= 8;
            shift -= 8;
        }
        byteCounter += 3;

        // Write Horizontal Channel
        mask = 0x00FF0000;
        shift = 16;
        for (i = 0; i < 3; i ++) {
            pCache[byteCounter + i] = (horizontalChannel & mask) >> shift;
            mask >>= 8;
            shift -= 8;
        }
        byteCounter += 3;

        // Write Vertical Channel
        mask = 0x00FF0000;
        shift = 16;
        for (i = 0; i < 3; i ++) {
            pCache[byteCounter + i] = (verticalChannel & mask) >> shift;
            mask >>= 8;
            shift -= 8;
        }
        byteCounter += 3;

        // Write Label
        mask = 0x0000FF00;
        shift = 8;
        for (i = 0; i < 2; i ++) {
            pCache[byteCounter + i] = (label & mask) >> shift;
            mask >>= 8;
            shift -= 8;
        }
        byteCounter += 2;

        if (byteCounter == 512){
            writeCache();
        }
    }
}

void SDCard::writePart(unsigned char header, bool fillCache) {
    if (fileIsOpen) {
        // Write Header
        pCache[byteCounter ++] = header;

        // Write Time Stamp
        writeTimestamp(millis());

        // Fill the sample with zeroes
        for (int i = 0; i < 11; i ++) {
            pCache[byteCounter ++] = 0;
        }

        if (fillCache) {
            while (byteCounter < 512) {
                pCache[byteCounter ++] = 0;
            }
        }

        if (byteCounter == 512){
            writeCache();
        }
    }
}

void SDCard::writeTestStarts() {
    if (fileIsOpen) {
        writePart(0x80, false);
    }
}

void SDCard::writeTestEnds() {
    if (fileIsOpen) {
        writePart(0x81, false);
    }
}

void SDCard::writeFooter() {
    if (fileIsOpen) {
        writePart(0x82, true);
    }
}

void SDCard::writeCache() {
    if (fileIsOpen) {
        if (blockCounter > BLOCK_COUNT)
            return;

        bSPI.activateSD();
        if (!card.writeData(pCache)) {
            msg.send("[ERR] Block write fail");
            msg.sendEOM();
        }
        bSPI.deactivateSD();

        byteCounter = 0;
        blockCounter ++;

        if (blockCounter == BLOCK_COUNT - 1) {
            close();
            blockCounter = 0;
        }
    }
}
