#include <RadioLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <nrf.h>          // needed for RTC and low-power registers

// RAK4600 INTERNAL HARDWARE DEFINITIONS
// Internal Radio Connections (SX1276)
#define RAK4600_NSS     4
#define RAK4600_MOSI    5
#define RAK4600_MISO    6
#define RAK4600_SCK     7
#define RAK4600_DIO0    27
#define RAK4600_DIO1    28
#define RAK4600_RESET   RADIOLIB_NC

// Antenna Switch Control (VCTL1/VCTL2)
#define RADIO_TX_EN     16
#define RADIO_RX_EN     15

// External Sensor Pin (IO1 on J11)
#define ONE_WIRE_BUS    14

// LORAWAN KEYS
uint64_t nodeDeviceEUI = 0x0000000000000000;
uint64_t nodeAppEUI    = 0x0000000000000000;
uint8_t nodeAppKey[]   = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Define the radio module with correct pins
SX1276 radio = new Module(RAK4600_NSS, RAK4600_DIO0, RAK4600_RESET, RAK4600_DIO1);
LoRaWANNode node(&radio, &EU868);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

volatile bool rtcWakeFlag = false;

// RTC2 interrupt handler
extern "C" void RTC2_IRQHandler(void) {
  if (NRF_RTC2->EVENTS_COMPARE[0]) {
    NRF_RTC2->EVENTS_COMPARE[0] = 0;   
    NRF_RTC2->TASKS_STOP = 1;          
    rtcWakeFlag = true;                
  }
}

// Low-level: sleep using RTC2 for ms milliseconds (System ON)
void rtcSleepMs(uint32_t ms) {
  // 1. Make sure LFCLK (low-frequency clock) is running
  NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
  NRF_CLOCK->TASKS_LFCLKSTART = 1;
  while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {}

  // 2. Configure RTC2
  // 32768 Hz / (PRESCALER + 1) = Freq
  // PRESCALER = 1023 -> 32 Hz -> tick ~31.25 ms
  NRF_RTC2->TASKS_STOP  = 1;
  NRF_RTC2->TASKS_CLEAR = 1;
  NRF_RTC2->PRESCALER   = 1023;   // ~32 Hz

  rtcWakeFlag = false;

  // 3. Convert ms to RTC ticks
  const uint32_t freq = 32;       
  uint32_t ticks = (ms * freq) / 1000;
  if (ticks == 0) ticks = 1;
  if (ticks > ((1UL << 24) - 1)) {
    ticks = (1UL << 24) - 1;      
  }

  NRF_RTC2->CC[0] = ticks;
  NRF_RTC2->EVENTS_COMPARE[0] = 0;
  NRF_RTC2->EVTENSET = RTC_EVTENSET_COMPARE0_Msk;
  NRF_RTC2->INTENSET = RTC_INTENSET_COMPARE0_Msk;

  NVIC_ClearPendingIRQ(RTC2_IRQn);
  NVIC_SetPriority(RTC2_IRQn, 7);
  NVIC_EnableIRQ(RTC2_IRQn);

  // 4. Start RTC
  NRF_RTC2->TASKS_START = 1;

  // 5. Enter System ON sleep (CPU off - RAM on - wake on RTC interrupt)
  while (!rtcWakeFlag) {
    __WFE();     
  }

  NRF_RTC2->INTENCLR = RTC_INTENCLR_COMPARE0_Msk;
  NRF_RTC2->EVTENCLR = RTC_EVTENCLR_COMPARE0_Msk;
}

void systemOnRtcSleep(uint32_t ms) {

  // Put LoRaWAN radio to sleep
  radio.sleep();

  // Disable RF switch
  pinMode(RADIO_TX_EN, OUTPUT);
  pinMode(RADIO_RX_EN, OUTPUT);
  digitalWrite(RADIO_TX_EN, LOW);
  digitalWrite(RADIO_RX_EN, LOW);

  pinMode(ONE_WIRE_BUS, INPUT);

  // Stop UART to avoid extra current in USB-UART path
  Serial.flush();
  Serial.end();

  // Timed sleep using RTC2
  rtcSleepMs(ms);

  // WAKE-UP
  // Re-enable UART
  Serial.setPins(18, 19); // RX=18, TX=19
  Serial.begin(115200);
  delay(10);

  // Restore sensor pin (DallasTemperature will drive it as needed later)
  pinMode(ONE_WIRE_BUS, OUTPUT);
}

void setup() {
  // Map Serial to the RAK4600 UART pins (J10)
  Serial.setPins(18, 19); // RX=18, TX=19
  Serial.begin(115200);
  delay(3000);
  Serial.println(F("RAK4600 Firmware Starting..."));

  // 1. SPI for RAK4600
  SPI.setPins(RAK4600_MISO, RAK4600_SCK, RAK4600_MOSI);
  SPI.begin(); // Start SPI

  // 2. Setup Antenna Switch
  pinMode(RADIO_TX_EN, OUTPUT);
  pinMode(RADIO_RX_EN, OUTPUT);
  digitalWrite(RADIO_TX_EN, LOW);
  digitalWrite(RADIO_RX_EN, LOW);

  // 3. Initialize Sensor
  pinMode(ONE_WIRE_BUS, OUTPUT);
  sensors.begin();
  if (sensors.getDeviceCount() > 0) {
    sensors.setResolution(12);
  } else {
    Serial.println(F("No 1-Wire devices found."));
  }

  // 4. Initialize Radio
  Serial.print(F("Initializing Radio... "));
  radio.setRfSwitchPins(RADIO_RX_EN, RADIO_TX_EN);
  
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Success!"));
  } else {
    Serial.print(F("Radio Error: "));
    Serial.println(state);
    while (true);  // hard stop
  }

  // 5. Join LoRaWAN
  Serial.println(F("Joining LoRaWAN..."));
  node.beginOTAA(nodeAppEUI, nodeDeviceEUI, nodeAppKey, nodeAppKey);
  state = node.activateOTAA();
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Joined!"));
  } else {
    Serial.print(F("Join Failed: "));
    Serial.println(state);
    // Let loop() handle re-tries with sleep
  }
}

void loop() {
  // Confirm join
  if (!node.isActivated()) {
    Serial.println(F("Re-joining..."));
    if (node.activateOTAA() == RADIOLIB_ERR_NONE) {
      Serial.println(F("Joined!"));
    } else {
      Serial.println(F("Join failed, sleeping 10 s..."));
      // Sleep 10 s before retrying join
      systemOnRtcSleep(10000);
      return;
    }
  }

  // Read temperature
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);

  // Safe integer printing to avoid ABI mismatch issues
  int tempInt = (int)temp;
  int tempDec = abs((int)(temp * 100) % 100);

  Serial.print(F("Temp: "));
  Serial.print(tempInt);
  Serial.print(F("."));
  if (tempDec < 10) Serial.print('0');
  Serial.println(tempDec);

  if (temp < -50 || temp > 125) {
    Serial.println(F("Temp out of range, using 25.00 C fallback"));
    temp = 25.0;
  }

  // Encode temperature * 100 as int16
  int16_t tempScaled = (int16_t)(temp * 100.0f);
  uint8_t payload[2];
  payload[0] = (uint8_t)(tempScaled >> 8);
  payload[1] = (uint8_t)(tempScaled & 0xFF);

  Serial.println(F("Sending..."));
  int state = node.sendReceive(payload, 2);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Sent OK"));
  } else {
    Serial.print(F("Send Error: "));
    Serial.println(state);
  }

  // Sleep for 1 minute in System ON + RTC wake
  Serial.println(F("Sleeping for 60s..."));
  systemOnRtcSleep(60000);   // 60000 ms = 1 minute
}
