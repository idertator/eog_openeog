#include <DSPI.h>

class BoardSPI {
    public:
        BoardSPI();

        void initialize();

        void activateADS();
        void deactivateADS();
        void activateSD();
        void deactivateSD();

        unsigned char send(unsigned char value);

        DSPI0 spi;
};

extern BoardSPI bSPI;
