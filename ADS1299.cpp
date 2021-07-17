#include "ADS1299.h"
#include "BoardSPI.h"
#include "Definitions.h"


void __USER_ISR ADS_DRDY_Service() {
    clearIntFlag(_EXTERNAL_4_IRQ); // clear the irq, or else it will continually interrupt!
    if (bitRead(PORTA, 0) == 0) {
        ads.channelDataAvailable = true;
    }
}


ADS1299::ADS1299() {
    channelDataAvailable = false;

    // Initialize channel settings
    for (int i = 0; i < 8; i++) {
        channelSettings[i][POWER_DOWN] = NO;
        channelSettings[i][GAIN_SET] = ADS_GAIN24;
        channelSettings[i][INPUT_TYPE_SET] = ADSINPUT_NORMAL;
        channelSettings[i][BIAS_SET] = YES;
        channelSettings[i][SRB2_SET] = YES;
        channelSettings[i][SRB1_SET] = NO;
        useInBias[i] = true;
        useSRB2[i] = true;
    }

    boardUseSRB1 = false;

    // Initialize Lead Off Settings
    for (int i = 0; i < 8; i ++) {
        leadOffSettings[i][PCHAN] = OFF;
        leadOffSettings[i][NCHAN] = OFF;
    }
}


unsigned char ADS1299::deviceID() {
    return RREG(0x00);
}


void ADS1299::updateChannelData() {
    unsigned char inByte;
    unsigned char byteCounter = 0;
    channelDataAvailable = false;

    bSPI.activateADS();
    boardStat = 0;
    for (unsigned char i = 0; i < 3; i ++) {
        inByte = bSPI.send(0x00);
        boardStat = (boardStat << 8) | inByte;
    }
    for (unsigned char i = 0; i < 8; i ++) {
        for (unsigned char j = 0; j < 3; j ++) {
            inByte = bSPI.send(0x00);
            dataRaw[byteCounter] = inByte;
            byteCounter ++;
        }
    }
    bSPI.deactivateADS();
}

int ADS1299::getChannel(unsigned char channel) {
    unsigned char idx = (channel - 1) * 3;
    int result = (dataRaw[idx] << 16) | (dataRaw[idx + 1] << 8) | dataRaw[idx + 2];
    return result;
}


void ADS1299::begin() {
    // Setting  pins
    pinMode(OPENBCI_PIN_LED, OUTPUT);
    digitalWrite(OPENBCI_PIN_LED, HIGH);
    pinMode(OPENBCI_PIN_PGC, OUTPUT);

    delay(10);

    // Begin ADS interrupt
    setIntVector(_EXTERNAL_4_VECTOR, ADS_DRDY_Service); // connect interrupt to ISR
    setIntPriority(_EXTERNAL_4_VECTOR, 4, 0);           // set interrupt priority and sub priority
    clearIntFlag(_EXTERNAL_4_IRQ);                      // these two need to be done together
    setIntEnable(_EXTERNAL_4_IRQ);                      // clear any flags before enabing the irq

    // Board reset
    pinMode(SD_SS, OUTPUT);
    digitalWrite(SD_SS, HIGH); // de-select SDcard if present
    pinMode(BOARD_ADS, OUTPUT);
    digitalWrite(BOARD_ADS, HIGH);

    // Initialize SPI
    bSPI.initialize();

    // Initialize ADS
    delay(50);
    pinMode(ADS_RST, OUTPUT);
    digitalWrite(ADS_RST, LOW);  // reset pin connected to both ADS ICs
    delayMicroseconds(4);        // toggle reset pin
    digitalWrite(ADS_RST, HIGH); // this will reset the board
    delayMicroseconds(20);       // recommended to wait 18 Tclk before using device (~8uS);

    pinMode(ADS_DRDY, INPUT); // we get DRDY asertion from the on-board ADS
    delay(40);

    // Reset ADS
    bSPI.activateADS();
    bSPI.send(_RESET);
    delayMicroseconds(12);
    bSPI.deactivateADS();

    SDATAC(); // exit Read Data Continuous mode to communicate with ADS
    delay(100);
    // turn off all channels
    for (unsigned char i = 1; i <= 8; i ++)
        deactivateChannel(i);

    delay(10);
    WREG(CONFIG1, (ADS1299_CONFIG1 | 4)); // 4 = Set 1000 Hz Sample Rate

    writeChannelSettings(); // write settings to the on-board ADS if present

    WREG(CONFIG3, 0b11101100);
    delay(500);

    // Configure Lead Off Detection
    unsigned char setting = RREG(LOFF); //get the current bias settings
    setting &= 0b11110000;    //clear out the last four bits
    setting |= (LOFF_MAG_6NA & 0b00001100) | (LOFF_FREQ_31p2HZ & 0b00000011); //set the amplitude
    WREG(LOFF, setting);
    delay(1); //send the modified byte back to the ADS
}

void ADS1299::start() {
    RDATAC();
    delay(1);
    START();
    delay(1);
}

void ADS1299::stop() {
    STOP();
    delay(1);
    SDATAC();
    delay(1);
}

void ADS1299::changeChannelLeadOffDetect(unsigned char N) {
    SDATAC();
    delay(1);
    unsigned char P_setting = RREG(LOFF_SENSP);
    unsigned char N_setting = RREG(LOFF_SENSN);

    if (leadOffSettings[N][PCHAN] == ON) {
        bitSet(P_setting, N - 1);
    } else  {
        bitClear(P_setting, N - 1);
    }

    if (leadOffSettings[N][NCHAN] == ON) {
        bitSet(N_setting, N - 1);
    } else {
        bitClear(N_setting, N - 1);
    }

    WREG(LOFF_SENSP, P_setting);
    WREG(LOFF_SENSN, N_setting);
}


void ADS1299::activateChannel(unsigned char N) {
    SDATAC();
    delay(1);

    unsigned char setting = 0x00;
    setting |= channelSettings[N - 1][GAIN_SET];
    setting |= channelSettings[N - 1][INPUT_TYPE_SET];

    if (useSRB2[N - 1]) {
        channelSettings[N - 1][SRB2_SET] = YES;
    } else {
        channelSettings[N - 1][SRB2_SET] = NO;
    }

    if (channelSettings[N - 1][SRB2_SET] == YES) {
        bitSet(setting, 3);
    }

    WREG(CH1SET + N - 1, setting);

    if (useInBias[N - 1]) {
        channelSettings[N - 1][BIAS_SET] = YES;
    }  else {
        channelSettings[N - 1][BIAS_SET] = NO;
    }

    setting = RREG(BIAS_SENSP);
    if (channelSettings[N - 1][BIAS_SET] == YES) {
        bitSet(setting, N - 1);
        useInBias[N - 1] = true;
    } else {
        bitClear(setting, N - 1);
        useInBias[N - 1] = false;
    }

    WREG(BIAS_SENSP, setting);
    delay(1);

    setting = RREG(BIAS_SENSN);
    if (channelSettings[N - 1][BIAS_SET] == YES) {
        bitSet(setting, N - 1);
    } else {
        bitClear(setting, N - 1);
    }

    WREG(BIAS_SENSN, setting);
    delay(1);

    setting = 0x00;
    if (boardUseSRB1) {
        setting = 0x20;;
    }
    WREG(MISC1, setting);
}


void ADS1299::deactivateChannel(unsigned char N) {
    SDATAC();
    delay(1);

    unsigned char setting = RREG(CH1SET + N - 1);
    delay(1);

    bitSet(setting, 7);
    bitClear(setting, 3);
    WREG(CH1SET + N - 1, setting);
    delay(1);

    setting = RREG(BIAS_SENSP);
    delay(1);

    bitClear(setting, N - 1);
    WREG(BIAS_SENSP, setting);
    delay(1);

    setting = RREG(BIAS_SENSN);
    delay(1);

    bitClear(setting, N - 1);
    WREG(BIAS_SENSN, setting);
    delay(1);

    leadOffSettings[N - 1][PCHAN] = leadOffSettings[N - 1][NCHAN] = NO;
    changeChannelLeadOffDetect(N);
}


void ADS1299::writeChannelSettings() {
    boolean use_SRB1 = false;
    unsigned char setting;

    SDATAC();
    delay(1);

    for (unsigned char i = 0; i < 8; i ++) {
        setting = 0x00;
        if (channelSettings[i][POWER_DOWN] == YES) {
            setting |= 0x80;
        }
        setting |= channelSettings[i][GAIN_SET];
        setting |= channelSettings[i][INPUT_TYPE_SET];

        if (channelSettings[i][SRB2_SET] == YES) {
            setting |= 0x08;
            useSRB2[i] = true;
        } else {
            useSRB2[i] = false;
        }
        WREG(CH1SET + i, setting);

        setting = RREG(BIAS_SENSP);
        if (channelSettings[i][BIAS_SET] == YES) {
            bitSet(setting, i);
            useInBias[i] = true;
        } else {
            bitClear(setting, i);
            useInBias[i] = false;
        }

        WREG(BIAS_SENSP, setting);
        delay(1);

        setting = RREG(BIAS_SENSN);
        if (channelSettings[i][BIAS_SET] == YES) {
            bitSet(setting, i);
        } else {
            bitClear(setting, i);
        }

        WREG(BIAS_SENSN, setting);
        delay(1);

        if (channelSettings[i][SRB1_SET] == YES) {
            use_SRB1 = true;
        }
    }

    if (use_SRB1) {
        for (int i = 0; i < 8; i ++) {
            channelSettings[i][SRB1_SET] = YES;
        }
        WREG(MISC1, 0x20);
        boardUseSRB1 = true;
    } else {
        for (int i = 0; i < 8; i ++) {
            channelSettings[i][SRB1_SET] = NO;
        }
        WREG(MISC1, 0x00);
        boardUseSRB1 = false;
    }
}

void ADS1299::writeChannelSettings(unsigned char N) {
    SDATAC();
    delay(1);

    unsigned char setting = 0x00;
    if (channelSettings[N - 1][POWER_DOWN] == YES) {
        setting |= 0x80;
    }
    setting |= channelSettings[N - 1][GAIN_SET];
    setting |= channelSettings[N - 1][INPUT_TYPE_SET];
    if (channelSettings[N - 1][SRB2_SET] == YES) {
        setting |= 0x08;
        useSRB2[N - 1] = true;
    } else {
        useSRB2[N - 1] = false;
    }
    WREG(CH1SET + N - 1, setting);

    setting = RREG(BIAS_SENSP);
    if (channelSettings[N - 1][BIAS_SET] == YES) {
        useInBias[N - 1] = true;
        bitSet(setting, N - 1);
    } else {
        useInBias[N - 1] = false;
        bitClear(setting, N - 1);
    }
    WREG(BIAS_SENSP, setting);
    delay(1);

    setting = RREG(BIAS_SENSN);
    if (channelSettings[N - 1][BIAS_SET] == YES) {
        bitSet(setting, N - 1);
    } else {
        bitClear(setting, N - 1);
    }
    WREG(BIAS_SENSN, setting);
    delay(1);

    if (channelSettings[N - 1][SRB1_SET] == YES) {
        for (int i = 0; i < 8; i ++) {
            channelSettings[i][SRB1_SET] = YES;
        }
        boardUseSRB1 = true;
        setting = 0x20;
    }

    if (channelSettings[N - 1][SRB1_SET] == NO) {
        for (int i = 0; i < 8; i ++) {
            channelSettings[i][SRB1_SET] = NO;
        }
        boardUseSRB1 = false;
        setting = 0x00;
    }

    WREG(MISC1, setting);
}

void ADS1299::RDATAC() {
    bSPI.activateADS();
    bSPI.send(_RDATAC);
    bSPI.deactivateADS();
    delayMicroseconds(3);
}

void ADS1299::SDATAC() {
    bSPI.activateADS();
    bSPI.send(_SDATAC);
    bSPI.deactivateADS();
    delayMicroseconds(10);
}

void ADS1299::START() {
    bSPI.activateADS();
    bSPI.send(_START);
    bSPI.deactivateADS();
}

void ADS1299::STOP() {
    bSPI.activateADS();
    bSPI.send(_STOP);
    bSPI.deactivateADS();
}


unsigned char ADS1299::RREG(unsigned char addr) {
    bSPI.activateADS();
    bSPI.send(addr + 0x20);
    bSPI.send(0x00);
    unsigned char value = bSPI.send(0x00);
    bSPI.deactivateADS();
    return value;
}

void ADS1299::WREG(unsigned char addr, unsigned char value) {
    bSPI.activateADS();
    bSPI.send(addr + 0x40);
    bSPI.send(0x00);
    bSPI.send(value);
    bSPI.deactivateADS();
}


ADS1299 ads;
