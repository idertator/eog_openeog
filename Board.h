#ifndef _____OpenEOG_32bit__EOGBoard
#define _____OpenEOG_32bit__EOGBoard

#include <Arduino.h>

#include "SDCard.h"


// EOG Commands
#define EOG_SET_POSITION            'O'
#define EOG_SET_CHANNEL             'N'
#define EOG_START_COLLECTING        '('
#define EOG_FINISH_COLLECTING       ')'
#define EOG_SOFT_RESET              'v'
#define EOG_CHANNEL_CONFIG          'x'
#define EOG_SD_OPEN                 'S'
#define EOG_SD_CLOSE                'j'
#define EOG_SD_RESET_FILENAME       '_'

#define BUFFER_SIZE 60
#define UPLOADING_STEP 16

enum Position {
    Left = 0x0001,
    Right = 0x0002,
    Top = 0x0004,
    Bottom = 0x0008,
    Center = 0x0010
};

class EOGBoard {
    public:
        EOGBoard();

        bool processChar(char c);
        void updateChannelData();
        void sendChannelData();

        // Variables
        unsigned char horizontalChannel;
        unsigned char verticalChannel;
        Position position;

        bool streaming;

        unsigned long timestamp;
        unsigned long sampleNumber;
        unsigned long horizontalSample;
        unsigned long verticalSample;

    private:
        void start();
        void stop();

        SDCard sd;

        unsigned char command;
        unsigned char params;
        unsigned char selectedChannel;

        unsigned char bufferCursor;
        char buffer[BUFFER_SIZE];
};

extern EOGBoard eogBoard;

#endif
