#include "Messages.h"

Messages::Messages() {

}

void Messages::send(char value) {
    Serial0.print(value);
}

void Messages::send(const char* value) {
    if (value != NULL) {
        for (int i = 0; i < strlen(value); i++)
            Serial0.print(value[i]);
    }
}

void Messages::sendBinary(unsigned short value) {
    Serial0.write(value >> 8);
    Serial0.write(value & 0xFF);
}

void Messages::sendPackage(const char* value, int length) {
    Serial0.write(value, length);
}

void Messages::sendHex(unsigned char value) {
    if (value < 0x10)
        Serial0.print('0');
    char buf[4];
    itoa(value, buf, HEX);
    for (int i = 0; i < strlen(buf); i++)
        Serial0.print(buf[i]);
}

void Messages::sendEOM() {
    send("$$$");
}

void Messages::sendVersion(unsigned char adsDeviceID) {
    send("OpenEOG v1.0.0 - ADS1299 Device ID: 0x");
    sendHex(adsDeviceID);
    sendEOM();
}

Messages msg;
