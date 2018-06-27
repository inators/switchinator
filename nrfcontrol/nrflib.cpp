#include "nrflib.h"

using namespace std;

int spiHandle;

// connect to the radio and get the party started
void nrfInit(void){
	if(gpioInitialise() < 0) {
		cout << "Can't initialize pigpio!";
		exit(-1);
	}
	spiHandle = spiOpen(CSN_PIN,SPI_BAUD,SPI_MODE);
	if (spiHandle < 0) {
		cout << "Can't initialize spi!";
		nrfClose();
		exit(-1);
	}
	gpioSetMode(CE_PIN, PI_OUTPUT);
		
}
// close the radio and stop the party
void nrfClose(void) {
	spiClose(spiHandle);
	gpioTerminate();
}
// Power on the radio to standby mode (radio config register)
void startRadio(void) {
	char config = readReg(CONFIG);
	config |= PWR_UP;
	writeReg(CONFIG,config);
	usleep(1500); // data sheet says 1.5ms to power up
}

// Powers off the radio from standby mode
void stopRadio(void) {
	char config = readReg(CONFIG);
	config &= ~PWR_UP;
	writeReg(CONFIG,config);
	usleep(1500); // data sheet says 1.5ms to power up
}

// Powers up the radio - turns on CE pin
void powerUp(void) {
	gpioWrite(CE_PIN, 1);
	usleep(1000);

}

// Power down the radio - turn off CE pin
void powerDown(void) {
	gpioWrite(CE_PIN, 0);
	usleep(5000);

}

// Put the radio in receive mode
void startRx(void) {
	powerUp();
	char config = readReg(CONFIG);
	config |= PRIM_RX;
	writeReg(CONFIG, config);
	usleep(150); // datasheet says 130us delay here
}

// Cancel receive mode
void stopRx(void) {
	powerDown();
	char config = readReg(CONFIG);
	config &= ~PRIM_RX;
	writeReg(CONFIG, config);
	usleep(150); // datasheet says 130us delay here
}

// Read a register and return an uint8_t result
char readReg(char reg) {
	reg &= REGISTER_MASK;  // make sure command bits are clean
	reg |= R_REGISTER;
	char TxData[] = {reg, NOP};
	char RxData[2];
	spiXfer(spiHandle,TxData,RxData,2);
	unsigned char output = RxData[1];
	return output;
}

// Write a register
void writeReg(char reg, char data) {
	reg &= REGISTER_MASK;
	reg |= W_REGISTER;
	char TxData[] = {reg , data};
	char RxData[2];
	spiXfer(spiHandle,TxData,RxData,2);
}

// Read the address specified by "reg" (which address) and return an
// integer of the address (40 bytes)
uint64_t readAddr( char reg) {
	uint64_t address = 0;
	reg &= REGISTER_MASK;
	reg |= R_REGISTER;
	// set up data out to get all 5 chars
	char TxData[] = {reg, NOP, NOP, NOP, NOP, NOP};
	char RxData[6];
	// 3 addresses are 5 bytes but 4 are only 1 byte. Deal with the 4
	if ( (reg == (R_REGISTER | RX_ADDR_P2 )) ||
			(reg == (R_REGISTER | RX_ADDR_P3 )) ||
			(reg == (R_REGISTER | RX_ADDR_P4 )) ||
			(reg == (R_REGISTER | RX_ADDR_P5 ))) { // all the 1 byte returns
		TxData[0] = (R_REGISTER | RX_ADDR_P1); // MSB equal here 
		spiXfer(spiHandle, TxData, RxData, 6); // get the MSBs
		char lsb = readReg(reg);
		RxData[1] = lsb;
	} else { // just get the 5 byte address
		spiXfer(spiHandle, TxData, RxData, 6);
	}
	// nrf addresses are LSB first
	for (int x = 5; x > 1; x--) {
		address |= RxData[x];
		address = address << 8;
	}
	address |= RxData[1];
	return address;
}

// Write the address specified. 
void writeAddr( char reg, uint64_t address) {
    char fifth,fourth,third,second,first;
    if( (reg == RX_ADDR_P0) || (reg == RX_ADDR_P1) || (reg == TX_ADDR) ) {
		reg &= REGISTER_MASK;
		reg |= W_REGISTER;
		// nrf addresses are LSB first
        first = (address & 0xff);
        second = ((address >> 8) & 0xff);
        third = ((address >> 16) & 0xff);
        fourth = ((address >> 24) & 0xff);
        fifth = (address >> 32);
		char TxData[] = {reg ,first, second, third, fourth, fifth};
		char RxData[6];
		spiXfer(spiHandle, TxData, RxData, 6);
	} else {
		char lastByte = (address & 0xff);
		writeReg(reg, lastByte);
	}
}

// Returns the dynamic payload length
uint8_t getLength(void) {
	char TxData[] = {R_RX_PLD_WID, NOP};
	char RxData[2];
	uint8_t payloadLength;
	spiXfer(spiHandle, TxData, RxData, 2);
	payloadLength = RxData[1];
	// if this returns more than 32 then there is a problem. flush it
	if (payloadLength > 32) {
		TxData[0] = FLUSH_RX;
		spiXfer(spiHandle, TxData, RxData, 1);
		return 0;
	}
	return payloadLength;
}

// check and see if anything is in the receive buffer
uint8_t checkRx(void) {
	char status = readReg(STATUS);
	if ((status & 0x0E)== 0) { // FIFO status isn't empty
		return 1;
	} else {
		return 0;
	}
}

// Receive a payload with a dynamic length - returns length
// char * payload MUST BE 33 bytes. NO LESS
uint8_t dynReceive(char * payload) {
	uint8_t rxcheck = checkRx();
	if (rxcheck == 0) {
		return 0;
	}   
	//usleep(3); // small delay so entire message is ready first
	uint8_t payloadLength = getLength();
	if (payloadLength == 0) {
		return 0;
	}
	char TxData[(payloadLength + 1)];
	char RxData[(payloadLength + 1)];
	TxData[0] = R_RX_PAYLOAD;
	// set up TxData
	for (int x = 0; x < payloadLength; x++) {
		TxData[(x + 1)] = NOP;
	}
	spiXfer(spiHandle, TxData, RxData, (payloadLength + 1));
	// Assign the data to the payload pointer. TxData[0] = status so skip it
	for (int y = 0; y < payloadLength; y++) {
		payload[y] = RxData[(y + 1)];
	}
	// terminat string
	payload[payloadLength] = 0;
	char status = readReg(STATUS);
	status |= RX_DR; // writing to the rx fifo to clear it
	writeReg(STATUS,status);
	payload[payloadLength] = 0;
	return payloadLength;
}

// Receive a fixed length payload
// char * payload must be DATA_SIZE or larger
uint8_t receive(char * payload) {
	uint8_t rxcheck = checkRx();
	if (rxcheck == 0) {
		return 0;
	}   
	char TxData[(DATA_SIZE + 1)];
	char RxData[(DATA_SIZE + 1)];
	TxData[0] = R_RX_PAYLOAD;
	// set up TxData
	for (int x = 0; x < DATA_SIZE; x++) {
		TxData[(x + 1)] = NOP;
	}
	spiXfer(spiHandle, TxData, RxData, (DATA_SIZE + 1 ));
	// Assign the data to the payload pointer. TxData[0] = status so skip it
	for (int y = 0; y < DATA_SIZE; y++) {
		payload[y] = RxData[(y + 1)];
	}
	return 1;
}

// Send something.  Make sure you turn off receive first
uint8_t transmit(char * payload, uint8_t datasize) {
	char TxData[(datasize + 1)];
	char RxData[(datasize + 1)];
	int status;
	char flush[] = {FLUSH_TX}; // flush tx command
	TxData[0] = W_TX_PAYLOAD;
	for (int x = 0; x < datasize; x++) {
		TxData[(x+1)] = payload[x];
	}
	spiXfer(spiHandle,flush,RxData,1);	
	// fill the tx buffer
	spiXfer(spiHandle, TxData, RxData, (datasize +1));

	powerUp();
	
	// self induced time out loop incase it doesn't respond
	for (int y = 0; y < 100; y ++) {
		// get tx status and see if it worked
		status = readReg(STATUS);
		if ((status & TX_DS) == TX_DS) { // it worked
			writeReg(STATUS, status); // clear the status
			powerDown();
			return 1;
		} else if ((status & MAX_RT) == MAX_RT) { // radio time out
			writeReg(STATUS, status);
			spiXfer(spiHandle,flush,RxData,1); // clear the buffer
			powerDown();
			return 0;
		}
		usleep(100); // wait some arbitrary time
	}
	// if we are here we timed out
	std::cout << "time out\n";
	powerDown();
	spiXfer(spiHandle,flush,RxData,1);
	return 0;
}

// flush the buffers
void flushme(void) {
	char flush[] = {FLUSH_TX};
	char RxData[1];
	spiXfer(spiHandle,flush,RxData, 1);
	flush[0] = FLUSH_RX;
	spiXfer(spiHandle,flush,RxData, 1);
}

void dumpMe(void) {
	int result;
	result = readReg(CONFIG);
	std::cout << "CONFIG:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(EN_AA);
	std::cout << "EN_AA:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(EN_RXADDR);
	std::cout << "EN_RXADDR:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(SETUP_AW);
	std::cout << "SETUP_AW:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(SETUP_RETR);
	std::cout << "SETUP_RETR:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RF_CH);
	std::cout << "RF_CH:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RF_SETUP);
	std::cout << "RF_SETUP:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(STATUS);
	std::cout << "STATUS:0x" << setfill('0') << setw(2) << hex << result << "\n";
	std::cout << "RX_ADDR_P0:0x" << setfill('0') << setw(2) << hex << readAddr(RX_ADDR_P0) << "\n";
	std::cout << "RX_ADDR_P1:0x" << setfill('0') << setw(2) << hex << readAddr(RX_ADDR_P1) << "\n";
	result = readReg(RX_ADDR_P2);
	std::cout << "RX_ADDR_P2:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RX_ADDR_P3);
	std::cout << "RX_ADDR_P3:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RX_ADDR_P4);
	std::cout << "RX_ADDR_P4:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RX_ADDR_P5);
	std::cout << "RX_ADDR_P5:0x" << setfill('0') << setw(2) << hex << result << "\n";
	std::cout << "TX_ADDR:0x" << setfill('0') << setw(2) << hex << readAddr(TX_ADDR) << "\n";
	result = readReg(RX_PW_P0);
	std::cout << "RX_PW_P0:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RX_PW_P1);
	std::cout << "RX_PW_P1:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RX_PW_P2);
	std::cout << "RX_PW_P2:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RX_PW_P3);
	std::cout << "RX_PW_P3:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RX_PW_P4);
	std::cout << "RX_PW_P4:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(RX_PW_P5);
	std::cout << "RX_PW_P5:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(FIFO_STATUS);
	std::cout << "FIFO_STATUS:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(DYNPD);
	std::cout << "DYNPD:0x" << setfill('0') << setw(2) << hex << result << "\n";
}

void dumpALittle(void) {
	int result;
	result = readReg(CONFIG);
	std::cout << "CONFIG:0x" << setfill('0') << setw(2) << hex << result << "\n";
	result = readReg(STATUS);
	std::cout << "STATUS:0x" << setfill('0') << setw(2) << hex << result << "\n";
}
