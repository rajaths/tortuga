#include <p30fxxxx.h>
#include "buscodes.h"
#include <stdio.h>

#define SENSORBOARD_IC1
#include "uart.c"

_FOSC( CSW_FSCM_OFF & ECIO );
_FWDT ( WDT_OFF );


#define TRIS_OUT 0
#define TRIS_IN  1
#define byte unsigned char

/*
 * Bus = D1 D0 E5-E0
 * Akn = D3
 * RW  = E8
 */

/* Bus pin assignments */
#define IN_AKN      _RD3
#define LAT_AKN     _LATD3
#define TRIS_AKN    _TRISD3

#define IN_RW       _RE8
#define TRIS_RW     _TRISE8
#define LAT_RW      _LATE8


#define IN_KS       _RF0
#define TRIS_KS     _TRISF0



#define RW_READ     0
#define RW_WRITE    1


#define SLAVE_ID_POWERBOARD 1
#define SLAVE_ID_DEPTH      0
#define SLAVE_ID_THRUSTERS  0
#define SLAVE_ID_MARKERS    0
#define SLAVE_ID_TEMP       1
#define SLAVE_ID_LCD        2
#define SLAVE_ID_HARDKILL   1
#define SLAVE_ID_SONAR	    3

#define SLAVE_ID_MM1        0
#define SLAVE_ID_MM2        2
#define SLAVE_ID_MM3        2

#define SLAVE_MM1_WRITE_CMD BUS_CMD_SETSPEED_U1
#define SLAVE_MM2_WRITE_CMD BUS_CMD_SETSPEED_U2
#define SLAVE_MM3_WRITE_CMD BUS_CMD_SETSPEED_U1

#define SLAVE_MM1_READ_CMD  BUS_CMD_GETREPLY_U1
#define SLAVE_MM2_READ_CMD  BUS_CMD_GETREPLY_U2
#define SLAVE_MM3_READ_CMD  BUS_CMD_GETREPLY_U1

/*
 * Bus Constants
 * BUS_TIMEOUT - how many iterations to wait when waiting for AKN to change state
 *
 * BUS_ERROR   - AKN failed to go high when talking to Slave. Most likely indicates a
 *               Slave fault. Can also mean that some Slave is forcibly holding AKN
 *               low, but this is very unlikely to happen.
 *
 * BUS_FAILURE - A Slave is holding the AKN line high, preventing any further bus
 *               operations. This is catastrophic failure and a Reset may be needed.
 *               It may be possible to retry (in case Slave bus code got interrupted
 *               in the middle of an operation, but this is extremely unlikely (and
 *               should be avoided by disabling that interrupt anyway).
 */
#define BUS_TIMEOUT     25000
#define BUS_ERROR       -1
#define BUS_FAILURE     -2

#define DIAG_TIMEOUT     25000
#define FAILSAFE_TIMEOUT 75000


/* No sonar? */
#define NUM_SLAVES  3

static const unsigned char hkSafety[]={0xDE, 0xAD, 0xBE, 0xEF, 0x3E};
static const unsigned char tkSafety[]={0xB1, 0xD0, 0x23, 0x7A, 0x69};
static const unsigned char cdSafety[]={0xBA, 0xDB, 0xEE, 0xEF, 0x4A};


void processRuntimeDiag();
void stopThrusters();

/* Read byte from bus */
byte readBus()
{
    return (PORTE & 0x3F) | (_RD0 << 6) | (_RD1 << 7);
}


/* Take bus out of high-impedance state and write a byte to it */
void writeBus(byte b)
{
    TRISE = TRISE & 0xFFC0;
    _TRISD1 = TRIS_OUT;
    _TRISD0 = TRIS_OUT;

     LATE = (LATE & 0xFFC0) | (b & 0x3F);
    _LATD0 = (b & 0x40) >> 6;
    _LATD1 = (b & 0x80) >> 7;
}


/* Put bus in high-impedance state */
void freeBus()
{
    _TRISD1 = TRIS_IN;
    _TRISD0 = TRIS_IN;
    TRISE = TRISE | 0x3F;
}

byte diagMsg = 1;
byte failsafeExpired = 0;

/* Wait for a byte on the serial console */
unsigned char waitchar(byte timeout)
{
    long waitTime=0;
    long failsafeTime=0;
    byte x;



    U1STAbits.OERR = 0;
    U1STAbits.FERR = 0;
    U1STAbits.PERR = 0;
    U1STAbits.URXDA = 0;

    while(U1STAbits.URXDA == 0)
    {
        if(diagMsg && waitTime++ == DIAG_TIMEOUT)
        {
            processRuntimeDiag();
            waitTime=0;
        }

        if(failsafeTime++ == FAILSAFE_TIMEOUT)
        {
            if(!failsafeExpired)
            {
                failsafeExpired = 1;
                stopThrusters();
            }
            failsafeTime = 0;
        }
    }

    x = U1RXREG;
    U1STAbits.URXDA = 0;
    return x;
}


/* Initialize bus */
void initBus()
{
    /* Put everything in high-impedance state */
    freeBus();
    TRIS_RW = TRIS_OUT;
    TRIS_AKN = TRIS_IN;
}


/* Set the given Slave's Req line to the given value */
void setReq(byte req, byte val)
{
    if(req == 0)
        _LATB0 = val;

    if(req == 1)
        _LATB1 = val;

    if(req == 2)
        _LATB2 = val;

    if(req == 3)
        _LATB3 = val;
}


/* Read a byte from a given Slave */
/* Returns BUS_ERROR or BUS_FAILURE on error */
int busReadByte(byte req)
{
    byte data=0;
    long timeout = 0;

    /* Set RW to read */
    LAT_RW = RW_READ;

    /* Raise Req */
    setReq(req, 1);

    /* Wait for AKN to go high */
    /* Need timeout to detect Slave fault */
    while(IN_AKN == 0)
    {
        if(timeout++ == BUS_TIMEOUT)
        {
            setReq(req, 0);
            return BUS_ERROR;
        }
    }

    /* Read the data */
    data = readBus();

    /* Drop Req */
    setReq(req, 0);

    /* Wait for Slave to release bus */
    timeout=0;
    while(IN_AKN == 1)
    {
        if(timeout++ == BUS_TIMEOUT)
            return BUS_FAILURE;     /* We're totally screwed */
    }

    return data;
}


/* Write a byte to a given slave */
/* Returns BUS_ERROR or BUS_FAILURE on error */
int busWriteByte(byte data, byte req)
{
    long timeout=0;

    /* Set RW to write */
    LAT_RW = RW_WRITE;

    /* Put the data on the bus */
    writeBus(data);

    /* Raise Req */
    setReq(req, 1);

    /* Wait for AKN to go high */
    /* Need timeout to detect Slave fault */
    while(IN_AKN == 0)
    {
        if(timeout++ == BUS_TIMEOUT)
        {
            setReq(req, 0);
            freeBus();
            return BUS_ERROR;
        }
    }

    /* Release bus */
    freeBus();

    /* Drop Req */
    setReq(req, 0);

    /* Wait for Slave to release bus */
    timeout=0;
    while(IN_AKN == 1)
    {
        if(timeout++ == BUS_TIMEOUT)
            return BUS_FAILURE;     /* We're totally screwed */
    }

    return 0;
}


void initMasterUart()
{
    U1MODE = 0x0000;
//    U1BRG = 15;  /* 7 for 230400, 15 for 115200 194 for 9600  AT 30 MIPS*/
    U1BRG = MASTER_U1_BRG;  /* 7 for 115200 at 15 MIPS */
    U1MODEbits.ALTIO = 1;   // Use alternate IO
    U1MODEbits.UARTEN = 1;
    U1STAbits.UTXEN = 1;   // Enable transmit
}


/* Send a byte to the serial console */
void sendByte(byte i)
{
    while(U1STAbits.UTXBF);
    while(!U1STAbits.TRMT);
    U1TXREG = i;
    while(U1STAbits.UTXBF);
    while(!U1STAbits.TRMT);
}


/* Send a string to the serial console */
void U2SendString(unsigned char str[])
{
    byte i=0;
    for(i=0; str[i]!=0; i++)
        U2WriteByte(str[i]);
}


/* General purpose bus receive buffer */
byte rxBuf[60];


/*
 * Read data from bus into rxBuf and return number of bytes read.
 * Returns BUS_ERROR or BUS_FAILURE on error
 */
int readDataBlock(byte req)
{
    int rxPtr, rxLen, rxData;
    rxBuf[0]=0;
    rxLen = busReadByte(req);

    if(rxLen < 0)
        return rxLen;

    for(rxPtr=0; rxPtr<rxLen; rxPtr++)
    {
        rxData = busReadByte(req);

        if(rxData < 0)
            return rxData;

        rxBuf[rxPtr] = rxData;
    }

    rxBuf[rxLen]=0;
    return rxLen;
}


void showString(unsigned char str[], int line)
{
    int i;
    for(i=0; i<str[i]!=0; i++)
    {
        busWriteByte(BUS_CMD_LCD_WRITE, SLAVE_ID_LCD);
        busWriteByte(line*16+i, SLAVE_ID_LCD);
        busWriteByte(str[i], SLAVE_ID_LCD);
    }
    busWriteByte(BUS_CMD_LCD_REFRESH, SLAVE_ID_LCD);
}

byte pollStatus()
{
    if(busWriteByte(BUS_CMD_BOARDSTATUS, SLAVE_ID_POWERBOARD) != 0)
    {
        showString("STA FAIL   ", 1);
        return 255;
    }

    byte len = readDataBlock(SLAVE_ID_POWERBOARD);

    if(len!=1)
    {
        showString("STA FAIL   ", 1);
        return 255;
    }

    rxBuf[0] &= 0xFD;   /* Clear the reported kill switch bit */


    /* We report the kill switch our own way */
    if(IN_KS == 1)
        rxBuf[0] |= 0x02;

    return rxBuf[0];
}


byte pollThrusterState()
{
    if(busWriteByte(BUS_CMD_THRUSTER_STATE, SLAVE_ID_THRUSTERS) != 0)
    {
        showString("TSTA FAIL  ", 1);
        return 0;
    }

    byte len = readDataBlock(SLAVE_ID_THRUSTERS);

    if(len!=1)
    {
        showString("TSTA FAIL  ", 1);
        return 0;
    }

    if(IN_KS == 1)
        rxBuf[0] |= 0x10;

    return rxBuf[0];
}

byte tsValue=255;

/* Run a bit of the run-time diagnostic message system */
void processRuntimeDiag()
{
    byte t;
    unsigned char tmp[16];
    t = pollThrusterState();

    if(tsValue != t)    /* A change */
    {

        switch(t)
        {
            case 0x00:
            {
                showString("Vehicle Safe    ", 1);
                break;
            }

            case 0x1F:  /* Thrusters enabled and magnet attached */
            {
                showString("Vehicle Enabled ", 1);
                break;
            }

            case 0x0F:  /* Thrusters enabled by sensor board, but no magnet */
            {
                showString("No Kill Switch  ", 1);
                break;
            }

            case 0x10:  /* Magnet attached but thrusters disabled by sensor board */
            {
                showString("Safe only in SW ", 1);
                break;
            }


            default:
            {
                sprintf(tmp, "TS: %c%c%c%c%c       ",
                    (t & 0x10) ? 'K' : '-',
                    (t & 0x08) ? '1' : '-',
                    (t & 0x04) ? '2' : '-',
                    (t & 0x02) ? '3' : '-',
                    (t & 0x01) ? '4' : '-');

                if(t & 0x10)
                {
                    sprintf(tmp+10, "UNSAFE");
                }

                showString(tmp, 1);
            }

        }

        tsValue=t;
    }

}


void showBootDiag(int mode)
{
    unsigned char tmp[16];
    if(mode == 0)
    {
        //sprintf(tmp, "Status: %02X      ", pollStatus());

        byte sta = pollStatus();
        sprintf(tmp, "Sta: %02X %c%c%c%c%c%c%c%c", sta,
            (sta & 0x80) ? 'S' : '-',
            (sta & 0x40) ? '?' : '-',
            (sta & 0x20) ? '1' : '-',
            (sta & 0x10) ? '2' : '-',
            (sta & 0x08) ? '3' : '-',
            (sta & 0x04) ? '4' : '-',
            (sta & 0x02) ? 'K' : '-',
            (sta & 0x01) ? 'W' : '-');

        showString(tmp, 1);
    }

    if(mode == 1)
    {

        if(busWriteByte(BUS_CMD_DEPTH, SLAVE_ID_DEPTH) != 0)
        {
            showString("DEPTH FAIL      ", 1);
            return;
        }

        int len = readDataBlock(SLAVE_ID_DEPTH);

        if(len != 2)
        {
            showString("DEPTH LEN FAIL", 1);
            return;
        }

        sprintf(tmp, "Depth: %02X %02X     ", rxBuf[0], rxBuf[1]);
        showString(tmp, 1);
    }

    if(mode == 2)
    {
        if(busWriteByte(BUS_CMD_TEMP, SLAVE_ID_TEMP) != 0)
        {
            showString("TEMP FAIL       ", 1);
            return;
        }

        int len = readDataBlock(SLAVE_ID_TEMP);

        if(len != 5)
        {
            showString("TEMP LEN FAIL   ", 1);
            return;
        }

        sprintf(tmp, "T:%02X %02X %02X %02X %02X", rxBuf[0], rxBuf[1], rxBuf[2], rxBuf[3], rxBuf[4]);
        showString(tmp, 1);
    }
}

void showIdent()
{
    byte i=0;
    long j=0;
    unsigned char tmp[16];

    while(!(pollStatus() & 0x80));

    for(i=0; i<NUM_SLAVES; i++)
    {
        sprintf(tmp, "Ident IRQ %d:    ", i);
        showString(tmp, 0);

        /* Don't mix the strings */
        for(j=0; j<17; j++)
            rxBuf[j]=0;

        if(busWriteByte(BUS_CMD_ID, i) != 0)
        {
            showString("<Write Fail>    ", 1);
        } else
        {
            byte len = readDataBlock(i);

            if(len > 0)
            {
                for(j=len; j<16; j++)
                    rxBuf[j]=32;

                showString(rxBuf, 1);
            } else
            {
                showString("<Read Fail>     ", 1);
            }
        }

        while(pollStatus() & 0x80);
        while(!(pollStatus() & 0x80));
    }
    showString("Diagnostic Mode ", 0);
}

void diagBootMode()
{
    byte mode=0;
//    unsigned char tmp[16];
    long j=0;

    showString("Diagnostic Mode ", 0);
    while(pollStatus() & 0x80);

    while(1)
    {
        if(pollStatus() & 0x80)
        {
            mode++;
            if(mode == 3)
            {
                showIdent();
                mode = 0;
            }
            showBootDiag(mode);

            j=0;
            while(pollStatus() & 0x80)
            {
                j++;
                if(j == 25000)
                {
                    return;
                }
            }
        }
        showBootDiag(mode);
    }
}


void stopThrusters()
{
    busWriteByte(SLAVE_MM1_WRITE_CMD, SLAVE_ID_MM1);
    busWriteByte(0, SLAVE_ID_MM1);
    busWriteByte(0, SLAVE_ID_MM1);

    busWriteByte(SLAVE_MM2_WRITE_CMD, SLAVE_ID_MM2);
    busWriteByte(0, SLAVE_ID_MM2);
    busWriteByte(0, SLAVE_ID_MM2);

    busWriteByte(SLAVE_MM3_WRITE_CMD, SLAVE_ID_MM3);
    busWriteByte(0, SLAVE_ID_MM3);
    busWriteByte(0, SLAVE_ID_MM3);

    UARTSendSpeed(U2_MM_ADDR, rxBuf[6], rxBuf[7], 1);
}

int main(void)
{
    long j=0;
//    long t=0, b=0;
    byte i;

//    byte tmp[60];
//    byte rxPtr = 0;
//    byte rxLen = 0;

    TRIS_KS = TRIS_IN;

    initBus();

    for(i=0; i<NUM_SLAVES; i++)
        setReq(i, 0);

    ADPCFG = 0xFFFF;
    LATB = 0;
    TRISB = 0;


    initMasterUart();
    initInterruptUarts();


    for(j=0; j<25000; j++);


    unsigned char emptyLine[]="                ";

    showString(emptyLine, 0);
    showString(emptyLine, 1);

    for(j=0; j<25000; j++);

    showString("Diagnostic?", 0);

    for(j=0; j<25000 && ((pollStatus() & 0x80) == 0); j++);

    if(pollStatus() & 0x80)
        diagBootMode();


    showString("Starting up...  ", 0);
    showString("                ", 1);

    while(1)
    {
        byte c = waitchar(0);

        long t1, t2;

        switch(c)
        {
            case HOST_CMD_SYNC:
            {
                sendByte(HOST_REPLY_SUCCESS);
                break;
            }


            case HOST_CMD_PING:
            {
                t1 = waitchar(1);
                if(t1 == HOST_CMD_PING)
                    sendByte(HOST_REPLY_SUCCESS);
                else
                    sendByte(HOST_REPLY_BADCHKSUM);

                break;
            }


            case HOST_CMD_SYSCHECK:
            {
                byte err=0;
                t1 = waitchar(1);

                if(t1 != HOST_CMD_SYSCHECK)
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                for(i=0; i<NUM_SLAVES; i++)
                {
                    switch(busWriteByte(BUS_CMD_PING, i))
                    {
                        case BUS_ERROR:
                            err++;
                        break;

                        case BUS_FAILURE:
                            err++;
                        break;

                        case 0:
                        {
                            byte len = readDataBlock(i);

                            switch(len)
                            {
                                case 0:
                                break;

                                case BUS_ERROR:
                                case BUS_FAILURE:
                                default:
                                    err++;
                            }
                        }
                        break;
                    }

                }

                if(err == 0)
                    sendByte(HOST_REPLY_SUCCESS);
                else
                    sendByte(HOST_REPLY_FAILURE);

                break;
            }


            case HOST_CMD_DEPTH:
            {
                t1 = waitchar(1);
                if(t1 != HOST_CMD_DEPTH)
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                if(busWriteByte(BUS_CMD_DEPTH, SLAVE_ID_DEPTH) != 0)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                int len = readDataBlock(SLAVE_ID_DEPTH);

                if(len != 2)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                sendByte(HOST_REPLY_DEPTH);
                sendByte(rxBuf[0]);
                sendByte(rxBuf[1]);
                byte cs = HOST_REPLY_DEPTH+rxBuf[0]+rxBuf[1];
                sendByte(cs);
                break;
            }

            case HOST_CMD_THRUSTERSTATE:
            {
                t1 = waitchar(1);
                if(t1 != HOST_CMD_THRUSTERSTATE)
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                if(busWriteByte(BUS_CMD_THRUSTER_STATE, SLAVE_ID_THRUSTERS) != 0)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                int len = readDataBlock(SLAVE_ID_THRUSTERS);

                if(len != 1)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                sendByte(HOST_REPLY_THRUSTERSTATE);
                sendByte(rxBuf[0]);
                byte cs = HOST_REPLY_THRUSTERSTATE+rxBuf[0];
                sendByte(cs);
                break;
            }


            case HOST_CMD_BOARDSTATUS:
            {
                t1 = waitchar(1);
                if(t1 != HOST_CMD_BOARDSTATUS)
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                if(busWriteByte(BUS_CMD_BOARDSTATUS, SLAVE_ID_POWERBOARD) != 0)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                byte len = readDataBlock(SLAVE_ID_POWERBOARD);

                if(len!=1)
                {
                    sendByte(HOST_REPLY_FAILURE);
                } else
                {

                    rxBuf[0] &= 0xFD; // Clear kill switch bit

                    // Set kill switch bit based on the GPIO kill input
                    if(IN_KS == 1)
                        rxBuf[0] |= 0x02;

                    sendByte(HOST_REPLY_BOARDSTATUS);
                    sendByte(rxBuf[0]);
                    sendByte(HOST_REPLY_BOARDSTATUS+rxBuf[0]);
                }

                break;
            }


            case HOST_CMD_HARDKILL:
            {

                for(i=0; i<5; i++)
                    rxBuf[i] = waitchar(1);

                byte cflag=0;

                for(i=0; i<5; i++)
                {
                    if(rxBuf[i] != hkSafety[i])
                        cflag=1;
                }

                if(cflag == 1)
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                } else
                {
                    if(busWriteByte(BUS_CMD_HARDKILL, SLAVE_ID_HARDKILL) != 0)
                    {
                        sendByte(HOST_REPLY_FAILURE);
                        break;
                    }
                    sendByte(HOST_REPLY_SUCCESS);
                }
                break;
            }


            case HOST_CMD_MARKER:
            {
                t1 = waitchar(1);
                t2 = waitchar(1);

                if((t1 != 0 && t1 != 1) || (t1+HOST_CMD_MARKER != t2))
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                if(busWriteByte(t1==0 ? BUS_CMD_MARKER1 : BUS_CMD_MARKER2, SLAVE_ID_MARKERS) != 0)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                sendByte(HOST_REPLY_SUCCESS);
                break;
            }


            case HOST_CMD_BACKLIGHT:
            {
                t1 = waitchar(1);
                t2 = waitchar(1);

                const static unsigned char blCommands[]=
                        {BUS_CMD_LCD_LIGHT_OFF, BUS_CMD_LCD_LIGHT_ON, BUS_CMD_LCD_LIGHT_FLASH};

                if((t1 != 0 && t1 != 1 && t1 != 2) || (t1+HOST_CMD_BACKLIGHT != t2))
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                if(busWriteByte(blCommands[t1], SLAVE_ID_LCD) != 0)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                sendByte(HOST_REPLY_SUCCESS);
                break;
            }


            case HOST_CMD_THRUSTERS:
            {
                for(i=0; i<5; i++)
                    rxBuf[i] = waitchar(1);

                t1 = waitchar(1);
                t2 = waitchar(1);

                byte cflag=0;
                byte cs=0;

                // Check the special sequence
                for(i=0; i<5; i++)
                {
                    cs += rxBuf[i];
                    if(rxBuf[i] != tkSafety[i])
                        cflag=1;
                }

                cs += t1 + HOST_CMD_THRUSTERS;


                const static unsigned char tkCommands[]=
                {
                    BUS_CMD_THRUSTER1_OFF, BUS_CMD_THRUSTER2_OFF,
                    BUS_CMD_THRUSTER3_OFF, BUS_CMD_THRUSTER4_OFF,
                    BUS_CMD_THRUSTER1_ON, BUS_CMD_THRUSTER2_ON,
                    BUS_CMD_THRUSTER3_ON, BUS_CMD_THRUSTER4_ON
                };

                if(cflag == 1 || t1 > 7 || (t2 != cs))
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                } else
                {
                    if(busWriteByte(tkCommands[t1], SLAVE_ID_THRUSTERS) != 0)
                    {
                        sendByte(HOST_REPLY_FAILURE);
                        break;
                    }
                }
                sendByte(HOST_REPLY_SUCCESS);
                break;
            }


            case HOST_CMD_TEMPERATURE:
            {
                t1 = waitchar(1);
                if(t1 != HOST_CMD_TEMPERATURE)
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                if(busWriteByte(BUS_CMD_TEMP, SLAVE_ID_TEMP) != 0)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                int len = readDataBlock(SLAVE_ID_TEMP);

                if(len != 5)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                sendByte(HOST_REPLY_TEMPERATURE);

                byte cs=0;

                for(i=0; i<5; i++)
                {
                    cs += rxBuf[i];
                    sendByte(rxBuf[i]);
                }

                sendByte(cs + HOST_REPLY_TEMPERATURE);
                break;
            }


            case HOST_CMD_PRINTTEXT:
            {
                t1 = waitchar(1);
                byte cs=HOST_CMD_PRINTTEXT+t1;

                for(i=0; i<16; i++)
                {
                    rxBuf[i] = waitchar(1);
                    cs += rxBuf[i];
                }
                t2 = waitchar(1);

                if(t2 != cs || t1 > 1)
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                int err=0;

                for(i=0; i<16 && err==0; i++)
                {
                    err+=busWriteByte(BUS_CMD_LCD_WRITE, SLAVE_ID_LCD);
                    err+=busWriteByte(t1*16+i, SLAVE_ID_LCD);
                    err+=busWriteByte(rxBuf[i], SLAVE_ID_LCD);
                }

                err+=busWriteByte(BUS_CMD_LCD_REFRESH, SLAVE_ID_LCD);

                if(err != 0)
                    sendByte(HOST_REPLY_FAILURE);
                else
                    sendByte(HOST_REPLY_SUCCESS);

                break;
            }


            case HOST_CMD_SONAR:
            {
                t1 = waitchar(1);
		        byte cs=HOST_CMD_SONAR+t1;

                if(t1 != HOST_CMD_SONAR)
		        {
			        sendByte(HOST_REPLY_BADCHKSUM);
			        break;
                }

		        if(busWriteByte(BUS_CMD_SONAR, SLAVE_ID_SONAR) != 0)
		        {
			        sendByte(HOST_REPLY_FAILURE);
        			break;
                }


                int len = readDataBlock(SLAVE_ID_SONAR);
                if(len != 5)
		        {
			        sendByte(HOST_REPLY_FAILURE);
			        break;
		        }

		        sendByte(HOST_REPLY_SONAR);

		        cs=0;
                for(i=0; i<5; i++)
                {
                    cs += rxBuf[i];
	                sendByte(rxBuf[i]);
		        }

		        sendByte(cs + HOST_REPLY_SONAR);
		        break;
            }

            case HOST_CMD_RUNTIMEDIAG:
            {
                t1 = waitchar(1);
                t2 = waitchar(1);

                if((t1 != 0 && t1 != 1) || (t1+HOST_CMD_RUNTIMEDIAG != t2))
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                diagMsg=t1;

                if(t1==0)
                    showString("Runtime Diag Off", 1);
                else
                    showString("Runtime Diag On ", 1);

                sendByte(HOST_REPLY_SUCCESS);
                break;
            }

            case HOST_CMD_SETSPEED:
            {
                t1 = 0; /* Error counter */

                /* 12 bytes of speed, plus checksum */
                for(i=0; i<9; i++)
                    rxBuf[i] = waitchar(1);

                for(i=0; i<8; i++)
                    t1 += rxBuf[i];

                t1 += HOST_CMD_SETSPEED;

                if(rxBuf[8] != (t1 & 0xFF))
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                failsafeExpired = 0;    /* Reset failsafe mechanism */

                t1 = 0;
                if(busWriteByte(SLAVE_MM1_WRITE_CMD, SLAVE_ID_MM1) != 0) t1++;
                if(busWriteByte(rxBuf[0], SLAVE_ID_MM1) != 0) t1++;
                if(busWriteByte(rxBuf[1], SLAVE_ID_MM1) != 0) t1++;


                if(busWriteByte(SLAVE_MM2_WRITE_CMD, SLAVE_ID_MM2) != 0) t1++;
                if(busWriteByte(rxBuf[2], SLAVE_ID_MM2) != 0) t1++;
                if(busWriteByte(rxBuf[3], SLAVE_ID_MM2) != 0) t1++;

                if(busWriteByte(SLAVE_MM3_WRITE_CMD, SLAVE_ID_MM3) != 0) t1++;
                if(busWriteByte(rxBuf[4], SLAVE_ID_MM3) != 0) t1++;
                if(busWriteByte(rxBuf[5], SLAVE_ID_MM3) != 0) t1++;

                UARTSendSpeed(U2_MM_ADDR, rxBuf[6], rxBuf[7], 1);

                if(t1 == 0)
                    sendByte(HOST_REPLY_SUCCESS);
                else
                    sendByte(HOST_REPLY_FAILURE);
                break;
           }

           case HOST_CMD_MOTOR_READ:
           {
                unsigned char resp[4];
                t1 = waitchar(1);


                if(t1 != HOST_CMD_MOTOR_READ)
                {
                    sendByte(HOST_REPLY_BADCHKSUM);
                    break;
                }

                t1 = 0;

                if(busWriteByte(SLAVE_MM1_READ_CMD, SLAVE_ID_MM1) != 0) t1++;
                if(readDataBlock(SLAVE_ID_MM1) != 1) t1++;
                resp[0] = rxBuf[0];

                if(busWriteByte(SLAVE_MM2_READ_CMD, SLAVE_ID_MM2) != 0) t1++;
                if(readDataBlock(SLAVE_ID_MM2) != 1) t1++;
                resp[1] = rxBuf[0];

                if(busWriteByte(SLAVE_MM3_READ_CMD, SLAVE_ID_MM3) != 0) t1++;
                if(readDataBlock(SLAVE_ID_MM3) != 1) t1++;
                resp[2] = rxBuf[0];

                if(U2CanRead())
                    resp[3] = U2ReadByte();
                else
                    resp[3] = 0xFF;


                if(t1 != 0)
                {
                    sendByte(HOST_REPLY_FAILURE);
                    break;
                }

                sendByte(HOST_CMD_MOTOR_REPLY);
                sendByte(resp[0]);
                sendByte(resp[1]);
                sendByte(resp[2]);
                sendByte(resp[3]);

                sendByte(HOST_CMD_MOTOR_REPLY + resp[0] + resp[1] + resp[2] + resp[3]);

                break;
            }
        }
    }
}
