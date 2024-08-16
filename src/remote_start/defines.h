#ifndef DEFINES_H
#define DEFINES_H
#include <stdint.h>
#include <mcp2515.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define TPD_PWR 22 // To power off the transponder with a T connection
#define TPD_DIN 35
#define TPD_DOUT 32
#define TPD_CLK 34

#define ACC 16
#define ON 17
#define STR 21

#define CLUTCH 14

#define CAN_CS 5
#define CAN_INT 4
#define WAKE_UP_PIN GPIO_NUM_4

#define RM_UUID "0f210c11-150f-0f19-200a-250b0a0c1101"
#define RM_CHARACTERSITIC "0f210c11-150f-0f19-200a-250b0a0c1113"

#define DEBUG 0

/////////////GLOBAL/////////////
enum position
{
    OFF,
    IGNITION,
    STARTING,
    RUNNING
};
///////////////////////////////

//////////////BLE//////////////
BLEServer *server;
BLECharacteristic *rmChar;
BLEScan *scanner;

TaskHandle_t scanTask;
uint8_t foundDevices = 0x00; // 00 if none - F0 if apple watch - 0F if iphone - FF if both
bool checkedIphone = 0;
bool checkedWatch = 0;
///////////////////////////////

//////////////CAN//////////////
MCP2515 can(CAN_CS);
typedef struct carStatus
{
    uint8_t status;    // Corresponds to enum position (OFF, IGNITION, STARTING, RUNNING)
    uint8_t doorsOpen; // F if open, 0 if closed // Pilot door - copilot door
    uint16_t RPM;      // Value of rpms
    uint16_t KPH;      // Speed of car
} CarStatus;
struct can_frame canMsg;
CarStatus carStatus; // Initialized in setupCAN()
///////////////////////////////

//////////////CTRL/////////////
uint8_t remoteStartEnabled = 0;
///////////////////////////////

//////////////RMST/////////////
enum modes
{
    WAITING,
    READING,
    WRITING,
    MANCHESTER,
    BPLM,
};

enum messageToReply
{
    GET_CONFIG_PAGE_REQUEST = 0b00000111,
    READ_PHASE_REQUEST = 0b00001000,
    READ_TRANSPARENCY = 0b111,  // 0b111
    WRITE_TRANSPARENCY = 0b110, // 0b110
};

// GLOBAL
uint8_t currentMode = WAITING;
uint8_t startedByRM = 0;
uint8_t replayEnabled = 0;
void disableRMReply(bool enablePhysicalTransponder); // Just a definition to avoid problems afterwards

// TIMERS
hw_timer_t *manchesterTimer = NULL;
hw_timer_t *shortTimer = NULL;
hw_timer_t *longTimer = NULL;

// RX BUFFER RELATED
uint64_t rxBuffer = 0;
uint8_t rxBufferCounter = 0;
uint8_t tripleBitMessage = 0;

// BPLM
uint32_t lastBPLM = 0;
uint8_t BPLMCounter = 0;
uint8_t readOnce = 0;

// MANCHESTER
uint32_t page3 = 0x8E000004;
uint32_t manchesterBuffer = 0;
uint8_t manchesterBufferCounter = 0;

// TX BUFFER RELATED
uint32_t txBuffer = 0;
uint8_t txBufferCounter = 0;

// Encryption
TaskHandle_t ksTask;
uint8_t readyToEncrypt = 0;
const uint32_t UID = 0;        // UID GOES HERE
const uint64_t SECRET_KEY = 0; // Secret key goes here
uint32_t Page3Encrypted = UID;
uint32_t nR = 0;
uint32_t aR = 0;
uint32_t ks = 0;

void IRAM_ATTR write(uint8_t pin, uint8_t value) { digitalWrite(pin, !value); }

///////////////////////////////

#endif