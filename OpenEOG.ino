#include <EEPROM.h>

#include "ADS1299.h"
#include "Board.h"

void setup() {
    // Initializing Serial0
    if (Serial0)
        Serial0.end();
    Serial0.begin(115200);

    ads.begin();
}

void loop() {
    if (eogBoard.streaming) {
        if (ads.channelDataAvailable) {
            eogBoard.updateChannelData();
            eogBoard.sendChannelData();
        }
    }
    if (Serial0.available()) {
        char newChar = Serial0.read();
        eogBoard.processChar(newChar);
    }
}
