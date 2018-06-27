#ifndef _NRF_H_
#define _NRF_H_

#include <cstdlib>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <stdint.h>
#include <pigpio.h>
#include <unistd.h>
#include "nRF24L01.h"


// Pin definitions for chip select and chip enabled of the MiRF module
#define CE_PIN    	25 // gpio pin not header pin
#define CSN_PIN   	0 // for csn0

#define SPI_MODE 	0
#define SPI_BAUD	1000000



#ifndef DATA_SIZE
#define DATA_SIZE 32
#endif

void nrfInit(void);
void nrfClose(void);
void startRadio(void);
void stopRadio(void);
void powerUp(void);
void powerDown(void);
void startRx(void);
void stopRx(void);
char readReg(char reg);
void writeReg(char reg, char data);
uint64_t readAddr( char reg);
void writeAddr( char reg, uint64_t address);
uint8_t getLength(void);
uint8_t checkRx(void);
uint8_t dynReceive(char * payload);
uint8_t receive(char * payload);
uint8_t transmit(char * payload, uint8_t datasize);
void flushme(void);
void dumpMe(void);
void dumpALittle(void);

#endif // _NRF_H_
