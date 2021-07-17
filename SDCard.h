#ifndef _____OpenEOG_32bit__SDCard
#define _____OpenEOG_32bit__SDCard

#include <Arduino.h>
#include <OBCI32_SD.h>

#include "Definitions.h"

class SDCard {
    public:
        bool fileIsOpen;

        SDCard();

        bool open();
        bool close();

        void resetFileNumber();

        void writeTestStarts();
        void writeTestEnds();
        void writeSampleToSD(
            unsigned long timestamp,
            unsigned long sampleNumber,
            int horizontalChannel,
            int verticalChannel,
            unsigned short label
        );

    private:
        const unsigned long BLOCK_COUNT = 32768;
        int blockCounter = 0;
        int byteCounter = 0;

        SdFile openfile;
        Sd2Card card;
        SdVolume volume;
        SdFile root;

        unsigned char* pCache;
        unsigned long bgnBlock, endBlock;

        bool openvol;
        bool cardInit;

        char currentFileName[11];
        unsigned long fileNumber;

        void activate();
        void deactivate();
        void setFileName();

        void writeTimestamp(unsigned long timestamp);
        void writePart(unsigned char header, bool fillCache);
        void writeFooter();
        void writeCache();
};

#endif
