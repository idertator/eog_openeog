#include "BoardSPI.h"

#include "Definitions.h"

BoardSPI::BoardSPI() {

}


void BoardSPI::initialize() {
    spi.begin();
    spi.setSpeed(4000000);      // use 4MHz for ADS and LIS3DH
    spi.setMode(DSPI_MODE0);    // default to SD card mode!
}


void BoardSPI::activateADS() {
    spi.setMode(DSPI_MODE1);
    spi.setSpeed(4000000);
    digitalWrite(BOARD_ADS, LOW);
}


void BoardSPI::deactivateADS() {
    digitalWrite(BOARD_ADS, HIGH);
    spi.setSpeed(20000000);
    spi.setMode(DSPI_MODE0); // DEFAULT TO SD MODE!
}


void BoardSPI::activateSD() {
    spi.setMode(DSPI_MODE0);
    spi.setSpeed(20000000);
    digitalWrite(SD_SS, LOW);
}


void BoardSPI::deactivateSD() {
    digitalWrite(SD_SS, HIGH);
}


unsigned char BoardSPI::send(unsigned char value) {
    return spi.transfer(value);
}


BoardSPI bSPI;
