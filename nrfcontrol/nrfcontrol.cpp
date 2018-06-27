#include <iostream>
#include <string>
#include <iomanip>
#include <string>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include "nrflib.h"

using namespace std;

uint64_t parseAddress(char * address);
void printUsage(char * argc[]);


// radio settings see datasheet
// full power
#define SET_RF_SETUP 	RF_PWR
// Just an address I picked
uint64_t rx_addr_p0 = 0xf0f0f0f001;
uint64_t tx_addr = 0xf0f0f0f001;
// Turn on dynamic payload for all pipes
#define SET_DYNPD 	0x3f
// Turn on dynamic payload
#define SET_FEATURE 	EN_DPL
// RF Channel in a clear location
int rf_ch = 110;
// CRC on
#define SET_CONFIG 	EN_CRC
// Retry 15 times
#define SET_SETUP_RETR 	0x0f


/*
 * OK we have a few things to do here.
 * 1 - Figure out how the freak to set up a socket so that we can communicate from
 * 		this program.
 * 2 - Figure out how the freak to check the socket and check to see if we have received 
 * 		messages from the radio
 * 3 - Store the messages somehow. Don't do a buffer overflow in case nothing gets checked.
 * 		Just make it override the buffer after so many messages.
 * Berkeley Sockets (POSIX sockets) - a file in the tmp directory perhaps? That's what 
 *   Mysql does
 * Unix Domain Socket maybe - what the file looking socket is called
 * http://beej.us/guide/bgipc/output/html/multipage/unixsock.html
 * 
 */

int main (int argc, char * argv[]) {

    int opt, temp, loop, diag, noReceive,noTransmit;
    loop = 0;
    diag = 0;
    noReceive = 0;
    noTransmit = 0;
    uint64_t tempAddress = 0;
    int receiveTimeout = 50000;
    int seconds = 1;
    while ((opt = getopt(argc, argv, "r:t:w:f:l:d:n:s:p")) != -1) {
        switch (opt) {
            case 'r':
                tempAddress = parseAddress(optarg);
                if(tempAddress == EXIT_FAILURE) {
                    printUsage(argv);
                    exit(EXIT_FAILURE);
                }
                rx_addr_p0 = tempAddress;
                break;
            case 't':
                tempAddress = parseAddress(optarg);
                if(tempAddress == EXIT_FAILURE) {
                    printUsage(argv);
                    exit(EXIT_FAILURE);
                }
                tx_addr = tempAddress;
                break;
            case 'f':
                temp = atoi(optarg);
                if(temp > 0)
                    rf_ch = temp;
                break;
            case 'l':
                loop = atoi(optarg);
                break;
            case 'd':
                diag = 1;
                break;
            case 'n':
                noReceive = 1;
                break;
            case 's':
                receiveTimeout = 50;
                break;
            case 'w':
                temp = atoi(optarg);
                cout << temp;
                if(temp > 0)
                    seconds = temp;
                noTransmit = 1;
                break;
            case 'p':
                noTransmit = 1;
                break;
            default:
                printUsage(argv);
                exit(EXIT_FAILURE);
        }
    }

    char command[40];
    command[0] = 0;
    int size;
    if (optind >= argc) {
        time_t t = time(0);
        char time[100];
        strftime( time, sizeof(time), "%m%d%Y%H%M%S", localtime(&t));
        strcat(command, "TI:");
        strcat(command, time);
        size = 17;
    } else {
        size = strlen(argv[optind]);
        if(size > 32) {
            cout << "That's waaaaay to much for me to send.  Sorry. 32 is my limit.\n";
            printUsage(argv);
            return EXIT_FAILURE;
        }
        strcat(command,argv[optind]);
    }
    nrfInit();
    writeAddr(TX_ADDR,tx_addr);
    writeAddr(RX_ADDR_P0,rx_addr_p0); 
    writeReg(RF_SETUP,SET_RF_SETUP);
    writeReg(DYNPD,SET_DYNPD);
    writeReg(FEATURE,SET_FEATURE);
    writeReg(RF_CH,rf_ch);
    writeReg(CONFIG,SET_CONFIG);
    writeReg(SETUP_RETR,SET_SETUP_RETR);
    startRadio();
    flushme();
    receiveTimeout = receiveTimeout * seconds;
    if(loop == 0)
        loop = 1;
    while(loop > 0) {
        if (noTransmit == 0) {
            cout << "Transmitting...";
            int txWorked = transmit(command, size);
            if(txWorked) 
                cout << "ok\n";
            else
                cout << "fail\n";
        }
        char message[33];
        message[0] = 0;
        int messageSize;
        if(noReceive == 0) {
            startRx();
            for(int x=0;x< receiveTimeout ;x++) {
                messageSize = dynReceive(message);
                if(messageSize > 0) {
                    cout << message << "\n";
                }
            }
        }
        //if(!txWorked) 
        //	dumpALittle();
        stopRx();
        if(loop > 1)
            usleep(1000);
        loop --;
    }
    if(diag == 1)
        dumpMe();
    stopRadio();
    nrfClose();
}

uint64_t parseAddress(char * address) {
    int length = strlen(address);
    if(length<10)
        return EXIT_FAILURE;
    char piece[] = "00";
    int x;
    long intPiece;
    uint64_t outputAddress = 0;
    for(x = 0; x < 10; x+=2) {
        outputAddress <<= 8;
        piece[0] = address[x];
        piece[1] = address[(x+1)];
        intPiece = strtol(piece,NULL,16);
        outputAddress |= intPiece;
    }
    if (outputAddress == 0)
        return EXIT_FAILURE;
    return outputAddress;
}


void printUsage(char * argv[]) {
    cerr << "Usage: " << argv[0] << " [-r hex receive addr.] [-t hex transmit addr.] [-f freq.] [-l loop(repeat)] [-d(diagnose)] [-n(no receive)] [-s(short receive)] [-p (print only)] command\n";
}
