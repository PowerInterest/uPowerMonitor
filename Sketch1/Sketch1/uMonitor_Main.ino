// For CS5464 chip from Cirrus
// Read the peak current on channel I2
// Check for zero value coming back from CS5464, if so, then reset the device
// This code is for ardunio to command a CS5464 chip


#include <SPI.h>

const int slaveSelectPin = 15;

const int resetPin = 14;

#define fconvconst  .0000000596046483

union FourByte {
	struct {
		unsigned long value : 24; //24bit register values go in here
		byte command : 8; //8bit command goes in here.
	};
	byte bit8[4]; //this is just used for efficient conversion of the above into 4 bytes.
};

void setup() {
	// reset the CS5464 device
	pinMode(resetPin, OUTPUT);
	delay(100);
	digitalWrite(resetPin, LOW);
	delay(100);
	digitalWrite(resetPin, HIGH);
	delay(100);

	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE3); //I believe it to be Mode3
	SPI.setClockDivider(SPI_CLOCK_DIV16);
	pinMode(slaveSelectPin, OUTPUT); // Do this manually, as the SPI library doesn't work right for all versions.
	digitalWrite(slaveSelectPin, HIGH);

	Serial.begin(115200);
	delay(1000);
	Serial.println("setup...");

	//Perform software reset:
	SPI_writeCommand(0b10000000);
	SPI_writeCommand(0xC1);
	delay(2000);
	unsigned long status;
	do {
		status = SPI_read(0b00010111); //read the status register
		status &= (1UL << 23);
	} while (!status);

	SPI_writeCommand(0b10010000);

	setReg(0b00000000, 0b00000000, 0b00000010, 0b10101010);

	SPI_writeCommand(0b11010101); //Set continuous conversion mode
								  /*
								  // Print the configuration register, to confirm communication with the CS5464
								  check = SPI_read(0x00); //read the config register.
								  Serial1.print("Config = ");
								  Serial1.println(check, HEX);
								  */
								  /*
								  //example of writing a register
								  union FourByte data;
								  data.command = 0b01000000; //Write to config register
								  data.value = 1; //This is the default value from datasheet, just using it as an example.
								  SPI_writeRegister(data);
								  */
}

float fconv(unsigned long in) {
	return float(in) / (pow(2.0, 24) - 1);
}

float fconv2(long in) {
	return float(in << 8) / (pow(2.0, 24) - 1);
}

// Main loop
void loop() {
	delay(1000);
	unsigned long conf1 = SPI_read(0);
	unsigned long conf2 = SPI_read(1);
	SPI_writeCommand(0b10010000);
	float vmax = 120.0 / 0.6;
	float imax = 30.0 / 0.6;
	float I1r = fconv(SPI_read(6));
	float V1r = vmax*fconv(SPI_read(7));
	float I2r = imax*fconv(SPI_read(12));
	float V2r = vmax*fconv(SPI_read(13));
	unsigned long temp = SPI_read(27);
	delay(100);
	/*unsigned long tmp = SPI_read(0);
	//setReg(0, (byte)(tmp >> 16)|(1<<7), (byte)(tmp << 8), (byte)(tmp));
	String temp_ = String((int8_t)(highByte(temp))) + "." + String(temp & ((2 << 16) - 1));
	unsigned long voltage1 = SPI_read(0b00101100); //read Register 22 Peak Current Channel 2
	Serial.println(voltage1 >> 8);*/
	Serial.print(int(char(temp >> 16)));
	Serial.print(", ");
	Serial.println(V1r, 8);
	SPI_writeCommand(0b10000000);
	statusCheck();
}

// Send a Command to the CS5464 - see the data sheet for commands
void SPI_writeCommand(byte command) {
	digitalWrite(slaveSelectPin, LOW); //SS goes low to mark start of transmission
	union FourByte data = { 0x000000, command }; //generate the data to be sent, i.e. your command plus the Sync bytes.
												 //Serial.print("SPI_writeCommand:");
	for (char i = 3; i >= 0; i--) {
		SPI.transfer(data.bit8[i]); //transfer all 4 bytes of data - command first, then Big Endian transfer of the 24bit value.
									//Serial.print(data.bit8[i], HEX);
	}
	//Serial.println();
	digitalWrite(slaveSelectPin, HIGH);
}

// Read a register from the CS5464 - just supply a command byte (see the datasheet)
unsigned long SPI_read(byte command) {
	digitalWrite(slaveSelectPin, LOW); //SS goes low to mark start of transmission
	union FourByte data = { 0x000000, command }; //generate the data to be sent, i.e. your command plus the Sync bytes.
												 // Serial.print("SPI_Read:");
	for (char i = 3; i >= 0; i--) {
		data.bit8[i] = SPI.transfer(data.bit8[i]); //send the data whilst reading in the result
												   //Serial.print(data.bit8[i], HEX);
	}
	// Serial.println();
	digitalWrite(slaveSelectPin, HIGH); //SS goes high to mark end of transmission
	return data.value; //return the 24bit value recieved.
}

void setReg(byte reg, byte val1, byte val2, byte val3) {
	digitalWrite(slaveSelectPin, LOW);
	SPI.transfer(reg);
	SPI.transfer(val1);
	SPI.transfer(val2);
	SPI.transfer(val3);
	digitalWrite(slaveSelectPin, HIGH);
}

void statusCheck() {
	digitalWrite(slaveSelectPin, LOW);
	Read_Status_Reg(SPI_read(23));
	digitalWrite(slaveSelectPin, HIGH);
}

void Read_Status_Reg(byte reg23) {
	if (bitRead(reg23, 0)) Serial.print("SDI/RX time out Occured");
	if (bitRead(reg23, 2)) Serial.print("Data checksum Error");
	if (bitRead(reg23, 3)) Serial.print("Invalid Command Recieved");
	if (bitRead(reg23, 5)) Serial.print("updated");
	if (bitRead(reg23, 5)) Serial.print("Frequency updated");
	if (bitRead(reg23, 6)) Serial.print("V1 Sag event detected");
	if (bitRead(reg23, 7)) Serial.print("V2 Sag event detected");
	if (bitRead(reg23, 8)) Serial.print("I1 overcurrent");
	if (bitRead(reg23, 9)) Serial.print("I2 overcurrent");
	if (bitRead(reg23, 10)) Serial.print("V1 voltage register would overflow");
	if (bitRead(reg23, 11)) Serial.print("V2 voltage register would overflow");
	if (bitRead(reg23, 12)) Serial.print("I1 register would overflow");
	if (bitRead(reg23, 13)) Serial.print("I2 register would overflow");
	if (bitRead(reg23, 14)) Serial.print("P1 Power out of range, cause P2 register to overflow");
	if (bitRead(reg23, 15)) Serial.print("P2 Power out of range, cause P2 register to overflow");
	if (bitRead(reg23, 16)) Serial.print("V1  Swell Event Detected");
	if (bitRead(reg23, 17)) Serial.print("V2  Swell Event Detected");
	if (bitRead(reg23, 18)) Serial.print("MIPS overflow, Calculation engine did not complete before next read");
	if (bitRead(reg23, 21)) Serial.print("Watchdog timer overflow");
	if (bitRead(reg23, 22)) Serial.print("Conversion Ready");
	if (bitRead(reg23, 23)) Serial.print("Data is Ready");
}