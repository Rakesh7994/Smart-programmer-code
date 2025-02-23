#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char *ssid = "SmartProgrammer";
const char *password = "12345678";

// Web server on port 80
ESP8266WebServer server(80);

// EEPROM I2C address (0x50 for 24C04)
const uint8_t EEPROM_I2C_ADDRESS = 0x50;
uint16_t currentAddress = 0x0000;
bool eepromReadComplete = false;
unsigned long lastReadTimestamp = 0;


// Variables to store detail data
String mainMode;
String eepromMode;
String eepromSize;
bool isFrequencyGenerating = false;   // Indicates if frequency generation is ongoing
bool isEEPROMActive = false;          // Indicates if an EEPROM read/write operation is ongoing


// Helper functions to handle EEPROM sizes and addressing modes

size_t getEEPROMPageSize() {
    size_t pageSize;
    if (eepromSize == "24C01" || eepromSize == "24C02") pageSize = 8;
    else if (eepromSize == "24C04" || eepromSize == "24C08" || eepromSize == "24C16") pageSize = 16;
    else if (eepromSize == "24C32" || eepromSize == "24C64") pageSize = 32;
    else if (eepromSize == "24C128" || eepromSize == "24C256") pageSize = 64;
    else pageSize = 16;  // Default page size

    Serial.printf("Determined page size: %d bytes for EEPROM size: %s\n", pageSize, eepromSize.c_str());
    return pageSize;
}

// Get the maximum address based on EEPROM size
size_t getMaxAddress() {
    if (eepromSize == "24C01") return 128;       // 128 bytes
    else if (eepromSize == "24C02") return 256;   // 256 bytes
    else if (eepromSize == "24C04") return 512;   // 512 bytes
    else if (eepromSize == "24C08") return 1024;  // 1 KB
    else if (eepromSize == "24C16") return 2048;  // 2 KB
    else if (eepromSize == "24C32") return 4096;  // 4 KB
    else if (eepromSize == "24C64") return 8192;  // 8 KB
    else if (eepromSize == "24C128") return 16384; // 16 KB
    else if (eepromSize == "24C256") return 32768; // 32 KB
    else if (eepromSize == "24C512") return 65536; // 64 KB
    return 512;  // Default to 512 bytes if not recognized
}


bool is16BitAddress() {
    bool uses16Bit = (eepromSize == "24C32" || eepromSize == "24C64" || eepromSize == "24C128" || eepromSize == "24C256");
    Serial.printf("Addressing mode: %s-bit for EEPROM size: %s\n", uses16Bit ? "16" : "8", eepromSize.c_str());
    return uses16Bit;
}

uint8_t getDeviceAddress(uint16_t address) {
    uint8_t deviceAddress;

    if (eepromSize == "24C16") {
        // Correct 3-bit bank selection for 24C16
        deviceAddress = EEPROM_I2C_ADDRESS | ((address >> 8) & 0x07);
        Serial.printf("Using 3-bit bank selection for 24C16: Device address: 0x%02X\n", deviceAddress);
    } 
    else if (eepromSize == "24C08") {
        // Correct for 24C08
        deviceAddress = EEPROM_I2C_ADDRESS | ((address >> 8) & 0x07);
        Serial.printf("Using 3-bit bank selection for 24C08: Device address: 0x%02X\n", deviceAddress);
    } 
    else if (is16BitAddress()) {
        // For larger EEPROMs with 16-bit addressing
        deviceAddress = EEPROM_I2C_ADDRESS;
        Serial.printf("Using single device address for 16-bit EEPROM: 0x%02X\n", deviceAddress);
    } 
    else {
        // For smaller EEPROMs with A8 bit selection
        deviceAddress = EEPROM_I2C_ADDRESS | ((address >> 8) & 0x01);
        Serial.printf("Using bank selection (A8) for smaller EEPROMs: Device address: 0x%02X\n", deviceAddress);
    }

    return deviceAddress;
}


// Function to handle detail data requests
void handleDetailDataRequest() {
    Serial.println("Received POST request at /detail_data");

    if (!server.hasArg("plain")) {
        Serial.println("No data received.");
        server.send(400, "text/plain", "Bad Request: No data received.");
        return;
    }

    // Parse the JSON payload
    String payload = server.arg("plain");
    Serial.printf("Payload received: %s\n", payload.c_str());

    StaticJsonDocument<256> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, payload);
    if (error) {
        Serial.println("Failed to parse JSON payload.");
        server.send(400, "text/plain", "Bad Request: Invalid JSON.");
        return;
    }

    // Extract details from the payload
    mainMode = jsonDoc["main_mode"] | "";
    eepromMode = jsonDoc["eeprom_mode"] | "";
    eepromSize = jsonDoc["eeprom_size"] | "";

    // Log the received details
    Serial.println("Received detail data:");
    Serial.printf("Main Mode: %s\n", mainMode.c_str());
    Serial.printf("EEPROM Mode: %s\n", eepromMode.c_str());
    Serial.printf("EEPROM Size: %s\n", eepromSize.c_str());

    server.send(200, "text/plain", "Detail data received successfully.");
}

// Function to write a single page of data to EEPROM
bool writeEEPROMPage(uint16_t address, const uint8_t *data, size_t length) {
    size_t pageSize = getEEPROMPageSize();

    // Ensure data fits within the page boundary
    if ((address % pageSize) + length > pageSize) {
        Serial.printf("Error: Data crosses page boundary. Address: 0x%04X\n", address);
        return false;
    }

    uint8_t deviceAddress = getDeviceAddress(address);
    Serial.printf("Starting write operation to address 0x%04X (Device address: 0x%02X)\n", address, deviceAddress);

    // Begin I2C transmission
    Wire.beginTransmission(deviceAddress);

    // Write address (8-bit or 16-bit)
    if (is16BitAddress()) {
        Wire.write((address >> 8) & 0xFF);  // Upper 8 bits of address
        Serial.printf("Sending upper address byte: 0x%02X\n", (address >> 8) & 0xFF);
    }
    Wire.write(address & 0xFF);  // Lower 8 bits of address
    Serial.printf("Sending lower address byte: 0x%02X\n", address & 0xFF);

    // Write data bytes
    Serial.println("Writing data bytes:");
    for (size_t i = 0; i < length; i++) {
        Wire.write(data[i]);
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();

    if (Wire.endTransmission() == 0) {
        delay(10);  // Allow EEPROM time to complete write cycle
        Serial.println("Write operation successful.");
        return true;
    } else {
        Serial.println("EEPROM write failed.");
        return false;
    }
}

// Function to read a page of data from EEPROM
bool readEEPROMPage(uint16_t address, uint8_t *data, size_t length) {
    uint8_t deviceAddress = getDeviceAddress(address);
    Serial.printf("Starting read operation from address 0x%04X (Device address: 0x%02X)\n", address, deviceAddress);

    // Begin I2C transmission
    Wire.beginTransmission(deviceAddress);

    // Write address (8-bit or 16-bit)
    if (is16BitAddress()) {
        Wire.write((address >> 8) & 0xFF);  // Upper 8 bits of address
        Serial.printf("Sending upper address byte: 0x%02X\n", (address >> 8) & 0xFF);
    }
    Wire.write(address & 0xFF);  // Lower 8 bits of address
    Serial.printf("Sending lower address byte: 0x%02X\n", address & 0xFF);

    if (Wire.endTransmission() != 0) {
        Serial.println("EEPROM read failed.");
        return false;
    }

    // Request data from EEPROM
    Wire.requestFrom(deviceAddress, length);
    Serial.println("Reading data bytes:");
    for (size_t i = 0; i < length; i++) {
        if (Wire.available()) {
            data[i] = Wire.read();
            Serial.printf("%02X ", data[i]);
        } else {
            Serial.println("\nFailed to read data from EEPROM.");
            return false;
        }
    }
    Serial.println("\nRead operation successful.");
    return true;
}



void handleReadDataRequest() {
    Serial.println("Received GET request at /read_data");

    if (!(mainMode == "EEPROM Programmer" && eepromMode == "Read")) {
        Serial.println("Read operations are disabled in the current mode.");
        server.send(403, "application/json", "{\"error\": \"Read operations are disabled in the current mode.\"}");
        return;
    }

    stopFrequencyGeneration();

    size_t pageSize = getEEPROMPageSize();
    size_t maxAddress = getMaxAddress();

    // If read is already complete, only send the final confirmation once
    if (eepromReadComplete) {
        Serial.println("EEPROM read has already completed. Sending final response.");
        server.send(200, "application/json", "{\"message\": \"EEPROM read completed.\"}");
        eepromReadComplete = false; // Mark completion
        
        return;
    }

    // Check if we have reached the last page
    if (currentAddress >= maxAddress) {
        Serial.println("EEPROM read complete. Sending final response.");
        eepromReadComplete = true; // Mark completion
        server.send(200, "application/json", "{\"message\": \"EEPROM read completed.\"}");

        // **Reset address & state for next read session**
        currentAddress = 0x0000;
        Serial.println("EEPROM address reset to 0x0000 for the next read operation.");
        return;
    }

    Serial.printf("Starting EEPROM read from address: 0x%04X\n", currentAddress);

    uint8_t data[pageSize];
    if (readEEPROMPage(currentAddress, data, pageSize)) {
        StaticJsonDocument<256> jsonDoc;
        jsonDoc["address"] = currentAddress;

        JsonArray dataArray = jsonDoc.createNestedArray("data");
        for (size_t i = 0; i < pageSize; i++) {
            dataArray.add(data[i]);
        }

        String response;
        serializeJson(jsonDoc, response);

        Serial.printf("Sending data for address 0x%04X: %s\n", currentAddress, response.c_str());
        server.send(200, "application/json", response);

        // Store last read timestamp for reference
        lastReadTimestamp = millis();

    } else {
        Serial.printf("Failed to read data from address 0x%04X.\n", currentAddress);
        server.send(500, "application/json", "{\"error\": \"Failed to read EEPROM page.\"}");
    }
}

void handleAck() {
    Serial.println("ACK received from Android.");

    size_t pageSize = getEEPROMPageSize();
    currentAddress += pageSize;

    if (currentAddress >= getMaxAddress()) {
        Serial.println("EEPROM read complete. Sending final confirmation.");
        server.send(200, "text/plain", "EEPROM read completed.");

        // **Reset address & state for next session**
        eepromReadComplete = true;
        currentAddress = 0x0000;
        Serial.println("EEPROM address reset to 0x0000 for the next read operation.");

    } else {
        server.send(200, "text/plain", "ACK received - Moving to next page");
    }
}






void handleTestRequest() {
    Serial.println("Received GET request at /test");
    server.send(200, "text/plain", "ESP8266 is alive!");
}

void handleWriteDataRequest() {
    Serial.println("Received POST request at /program_data");

    // Check if write operations are allowed
    if (!(mainMode == "EEPROM Programmer" && eepromMode == "Write")) {
        Serial.println("Write operations are disabled in the current mode.");
        server.send(403, "text/plain", "Forbidden: Write operations are disabled in the current mode.");
        return;
    }

    // If a frequency generation task is active, stop it
    stopFrequencyGeneration();

    // Mark EEPROM task as active
    isEEPROMActive = true;

    // Check if the request contains a body
    if (!server.hasArg("plain")) {
        Serial.println("No data received.");
        server.send(400, "text/plain", "Bad Request: No data received.");
        return;
    }

    // Parse the JSON payload
    String payload = server.arg("plain");
    StaticJsonDocument<256> jsonDoc;

    DeserializationError error = deserializeJson(jsonDoc, payload);
    if (error) {
        Serial.println("Failed to parse JSON payload.");
        server.send(400, "text/plain", "Bad Request: Invalid JSON.");
        return;
    }

    // Extract the address and data from the payload
    String addressStr = jsonDoc["address"];
    String dataStr = jsonDoc["data"];

    // Convert address from string to integer
    uint16_t address = strtoul(addressStr.c_str(), nullptr, 16);

    // Convert data string to a byte array
    std::vector<uint8_t> dataBytes;
    size_t pos = 0;
    while (pos < dataStr.length()) {
        String byteStr = dataStr.substring(pos, pos + 2);
        uint8_t byteValue = strtoul(byteStr.c_str(), nullptr, 16);
        dataBytes.push_back(byteValue);
        pos += 3;  // Move to the next byte (skip the space delimiter)
    }

    // Write the data to EEPROM at the specified address
    if (writeEEPROMPage(address, dataBytes.data(), dataBytes.size())) {
        Serial.printf("Data written successfully to address 0x%04X.\n", address);
        server.send(200, "text/plain", "Data written successfully.");
    } else {
        Serial.printf("Failed to write data to address 0x%04X.\n", address);
        server.send(500, "text/plain", "Internal Server Error: Write failed.");
    }

    // Mark EEPROM task as inactive
    isEEPROMActive = false;
    
}


void handleFrequencyGenerationRequest() {
    Serial.println("Received POST request at /generate_frequency");

    if (!(mainMode == "Frequency Generator")) {
        Serial.println("Frequency generation operations are disabled in the current mode.");
        server.send(403, "text/plain", "Forbidden: Frequency generation operations are disabled in the current mode.");
        return;
    }

    if (isEEPROMActive) {
        server.send(409, "text/plain", "Conflict: An EEPROM operation is currently active.");
        return;
    }

    if (!server.hasArg("plain")) {
        Serial.println("No data received.");
        server.send(400, "text/plain", "Bad Request: No data received.");
        return;
    }

    // Parse the JSON payload to get frequency and duty cycle
    String payload = server.arg("plain");
    StaticJsonDocument<256> jsonDoc;

    DeserializationError error = deserializeJson(jsonDoc, payload);
    if (error) {
        Serial.println("Failed to parse JSON payload.");
        server.send(400, "text/plain", "Bad Request: Invalid JSON.");
        return;
    }

    int frequency = jsonDoc["frequency"] | 1000;  // Default to 1000 Hz
    int dutyCycle = jsonDoc["duty_cycle"] | 50;   // Default to 50%

    int invertedDutyCycle = 25 - dutyCycle;


    // **Check if frequency is 0 and return an error**
    if (frequency == 0) {
        Serial.println("Error: Received frequency is 0. Cannot generate PWM.");
        server.send(400, "text/plain", "Bad Request: Frequency cannot be 0.");
        return;
    }

    stopFrequencyGeneration();
    
    // Log received data
    Serial.printf("Generating frequency: %d Hz, Duty Cycle: %d%%\n", frequency, dutyCycle);

    // **Apply new frequency and duty cycle settings**
    analogWriteFreq(frequency);
    analogWrite(2, (invertedDutyCycle * 1023) / 100);

    isFrequencyGenerating = true;  // Mark frequency generation as active

    // **Send success response only after successful configuration**
    server.send(200, "text/plain", "Frequency generation started successfully.");
}


void setup() {
    Serial.begin(115200);

    // Start I2C (SDA = D1, SCL = D2)
    Wire.begin(5, 4);  // SDA = GPIO5 (D1), SCL = GPIO4 (D2)

     // Configure A0, A1, A2, WP as outputs
    pinMode(12, OUTPUT); // A0
    pinMode(13, OUTPUT); // A1
    pinMode(14, OUTPUT); // A2
    pinMode(15, OUTPUT);  // WP

    // Set default state (all LOW for lowest address 0x50, WP disabled)
    digitalWrite(12, LOW);  // A0 = 0
    digitalWrite(13, LOW);  // A1 = 0
    digitalWrite(14, LOW);  // A2 = 0
    digitalWrite(15, LOW);   // WP = 0 (Write enabled)

    Serial.println("EEPROM address and WP pins initialized.");


    // Start Wi-Fi access point
    WiFi.softAP(ssid, password);
    Serial.println("Wi-Fi access point started");

    // Set up server endpoints
    server.on("/detail_data", HTTP_POST, handleDetailDataRequest);
    server.on("/program_data", HTTP_POST, handleWriteDataRequest);
    server.on("/read_data", HTTP_GET, handleReadDataRequest);
    server.on("/generate_frequency", HTTP_POST, handleFrequencyGenerationRequest);
    server.on("/test", HTTP_GET, handleTestRequest);
    server.on("/ack", HTTP_GET, handleAck);

    
    server.begin();
    Serial.println("Web server started");
}

void loop() {
    server.handleClient();
}


void stopFrequencyGeneration() {
    if (isFrequencyGenerating) {
        analogWrite(2, 0);  // Stop PWM output
        isFrequencyGenerating = false;
        Serial.println("Frequency generation stopped.");
    }
}
