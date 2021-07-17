#include <Arduino.h>

void __USER_ISR ADS_DRDY_Service(void);

class ADS1299 {
    public:
        ADS1299();

        unsigned char deviceID();

        void updateChannelData();
        int getChannel(unsigned char channel);

        void begin();
        void start();
        void stop();

        void changeChannelLeadOffDetect(unsigned char N);
        void activateChannel(unsigned char N);
        void deactivateChannel(unsigned char N);
        void writeChannelSettings();
        void writeChannelSettings(unsigned char N);

        unsigned char dataRaw[24];
        volatile bool channelDataAvailable;
        unsigned char channelSettings[8][6];

    private:
        bool boardUseSRB1;
        bool useInBias[8];
        bool useSRB2[8];
        unsigned char leadOffSettings[8][2];

        int boardStat;

        void RDATAC();
        void SDATAC();
        void START();
        void STOP();
        unsigned char RREG(unsigned char addr);
        void WREG(unsigned char, unsigned char);
};

extern ADS1299 ads;
