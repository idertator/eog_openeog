#include <Arduino.h>

class Messages {
    public:
        Messages();
        void send(char value);
        void send(const char* value);
        void sendPackage(const char* value, int length);
        void sendBinary(unsigned short value);
        void sendHex(unsigned char value);
        void sendEOM();

        void sendVersion(unsigned char adsDeviceID);
};

extern Messages msg;
