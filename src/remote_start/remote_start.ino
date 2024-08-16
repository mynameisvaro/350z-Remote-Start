#include "defines.h"
#include "Encryption.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <mcp2515.h>
#include <esp_now.h>
#include <WiFi.h>
#include <cmath>

// REMOTE START FUNCTIONS

void ksEncrypter(void *) // Encrypts page 3 with stored nR, key and uid
{

    for (;;)
    {
        // Wait to be called to encrypt ks
        if (readyToEncrypt)
        {
            // Bla bla bla
            Page3Encrypted = page3 ^ getKS(SECRET_KEY, UID, nR);

            Serial.printf("nR: %04X ", nR);
            Serial.printf("aR: %04X \n", aR);

            // Reset flag
            readyToEncrypt = 0;
        }
        vTaskDelay(1); // Delay to avoid triggering the watchdog
    }
}

void IRAM_ATTR resetRxBuffer()
{
    rxBuffer = 0;
    rxBufferCounter = 0;
    tripleBitMessage = 0;
}

void IRAM_ATTR resetTxBuffer()
{
    txBuffer = 0;
    txBufferCounter = 0;
}

void IRAM_ATTR setMode(uint8_t mode)
{
    currentMode = mode;
    switch (mode)
    {
    case WRITING:
        // Set first value because it wont be called on time
        delayMicroseconds(25); // To sync it somehow
        write(TPD_DOUT, (txBuffer >> (7 - txBufferCounter++) & 1));
        delayMicroseconds(5);

        break;
    case MANCHESTER:
        manchesterBufferCounter = 0;
        // Start timer
        // timerWrite(timer, 0);
        // timerAlarmEnable(timer);
        // timerStart(timer);

        delayMicroseconds(25);
        // Place first bit
        write(TPD_DOUT, 0);
        delayMicroseconds(25);
        write(TPD_DOUT, 1);
        manchesterBufferCounter++;

        break;
    case READING:
    case WAITING:
    case BPLM:
        // Do nothing
        break;
    }
}

void IRAM_ATTR parseBuffer(uint8_t tripleBit = 0)
{

    if (tripleBit)
    {
        // Parse triple bit command
        switch (rxBuffer)
        {
        case READ_TRANSPARENCY:
            // Write UID and page 3 encrypted
            manchesterBuffer = Page3Encrypted;
            setMode(MANCHESTER);
            break;

        case WRITE_TRANSPARENCY:
            // Listen for nR and aR with BPLM
            // Wait a bit not to get caught by clock going down
            delayMicroseconds(10);
            setMode(BPLM);

            break;
        }
    }
    else
    {
        switch (rxBuffer)
        {

        case GET_CONFIG_PAGE_REQUEST:
            txBuffer = 0x21;
            setMode(WRITING);
            break;

        case READ_PHASE_REQUEST:
            txBuffer = 0x13;
            setMode(WRITING);
            break;

        default:
            // Go back to waiting if none of the messages are usable
            setMode(WAITING);
            break;
        }
    }
}

void IRAM_ATTR handleCLK() // Triggered when CLK changes (either to high or low)
{
    switch (currentMode)
    {
    case WAITING:
        // Do nothing, wait for DI confirmation
        break;

    case READING:
        // If its low pulse of clk, break
        if (!digitalRead(TPD_CLK))
            break;

        rxBuffer |= (digitalRead(TPD_DIN) << (7 - rxBufferCounter));

        // Classify messages of 3 bits or 8 bits
        switch (rxBufferCounter)
        {
        case 1:
            // If by now the message looks like 0b11xxxxxx, its a 3 bit message
            if ((rxBuffer >> 6) == 0b11)
            {
                tripleBitMessage = 1;
                timerWrite(manchesterTimer, 0);
                timerAlarmEnable(manchesterTimer);
                timerStart(manchesterTimer);
            }
            rxBufferCounter++;
            break;

        default:
            // Normal operation, add one to counter
            rxBufferCounter++;
            break;

        // ENDING CASES
        case 2:
            // Stop reading if its a 3 bit message
            if (tripleBitMessage)
            {
                // Filter the incomming message
                // Shift buffer all the way to the right
                rxBuffer >>= 5; // Leave 3 MSB bits (like 0b110 or 0b111)
                parseBuffer(1);
                resetRxBuffer();
            }
            else
                rxBufferCounter++;

            break;

        case 7:
            // When reached to 7 in counter ( 8 bits total ), parse buffer
            parseBuffer();
            resetRxBuffer();

            break;
        }

        break;

    case WRITING:
        // Only want low pulse to update for next high pulse
        if (digitalRead(TPD_CLK))
            break;

        delayMicroseconds(25); // To sync it with wave
        // Write corresponding message
        // MSB is set in setMode() to sync with clk signal
        if (txBufferCounter < 8)
            write(TPD_DOUT, (txBuffer >> (7 - txBufferCounter++) & 1));
        else
        {
            // When finished writing, return to waiting and reset buffer
            resetTxBuffer();
            setMode(WAITING);
        }

        break;

    case BPLM:
        // Do nothing since it needs to read on DI pin
        // On pulse high, stop reading
        if (digitalRead(TPD_CLK))
        {
            // Reset counter
            BPLMCounter = 0;
            // Set flag to encrypt
            readyToEncrypt = 1;
            readOnce = 1;
            // Back to waiting
            setMode(WAITING);
        }
        break;
    }
}

void IRAM_ATTR handleDI() // Triggered when DI changes (either high or low)
{

    switch (currentMode)
    {
    case WAITING:
        if (digitalRead(TPD_CLK) && digitalRead(TPD_DIN))
        {
            // If waiting, and both high, start reading
            setMode(READING);
        }
        break;

    case BPLM:
        // Read nR and aR
        // If DI is high, do not mind
        if (digitalRead(TPD_DIN))
            break;

        if (readOnce)
            break;

        switch (BPLMCounter)
        {
        case 0:
            // First DI pull down, no actual info
            // Just save micros()
            lastBPLM = micros();
            BPLMCounter = 0;
            BPLMCounter++;
            break;

        default:
            // 144 us and 176 us (avr 160) = 0
            // 208 us and 256 us (avr 232) = 1

            uint64_t deltaT = micros() - lastBPLM;
            if (deltaT > 144 && deltaT < 176)
            {
                // Its a 0, just advance counter
                BPLMCounter++;
            }
            else if (deltaT > 208 && deltaT < 256)
            {
                // Its a 1, mark and advance counter
                if (BPLMCounter >= 1 && BPLMCounter <= 32)
                {
                    nR |= (1 << (32 - BPLMCounter++)); // 32 because counter gets effective at 1, not 0
                }
                else if (BPLMCounter > 32 && BPLMCounter <= 64)
                {
                    aR |= (1 << (64 - BPLMCounter++));
                }
            }
            // Save last micros
            lastBPLM = micros();
            break;
        }

        break;

    default:
        break;
    }
}

void IRAM_ATTR writeManchester() // Will get called every 250 µs
{
    if (currentMode != MANCHESTER)
        return;
    // Write according to baud rate
    switch (manchesterBufferCounter)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        write(TPD_DOUT, 0);
        delayMicroseconds(130);
        write(TPD_DOUT, 1);
        manchesterBufferCounter++;
        break;
    case 37:
    {

        // Stop timer
        timerAlarmDisable(manchesterTimer);
        timerStop(manchesterTimer);

        delayMicroseconds(150);

        // Reset manchester data
        manchesterBuffer = 0;
        manchesterBufferCounter = 0;

        setMode(WAITING);
        break;
    }

    default:
    {
        // Just print it
        uint8_t bit = (manchesterBuffer >> (36 - manchesterBufferCounter++)) & 1;
        write(TPD_DOUT, !bit);
        delayMicroseconds(130);
        write(TPD_DOUT, bit);
        break;
    }
    }
}

void enableRMReply()
{
    // Reset variables
    Page3Encrypted = UID;
    readOnce = 0;

    // Turn off car transponder
    digitalWrite(TPD_PWR, LOW);

    // Communication related
    attachInterrupt(TPD_CLK, handleCLK, CHANGE);
    attachInterrupt(TPD_DIN, handleDI, CHANGE);

    // Second core related
    xTaskCreatePinnedToCore(ksEncrypter, "ksEncrypt", 10000, NULL, 1, &ksTask, 0);

    // Update tracking value
    replayEnabled = 1;

    Serial.println("[RM] - RM reply enabled");
}

void disableRMReply(bool enablePhysicalTransponder = 1)
{
    // Communication related
    detachInterrupt(TPD_DIN);
    detachInterrupt(TPD_CLK);

    // Delete second core task
    vTaskDelete(ksTask);

    // Turn on car transponder?
    digitalWrite(TPD_PWR, enablePhysicalTransponder);

    // Update tracking value
    replayEnabled = 0;

    Serial.println("[RM] - RM reply disabled");
}

void startCar()
{
    // Turn ACC
    digitalWrite(ACC, HIGH);
    delay(1000);

    // Enable transponder reply (just before ON to make sure its fully on)
    enableRMReply();

    // Turn ON
    digitalWrite(ON, HIGH);
    delay(2000); // Wait for fuel pump to prime and key verification

    // Setup security features for start
    // Two timers needed, one for short 1s (in case of a gear selected), other for 3s (in case of a non start, somehow)
    timerWrite(shortTimer, 0);
    timerWrite(longTimer, 0);

    timerAlarmEnable(shortTimer);
    timerAlarmEnable(longTimer);

    timerStart(shortTimer);
    timerStart(longTimer);

    // Depress clutch
    digitalWrite(CLUTCH, HIGH);

    // Begin start
    digitalWrite(ACC, LOW);
    digitalWrite(STR, HIGH);
    carStatus.status = STARTING;
    Serial.println("Waiting for the car to crank up");
}

void stopCar()
{
    digitalWrite(ACC, LOW);
    digitalWrite(ON, LOW);
    digitalWrite(STR, LOW);

    digitalWrite(TPD_PWR, HIGH);

    startedByRM = 0;
    remoteStartEnabled = 0;
    Serial.println("Car powered off");
}

void IRAM_ATTR shortSafetyCheck()
{
    Serial.println("Short timer triggered");
    if (carStatus.RPM < 1000)
        abort(); // Leave because something wrong is going on
}

void IRAM_ATTR longSafetyCheck()
{
    Serial.println("Long timer triggered");
    if (carStatus.status != RUNNING) // If car isnt running yet, something wrong is going on
        abort();
}

void checkForRM()
{
    // Car needs to be off to remote start it
    if (carStatus.status != OFF)
        return;

    if (remoteStartEnabled) // Remote start has been requested
    {
        // Begin prodecure of rolling through contacts
        startCar();
    }
}

void setupRM()
{
    pinMode(TPD_CLK, INPUT);
    pinMode(TPD_DIN, INPUT);
    pinMode(TPD_DOUT, OUTPUT);
    pinMode(ACC, OUTPUT);
    pinMode(ON, OUTPUT);
    pinMode(STR, OUTPUT);
    pinMode(CLUTCH, OUTPUT);
    pinMode(TPD_PWR, OUTPUT);

    digitalWrite(ACC, LOW);      // Turn ACC off
    digitalWrite(ON, LOW);       // Turn ON off
    digitalWrite(STR, LOW);      // Turn STR off
    digitalWrite(CLUTCH, LOW);   // Turn CLUTCH off
    digitalWrite(TPD_PWR, HIGH); // Power car transponder in case of manual start up

    // Timer related
    // Manchester
    manchesterTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(manchesterTimer, writeManchester, true);
    timerAlarmWrite(manchesterTimer, 254, true);
    timerAlarmDisable(manchesterTimer);
    timerStop(manchesterTimer);

    // Short safety measure
    shortTimer = timerBegin(1, 80, true);
    timerAttachInterrupt(shortTimer, shortSafetyCheck, true);
    timerAlarmWrite(shortTimer, 1000000, false); // Wait for 500 ms (500.000 µs)
    timerAlarmDisable(shortTimer);
    timerStop(shortTimer);

    // Long safety measure
    longTimer = timerBegin(2, 80, true);
    timerAttachInterrupt(longTimer, longSafetyCheck, true);
    timerAlarmWrite(longTimer, 3000000, false); // Wait for 3s (3.000.000 µs)
    timerAlarmDisable(longTimer);
    timerStop(longTimer);

    disableRMReply(); // Disable the remote start since its unknown the will of the owner

    Serial.println("[RM] - Service started");
}

// BLE

class BLEHandler : public BLECharacteristicCallbacks // Handler for any BLE input or output
{

    void onWrite(BLECharacteristic *pCharacteristic)
    {
        Serial.println("[BLE] - Written to characteristics");

        Serial.println(*(pCharacteristic->getData()));
        if (*pCharacteristic->getData() == 0x01)
        {

            // Request the remote start
            remoteStartEnabled = 1;
        }
        else
        {
            // Stop the car inmediately
            stopCar();
            abort();
        }
    }
    void onRead(BLECharacteristic *pCharacteristic)
    {
        Serial.println("[BLE] - Read from characteristics");
    }
};

class BLEServerHandler : public BLEServerCallbacks // Handler for client connect and disconnect
{
    void onConnect(BLEServer *pServer)
    {
        Serial.println("[BLE] - Client connected");
    }

    void onDisconnect(BLEServer *pServer)
    {
        Serial.println("[BLE] - Client disconnected");

        // As the device has disconnected, advertise again
        server->startAdvertising();
    }
};

void setupBLE()
{
    BLEDevice::init("350Z");
    server = BLEDevice::createServer();
    server->setCallbacks(new BLEServerHandler());

    BLEService *rmService = server->createService(RM_UUID);
    rmChar = rmService->createCharacteristic(RM_CHARACTERSITIC, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    rmChar->setCallbacks(new BLEHandler());
    rmChar->setValue(&carStatus.status, 1);

    rmService->start(); // Start service

    server->startAdvertising(); // Start advertising

    Serial.println("[BLE] - Service started");
}

// CAN FUNCTIONS

void canMessageHandler() // Handles CAN incomming message (called in main loop)
{
    if (can.readMessage(&canMsg) == MCP2515::ERROR_OK)
    {
        // Store the data
        switch (canMsg.can_id)
        {
        case 0x60D:
        {
            // From BCM
            // Store open door info
            carStatus.doorsOpen = ((canMsg.data[0] & 0b1000) ? 0xF : 0x0) << 4 | ((canMsg.data[0] & 0b10000) ? 0xF : 0x0);
            break;
        }
        case 0x1F9:
        {
            // RPM value
            carStatus.RPM = canMsg.data[2] << 8 | canMsg.data[3];
            break;
        }
        case 0x280:
        {
            // Vehicle speed
            carStatus.KPH = 10;
        }
            // case 0x358:
            // {
            //   // Key in - key in ignition position (look whatsapp marta)
            //   carStatus.key_in = (canMsg.data[0] == 0x1) ? 0xFF : 0x00;
            //   carStatus.key_in_ignition = (canMsg.data[4] == 0x82) ? 0xFF : 0x00;
            //   break;
            // }
        }
    };

    return;
}

void setupCAN()
{
    // Initialize CAN
    can.reset();
    can.setBitrate(CAN_500KBPS, MCP_8MHZ);
    can.setFilterMask(MCP2515::MASK0, false, 0x7FF);
    can.setFilter(MCP2515::RXF0, false, 0x60D);
    can.setFilter(MCP2515::RXF1, false, 0x1F9);

    can.setFilterMask(MCP2515::MASK1, false, 0x7FF);
    can.setFilter(MCP2515::RXF2, false, 0x280);
    can.setFilter(MCP2515::RXF3, false, 0x00);
    can.setFilter(MCP2515::RXF4, false, 0x00);
    can.setFilter(MCP2515::RXF5, false, 0x00);
    can.setNormalMode();
    // attachInterrupt(CAN_INT, canMessageHandler, FALLING);

    // Clean values of status struct
    carStatus.status = OFF;
    carStatus.doorsOpen = 0;
    carStatus.RPM = 0;
    carStatus.KPH = 0;

    Serial.println("[CAN] - Service started");
}

void updateCarStatus()
{
    // If something about remote start is going on, dont update anything but check for completion

    if (remoteStartEnabled)
    {
        if (carStatus.status == STARTING && carStatus.RPM > 5000)
        {
            // Car has successfully started
            digitalWrite(STR, LOW);
            digitalWrite(ACC, HIGH);
            carStatus.status == RUNNING;

            timerAlarmDisable(shortTimer);
            timerAlarmDisable(longTimer);

            timerStop(shortTimer);
            timerStop(longTimer);

            remoteStartEnabled = 0; // Reset variable

            startedByRM = 1;

            disableRMReply();

            // Release clutch
            digitalWrite(CLUTCH, LOW);

            Serial.println("Car started successfully");
        }
        return;
    }

    if (carStatus.RPM > 5000)
    {
        if (carStatus.status != RUNNING)
        {
            carStatus.status = RUNNING;
            Serial.println(carStatus.status);
        }
    }
    else if (carStatus.RPM == 0)
    {
        if (carStatus.status != OFF)
        {
            carStatus.status = OFF;
            Serial.println(carStatus.status);
        }
    }

    if (startedByRM && carStatus.KPH > 10) // Car is moving so key is inserted, we can disable remote start
    {
        startedByRM = 0;
        digitalWrite(ACC, LOW);
        digitalWrite(ON, LOW);
    }
}

void setup()
{
    // put your setup code here, to run once:
    WiFi.mode(WIFI_OFF);
    btStop();

    Serial.begin(115200);
    setupCAN();
    setupRM();
    setupBLE();
}

void loop()
{
    // put your main code here, to run repeatedly:
    canMessageHandler();

    checkForRM(); // Check for a remote start request

    updateCarStatus(); // Updates the status based on the can messages (in case of a non-remote start)
}
