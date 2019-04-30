/*
    Name:       Parking_System.ino
    Created:	28/04/2019 8:28:33 AM
    Author:     DESKTOP-CDTEKK\cdtekk
*/

#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <MFRC522.h>

const int RST_PIN = 5;
MFRC522 mfrc522(SS, RST_PIN);  // Create MFRC522 instance (53, 5) of arduino

Servo servoEntrance;
Servo servoExit;

LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// PINS
const uint8_t ANALOG_PINS[] = { A0, A1, A2, A3, A4 };
const int IR_PIN = 3;
const int LED_PIN_1_RED = 4;
const int LED_PIN_1_GREEN = 5;
const int LED_PIN_2_RED = 6;
const int LED_PIN_2_GREEN = 7;
const int LED_PIN_3_RED = 8;
const int LED_PIN_3_GREEN = 9;
const int LED_PIN_4_RED = 10;
const int LED_PIN_4_GREEN = 11;
const int LED_PIN_5_RED = 12;
const int LED_PIN_5_GREEN = 13;

//	
unsigned long previousMillis = 0;
int availableSlots = 5;
int balanceLeft;
String strContent;

void setup() {
	// Init Serial
	Serial.begin(9600);
	// Init EEPROM
	EEPROM.begin();
	// Init MFRC522
	mfrc522.PCD_Init();
	// Show details of PCD - MFRC522 Card Reader details
	mfrc522.PCD_DumpVersionToSerial();
	// Init pins to control
	for (size_t i = 3; i < 14; i++) {
		pinMode(i, OUTPUT);
	}
	// Init pins to listen
	for (size_t i = 0; i < 5; i++) {
		pinMode(ANALOG_PINS[i], INPUT);
	}
	// Init and designate servo pins
	servoEntrance.attach(5);
	servoExit.attach(6);

	//ClearEEPROM();
	//WriteString(0, "RFID_CONTENTS");

	availableSlots -= OccupiedSlots();
}

void loop() {
	// Wait for PICC tap
	if (!mfrc522.PICC_IsNewCardPresent()) {
		return;
	}

	if (!mfrc522.PICC_ReadCardSerial()) {
		return;
	}

	// Now that if card has been tapped get RFID information
	char *id = dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);

	// Check card
	if (!Exist(id)) {
		Serial.println("ID does not exist");
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("PICC does not");
		lcd.setCursor(0, 1);
		lcd.print("exist");

		delay(1500);

		lcd.clear();

		return;
	}

	// Evaluate ID contents
	CaptureHEX(id);

	// Check balance
	if (balanceLeft <= 35) {
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Bal not enough.");
		lcd.setCursor(0, 1);
		lcd.print("Process failed.");

		delay(1500);

		lcd.clear();

		return;
	}

	// Lastly, Check for available parking slot
	if (availableSlots == 0) {
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Slots full.");

		delay(1500);

		lcd.clear();

		return;
	}
	else {
		/*
		 * Cards will only be updated when all processes are successful
		 * else ALL changes made within CaptureHEX() are discarded
		 */

		availableSlots -= 1;

		// Clear first
		ClearEEPROM();

		Serial.println("Current content");
		Serial.println("--------------------------------------------------------");
		Serial.println(EEPROMGetContents());
		Serial.println("Updating contents...");
		// Update data/card with new content
		WriteString(0, strContent);
		Serial.println("--------------------------------------------------------");
		Serial.println(EEPROMGetContents());

		//servoEntrance.write(90);

		//delay(1500);

		//servoEntrance.write(0);
	}

	// Someone has left the parking area
	if (digitalRead(IR_PIN) == HIGH) {
		availableSlots++;

		//servoEntrance.write(90);

		//delay(1500);

		//servoEntrance.write(0);
	}

	// Provides non-block check
	if ((millis() - previousMillis) > 100) {
		previousMillis = millis();

		/* 
		 * Photoresistor value
		 * 0	- no light
		 * 1024 - bright light
		 */
		SwapState(
			LED_PIN_1_RED,							// Red led of the parking
			LED_PIN_1_GREEN,						// Green led of the parking
			CheckParkSpace(ANALOG_PINS[0]) > 0);	// Tell which leds should change state

		SwapState(
			LED_PIN_2_RED,
			LED_PIN_2_GREEN,
			CheckParkSpace(ANALOG_PINS[1]) > 0);

		SwapState(
			LED_PIN_3_RED,
			LED_PIN_3_GREEN,
			CheckParkSpace(ANALOG_PINS[2]) > 0);

		SwapState(
			LED_PIN_4_RED,
			LED_PIN_4_GREEN,
			CheckParkSpace(ANALOG_PINS[3]) > 0);

		SwapState(
			LED_PIN_5_RED,
			LED_PIN_5_GREEN,
			CheckParkSpace(ANALOG_PINS[4]) > 0);
	}
}

// Helper routine to dump a byte array as hex values to Serial.
char *dump_byte_array(byte *buffer, byte bufferSize) {
	char data[11];
	sprintf(data, "%02X:%02X:%02X:%02X", buffer[0], buffer[1], buffer[2], buffer[3]);

	Serial.print(F("Scanned -> "));
	Serial.println(data);

	// Returns duplicate
	return strdup(data);
}

// Continously monitor slots
uint8_t OccupiedSlots() {
	// Always reset to zero to avoid overflow counting of slot
	// availability
	int occupiedSlots = 0;
	for (size_t i = 0; i < 5; i++) {
		uint16_t value = analogRead(ANALOG_PINS[i]);
		// This will check how many slots are available
		if (value < 1) {
			occupiedSlots++;
		}
	}

	return occupiedSlots;
}

// Resolves scanned RFID content
void CaptureHEX(char *target) {
	/*
		How the content are handled...

		EEPROM CONTENT:
			79:02:B5:55,0,500|69:15:B8:55,0,500| ... |69:15:B8:55,500
				  |
				  |_ _ _ 79:02:B5:55,500
								|
								| _ _ _ [79:02:B5:55]	[1] 	[500]
											 |			 |		  |
										  Card ID	   Parked	Balance

		They're splitted with comma then evaluates whether its an ID or a Balance
	 */
	char str[1024];

	// Cache the EEPROM content to str
	strcpy(str, EEPROMGetContents().c_str());

	strContent = String(str);
	Serial.println(strContent);

	// Split EEPROM content with '|' character. See serial monitor for details
	char *bufferPtr = strtok(str, "|");

	char parkingState;
	char balance[5];
	String newContent;
	while (bufferPtr != nullptr) {
		bufferPtr = strtok(nullptr, "|");

		// If the card exists within the list of EEPROM values
		if (strstr(bufferPtr, target) == nullptr) {
			continue;
		}
		
		// Get the parking state
		parkingState = *(bufferPtr + 12);

		// Set the parking state
		parkingState = parkingState == '0' ? '1' : '0';

		// The character we'll look for when to split
		char *token = strrchr(bufferPtr, ',');

		// Resolve/Fetch balance value for scanned card
		balance[5];
		int index = 0;
		for (int i = token - bufferPtr + 1; i < strlen(bufferPtr); i++) {
			balance[index++] = bufferPtr[i];
		}

		balance[index] = '\0';

		// Convert char to integer
		balanceLeft = atoi(balance);

		// Fix price?
		balanceLeft -= 35;

		// All information are now cached and ready to be used or discarded
		newContent = String(target) + "," +  + parkingState + "," + balanceLeft + "|";

		// Stop scanning
		break;
	}

	// Updated strContent value
	strContent.replace(String(target), newContent);
}

// The Red led, green led and slot availablity. Resolves lighting states of parking led
void SwapState(int redLed, int greenLed, boolean slotEmpty) {
	if (slotEmpty) {
		digitalWrite(redLed, LOW);
		digitalWrite(greenLed, HIGH);
	}
	else {
		digitalWrite(redLed, HIGH);
		digitalWrite(greenLed, LOW);
	}
}

uint16_t CheckParkSpace(uint16_t sensor) {
	return sensor;
}

// Wipe RFID contents
void ClearEEPROM() {
	// Write 0 values to all memory addresses of EEPROM. Basically erase the data
	for (size_t i = 0; i < EEPROM.length(); i++) {
		EEPROM.write(i, 0);
	}
}

// Upon updating EEPROM/Changing RFID contents
void WriteString(char add, String data) {
	// Size of the data to be writtent
	int _size = data.length();
	int i;
	// Write every element of the data
	for (i = 0; i < _size; i++) {
		EEPROM.write(add + i, data[i]);
	}

	// Add termination null character for String Data 
	// as per requirement when handling characters
	EEPROM.write(add + _size, '\0');
}

// Fetching RFID contents
String EEPROMGetContents() {
	char data[1021];
	unsigned char k;
	int memPtr = 0;
	k = EEPROM.read(0);
	//Read until null character
	while (k != '\0' && memPtr < 1021) {
		k = EEPROM.read(0 + memPtr);
		data[memPtr] = k;
		memPtr++;
	}
	data[memPtr] = '\0';
	return String(data);
}

// Does the RFID exists with the storage
boolean Exist(char *id) {
	char content[1024];

	strcpy(content, EEPROMGetContents().c_str());

	if (strstr(content, id) != nullptr) {
		content[0] = '\0';
		return true;
	}
	else {
		content[0] = '\0';
		return false;
	}
}