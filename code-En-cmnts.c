// Include the Watchdog Timer (WDT) library for system recovery if the program hangs
#include <avr/wdt.h>

// Define the sensor pin as A0 for reading the gas sensor data (analog input)
#define sensor_pin A0

// Include the EEPROM library to read and write to the Arduino's non-volatile memory (EEPROM)
#include <EEPROM.h>


void setup() {
  int i = 0; // Initialize a counter variable

  // Start serial communication with a baud rate of 115200
  Serial.begin(115200);

  // Call a function to check and establish GSM network connectivity
  check_connect();

  // Configure GSM module to operate in SMS text mode
  Serial.print("AT+CMGF=1\r\n");
  delay(1000); // Wait for 1 second to ensure the command takes effect

  // Delete all SMS messages stored in the GSM module
  Serial.print("AT+CMGD=1,4\r\n");
  delay(1000);

  // Set SMS parameters (e.g., PDU mode, validity period)
  Serial.print("AT+CSMP=17,167,0,0\r\n");
  delay(1000);

  // Enable caller line identification (display incoming call numbers)
  Serial.print("AT+CLIP=1\r\n");
  delay(1000);

  // Clear any residual data from the serial buffer
  Serial.readString();

  // Wait until at least one phone number is saved in EEPROM
  // Numbers are stored at addresses 0 and 20 in EEPROM
  while (char(EEPROM.read(0)) == NULL && char(EEPROM.read(20)) == NULL) {
    check_sms(); // Continuously check for SMS to receive configuration
  }

  // Send an SMS to notify the user that the system is ready to receive settings
  send_sms("READY TO RECEIVE SETTING!");

  // Enter a loop to check for additional settings
  while (true) {
    delay(1000); // Wait for 1 second
    check_sms(); // Continuously check for SMS messages

    i++; // Increment the counter

    // Exit the loop if either a phone number is saved in EEPROM
    // or 120 seconds (2 minutes) have passed
    if ((i >= 120) && (char(EEPROM.read(0)) != NULL || char(EEPROM.read(20)) != NULL))
      break;
  }

  // Notify the user that the gas leak detector system has started
  send_sms("GAS LEAK DETECTOR BOOTED!");

  // Wait for 5 minutes before beginning monitoring
  // This ensures the system stabilizes and gives the user time for initial setup
  for (i = 0; i < 300; i++)
    delay(1000); // Delay for 1 second (300 seconds = 5 minutes)

  // Print a message to the serial monitor indicating that monitoring has started
  Serial.println("MONITORING");

  // Flush the serial buffer to ensure no residual data remains
  Serial.flush();
}


void loop() {
  // Read the analog value from the gas sensor
  // Compare it to the threshold value of 350
  if (analogRead(sensor_pin) > 350) {
    // If the gas concentration exceeds the threshold,
    // call the user(s) stored in the EEPROM
    call_user();

    // Check for any incoming calls within a specific time frame
    // This allows the user to interact with the system if needed
    check_incoming_call();
  }
}


void check_connect() {
  String status_ = ""; // String to store the network status response

  // Inform the user that the system is waiting to connect to the GSM network
  Serial.println("WAITING TO CONNECT TO NETWORK");

  // Clear any residual data in the serial buffer to ensure clean communication
  Serial.flush();

  // Continuously check for network connectivity
  while (true) {
    // If there's any data in the serial buffer, read and discard it
    if (Serial.available() > 0)
      Serial.readString();

    // Send an AT command to check the network registration status
    Serial.print("AT+CCALR?\r\n");

    // Wait for the GSM module to respond
    delay(1000);

    // Read the response from the GSM module
    status_ = Serial.readString();

    // Check if the response indicates that the module is registered on the network
    // "+CCALR: 1" signifies successful network registration
    if (status_.indexOf("+CCALR: 1") >= 0)
      break; // Exit the loop when connected
  }

  // Print a message indicating successful network connection
  Serial.println("CONNECTED TO NETWORK");

  // Clear the serial buffer again to ensure no residual data remains
  Serial.flush();
}


void check_sms() {
  String get_sms = ""; // Variable to store the received SMS content

  // Check if there is data available in the serial buffer
  if (Serial.available() > 0)
    get_sms = Serial.readString(); // Read the incoming serial data into the string

  // Check if the received data contains an SMS notification (+CMTI)
  if (get_sms.indexOf("+CMTI") >= 0) {
    // Clear any additional data in the serial buffer
    if (Serial.available() > 0)
      Serial.readString();

    // Send an AT command to read the first SMS in the inbox
    Serial.print("AT+CMGR=1\r\n");

    // Wait until a response is received from the GSM module
    while (Serial.available() == 0) {}

    // Read the SMS content from the serial buffer
    get_sms = Serial.readString();

    // Extract the actual message content from the SMS using delimiters '!' and '#'
    get_sms = get_sms.substring(get_sms.indexOf("!") + 1, get_sms.indexOf("#"));

    // Send an AT command to delete all SMS messages in the inbox to free memory
    Serial.print("AT+CMGD=1,4\r\n");
    delay(1000);

    // Check if the SMS content is a delete command ("D1" or "D2")
    if (get_sms == "D1" || get_sms == "D2")
      delete_number(get_sms); // Delete the corresponding number from EEPROM
    else if (get_sms != "")
      save_number(get_sms); // Save the new number to EEPROM if the content is valid
  }
}

void save_number(String number) {
  int i = 0; // Index variable for EEPROM address
  int j = 0; // Index variable for character array
  char buff[20]; // Buffer to hold the phone number as a character array

  // Convert the phone number from a String to a character array
  number.toCharArray(buff, 20);

  // Check if the first phone number slot in EEPROM is empty
  if (char(EEPROM.read(0)) == NULL) {
    // Save the length of the number at the first address (0)
    EEPROM.write(0, number.length());

    // Write each character of the number to subsequent EEPROM addresses
    for (i = 0; i < number.length(); i++) {
      EEPROM.write(i + 1, char(buff[i])); // Save the phone number starting at address 1
    }
  } else {
    // If the first slot is already occupied, save the number in the second slot
    // Save the length of the number at address 20
    EEPROM.write(20, number.length());

    // Write each character of the number to subsequent addresses starting at 21
    for (i = 20; i < number.length() + 20; i++) {
      EEPROM.write(i + 1, char(buff[j])); // Save the phone number
      j++; // Increment the index for the character array
    }
  }
}

void send_sms(String text) {
  String number = ""; // Variable to store the recipient phone number
  String cmgs = "AT+CMGS=\""; // Command to send an SMS, starts with the base format

  // Check if a phone number is saved in the first EEPROM slot
  if (char(EEPROM.read(0)) != NULL) {
    number = read_number(0); // Read the phone number from the first slot (starting at address 0)
    cmgs = cmgs + number + "\"\r\n"; // Complete the SMS command with the phone number
    Serial.print(cmgs); // Send the command to the GSM module
    delay(500); // Wait for the module to process the command
    Serial.println(text); // Send the SMS content
    delay(500); // Allow time for the content to be transmitted
    Serial.write(0x1a); // Send the CTRL+Z (0x1A) character to indicate end of SMS
    delay(15000); // Wait for the SMS to be sent successfully
  }
  // If the first EEPROM slot is empty, check the second slot
  else if (char(EEPROM.read(20)) != NULL) {
    cmgs = "AT+CMGS=\""; // Reset the base SMS command
    number = read_number(20); // Read the phone number from the second slot (starting at address 20)
    cmgs = cmgs + number + "\"\r\n"; // Complete the SMS command with the phone number
    Serial.print(cmgs); // Send the command to the GSM module
    delay(500); // Wait for the module to process the command
    Serial.println(text); // Send the SMS content
    Serial.write(0x1a); // Send the CTRL+Z (0x1A) character to indicate end of SMS
  }
}


String read_number(int addr) {
  String number_to_sms = ""; // String to store the phone number read from EEPROM
  int len = int(EEPROM.read(addr)); // Read the length of the phone number stored at the given address

  // Loop through the EEPROM to retrieve the phone number characters
  for (int i = addr + 1; i <= len + addr; i++) {
    number_to_sms.concat(char(EEPROM.read(i))); // Read each character and append it to the string
  }

  return number_to_sms; // Return the reconstructed phone number as a String
}


void delete_number(String del) {
  int len = 0; // Variable to store the length of the phone number being deleted

  // Check if the user wants to delete the first stored number ("D1")
  if (del == "D1") {
    len = EEPROM.read(0); // Read the length of the phone number stored at address 0
    for (int i = 0; i <= len; i++) {
      EEPROM.write(i, NULL); // Overwrite the phone number and its length with NULL
    }
  }
  // Check if the user wants to delete the second stored number ("D2")
  else if (del == "D2") {
    len = EEPROM.read(20); // Read the length of the phone number stored at address 20
    for (int i = 20; i <= len + 20; i++) {
      EEPROM.write(i, NULL); // Overwrite the phone number and its length with NULL
    }
  }
}


void call_user() {
  String number = "ATD"; // Base AT command for initiating a call

  // Check if a phone number is stored in the first EEPROM slot
  if (EEPROM.read(0) != NULL) {
    number = number + read_number(0) + ";\r\n"; // Append the stored phone number and terminate the command
    Serial.print(number); // Send the AT command to the GSM module to initiate the call
    delay(20000); // Wait for 20 seconds to allow the call to ring
  }

  // Check if a phone number is stored in the second EEPROM slot
  if (EEPROM.read(20) != NULL) {
    number = "ATD" + read_number(20) + ";\r\n"; // Append the second stored phone number and terminate the command
    Serial.print(number); // Send the AT command to the GSM module to initiate the call
    delay(20000); // Wait for 20 seconds to allow the call to ring
  }
}


void check_incoming_call() {
  String incoming_number = ""; // Variable to store the incoming data from the GSM module
  unsigned long tm = millis(); // Record the current time to manage the timeout duration

  // Loop for a maximum duration of 60 seconds to check for incoming calls
  while (millis() - tm < 60000) {
    // Check if there is any data available from the GSM module
    if (Serial.available() > 0) {
      incoming_number = Serial.readString(); // Read the incoming data
    }

    // Check if the incoming data indicates an incoming call ("RING")
    if (incoming_number.indexOf("RING") >= 0) {
      check_number(incoming_number); // Call the `check_number` function to validate the caller
    }
  }
}

void check_number(String data_to_parse) {
  // Extract the phone number from the incoming call data
  data_to_parse = data_to_parse.substring(data_to_parse.indexOf(":") + 3, data_to_parse.indexOf(",") - 1);

  // Check if the extracted phone number matches either of the stored numbers in EEPROM
  if (data_to_parse == read_number(0) || data_to_parse == read_number(20)) {
    Serial.print("ATH\r\n"); // Send the "ATH" command to hang up the call
    delay(500); // Wait for a short duration to ensure the command is processed
    wdt_enable(WDTO_4S); // Enable the Watchdog Timer with a timeout of 4 seconds for a system reset
  }
}
