#include <ArduinoBLE.h>

// --- GATT definition ---
BLEService msgService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLECharacteristic msgChar(
  "19B10001-E8F2-537E-4F6C-D104768A1214",
  BLERead | BLEWrite | BLEWriteWithoutResponse | BLENotify,
  6 // fixed 6 bytes
);

// --- States ---
uint32_t seqCounter = 0;
unsigned long lastPingMs = 0;

// ========== Functions ==========

// BLE initialization
void initBLE() {
  Serial.begin(115200);
  while (!Serial) {}

  if (!BLE.begin()) {
    Serial.println("ERR: BLE.begin() failed");
    while (1) {}
  }

  BLE.setLocalName("XIAO-MSG");
  BLE.setDeviceName("XIAO-MSG");

  BLE.setAdvertisedService(msgService);
  msgService.addCharacteristic(msgChar);
  BLE.addService(msgService);

  // Initial value (all zero, 6 bytes)
  uint8_t init[6] = {0};
  msgChar.writeValue(init, sizeof(init));

  BLE.advertise();
  Serial.println("Ready...");
}

// Print received data as Hex string
void printHex(const uint8_t* p, int n) {
  for (int i = 0; i < n; i++) {
    if (i) Serial.print(' ');
    Serial.print("0x");
    if (p[i] < 16) Serial.print('0');
    Serial.print(p[i], HEX);
  }
  Serial.println();
}

// Handle incoming data from central device (App -> XIAO)
void readIncoming() {
  if (!msgChar.written()) return;

  int n = msgChar.valueLength();
  const unsigned char* p = msgChar.value();

  Serial.print("RX["); Serial.print(n); Serial.print("]: ");
  printHex(p, n);

  // Example: parse only if 6-byte fixed packet
  if (n == 6) {
    uint8_t rseq = p[0];
    uint8_t m    = p[1];
    uint8_t cm   = p[2];
    uint8_t mm   = p[3];
    uint8_t A    = p[4];
    uint8_t stat = p[5];

    // Application logic goes here
    // Example: echo back the received packet
    if (msgChar.subscribed()) {
      msgChar.writeValue(p, n);
      Serial.println("TX: echo (notify)");
    }
  } else {
    // For variable length packets
    Serial.println("WARN: unexpected length");
  }
}

// Send 6-byte fixed packet (Notify)
bool sendPacket(uint8_t seq, uint8_t m, uint8_t cm, uint8_t mm, uint8_t A, uint8_t status) {
  if (!msgChar.subscribed()) {
    Serial.println("NOTE: not subscribed -> TX skipped");
    return false;
  }
  uint8_t packet[6] = { seq, m, cm, mm, A, status };
  bool ok = (msgChar.writeValue(packet, sizeof(packet)) > 0);

  Serial.print("TX: ");
  printHex(packet, sizeof(packet));
  return ok;
}

// Periodic packet sending with interval
void tickPeriodicPing(uint32_t interval_ms,
                      uint8_t m, uint8_t cm, uint8_t mm,
                      uint8_t A, uint8_t status) {
  unsigned long now = millis();
  if (now - lastPingMs < interval_ms) return;
  lastPingMs = now;
  sendPacket((uint8_t)(seqCounter++), m, cm, mm, A, status);
}

// Called on central connected
void onConnect(const BLEDevice& central) {
  Serial.print("Connected: ");
  Serial.println(central.address());
}

// Called on central disconnected
void onDisconnect() {
  Serial.println("Disconnected");
  initBLE();
}

// ========== Main ==========

void setup() {
  initBLE();
}

// Test distance countdown from 10m down to 0.50m
uint8_t cur_m = 10;   // meters
uint8_t cur_cm = 0;   // centimeters
uint8_t cur_mm = 0;   // millimeters (fixed)
uint8_t cur_A = 7;
uint8_t cur_status = 1;

void loop() {
  BLEDevice central = BLE.central();
  if (central) {
    onConnect(central);

    while (central.connected()) {
      readIncoming();

      tickPeriodicPing(500, cur_m, cur_cm, cur_mm, cur_A, cur_status); // every 500ms

      // --- Countdown logic for distance ---
      static unsigned long lastStep = 0;
      unsigned long now = millis();
      if (now - lastStep >= 500) {  // decrease every 500ms
        lastStep = now;

        if (cur_m > 0 || cur_cm > 50) {
          if (cur_cm == 0) {
            cur_m--;       // decrease 1m
            cur_cm = 99;   // reset cm
          } else {
            cur_cm--;      // decrease cm
          }
        }
      }
    }
    onDisconnect();
  }
}
