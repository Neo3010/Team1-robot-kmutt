#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Grove_Temperature_And_Humidity_Sensor.h>

// ===== BLE UUIDs =====
#define SERVICE_UUID        "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"

// ===== Motor Pins =====
const int M1A = 32, M1B = 33, M2A = 25, M2B = 26;

// ===== PWM =====
#define CH_M1A 4
#define CH_M1B 5
#define CH_M2A 6
#define CH_M2B 7
#define PWM_FREQ 1000
#define PWM_RES  8

// ===== Speed =====
int MOTOR_SPEED = 200;
int TURN_SPEED  = 80;   // inner wheel speed during a curve turn

// ===== LDR & LED =====
#define LDR_PIN   34
#define LED_PIN   12
#define THRESHOLD 1000

// ===== DHT11 =====
#define DHT_PIN 14
DHT dht(DHT_PIN, DHT11);

// ===== Watchdog =====
#define WATCHDOG_MS 300

// ===== BLE State =====
BLEServer*         pServer         = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool bleConnected = false;
bool doReconnect  = false;  // set in callback, handled safely in loop()

unsigned long lastCmdTime = 0;
bool isMoving = false;

// ════════════════════════════════════════════════
//  MOTOR FUNCTIONS
// ════════════════════════════════════════════════
void motorPWM(int ch, int val) { ledcWrite(ch, constrain(val, 0, 255)); }

// M1A HIGH = left  BACKWARD  | M1B HIGH = left  FORWARD
// M2A HIGH = right FORWARD   | M2B HIGH = right BACKWARD

void stopMotors() {
  motorPWM(CH_M1A,0); motorPWM(CH_M1B,0);
  motorPWM(CH_M2A,0); motorPWM(CH_M2B,0);
  Serial.println("[MOT] STOP");
  isMoving=false;
}
void moveForward() {
  motorPWM(CH_M1A,0); motorPWM(CH_M1B,MOTOR_SPEED);  // left  FORWARD
  motorPWM(CH_M2A,MOTOR_SPEED); motorPWM(CH_M2B,0);  // right FORWARD
  Serial.printf("[MOT] FWD: L=%d R=%d\n", MOTOR_SPEED, MOTOR_SPEED);
  isMoving=true;
}
void moveBackward() {
  motorPWM(CH_M1A,MOTOR_SPEED); motorPWM(CH_M1B,0);  // left  BACKWARD
  motorPWM(CH_M2A,0); motorPWM(CH_M2B,MOTOR_SPEED);  // right BACKWARD
  Serial.printf("[MOT] BWD: L=%d R=%d\n", MOTOR_SPEED, MOTOR_SPEED);
  isMoving=true;
}
void turnLeft() {
  motorPWM(CH_M1A,0); motorPWM(CH_M1B,TURN_SPEED);   // left  SLOW forward
  motorPWM(CH_M2A,MOTOR_SPEED); motorPWM(CH_M2B,0);  // right FULL forward
  Serial.printf("[MOT] LEFT: L=%d R=%d\n", TURN_SPEED, MOTOR_SPEED);
  isMoving=true;
}
void turnRight() {
  motorPWM(CH_M1A,0); motorPWM(CH_M1B,MOTOR_SPEED);  // left  FULL forward
  motorPWM(CH_M2A,TURN_SPEED); motorPWM(CH_M2B,0);   // right SLOW forward
  Serial.printf("[MOT] RIGHT: L=%d R=%d\n", MOTOR_SPEED, TURN_SPEED);
  isMoving=true;
}

// ════════════════════════════════════════════════
//  COMMAND HANDLER
// ════════════════════════════════════════════════
void handleCommand(const char* cmd, int len) {
  if (len == 0) return;

  // Speed command: "V:200"
  if (len > 2 && cmd[0] == 'V' && cmd[1] == ':') {
    int spd = atoi(cmd + 2);
    MOTOR_SPEED = constrain(spd, 0, 255);
    TURN_SPEED  = constrain(spd * 40 / 100, 0, 255);  // inner wheel = 40% of speed
    Serial.printf("[SPD] %d/255\n", MOTOR_SPEED);
    return;
  }

  // Motor command: F/B/L/R/S
  lastCmdTime = millis();
  Serial.printf("[CMD] %c\n", cmd[0]);
  switch (cmd[0]) {
    case 'F': moveForward();  break;
    case 'B': moveBackward(); break;
    case 'L': turnLeft();     break;
    case 'R': turnRight();    break;
    case 'S': stopMotors();   break;
  }
}

// ════════════════════════════════════════════════
//  BLE CALLBACKS
// ════════════════════════════════════════════════
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) {
    bleConnected = true;
    doReconnect  = false;
    Serial.println("[BLE] Client connected");
  }
  void onDisconnect(BLEServer* s) {
    bleConnected = false;
    doReconnect  = true;  // handled in loop() to avoid GATT crash
    stopMotors();
    Serial.println("[BLE] Client disconnected — will re-advertise");
  }
};

class CharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    if (value.length() > 0)
      handleCommand(value.c_str(), value.length());
  }
};

// ════════════════════════════════════════════════
//  LDR
// ════════════════════════════════════════════════
void checkLDR() {
  int v = analogRead(LDR_PIN);
  digitalWrite(LED_PIN, v < THRESHOLD ? HIGH : LOW);
  Serial.printf("[LDR] %d | LED: %s\n", v, v < THRESHOLD ? "ON" : "OFF");
}

// ════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════
void setup() {
  // Prevent motor twitch on boot
  pinMode(M1A,OUTPUT); digitalWrite(M1A,LOW);
  pinMode(M1B,OUTPUT); digitalWrite(M1B,LOW);
  pinMode(M2A,OUTPUT); digitalWrite(M2A,LOW);
  pinMode(M2B,OUTPUT); digitalWrite(M2B,LOW);

  // PWM channels
  ledcSetup(CH_M1A,PWM_FREQ,PWM_RES); ledcAttachPin(M1A,CH_M1A);
  ledcSetup(CH_M1B,PWM_FREQ,PWM_RES); ledcAttachPin(M1B,CH_M1B);
  ledcSetup(CH_M2A,PWM_FREQ,PWM_RES); ledcAttachPin(M2A,CH_M2A);
  ledcSetup(CH_M2B,PWM_FREQ,PWM_RES); ledcAttachPin(M2B,CH_M2B);
  Serial.printf("[PWM] M1A ch%d -> GPIO%d | M1B ch%d -> GPIO%d\n", CH_M1A,M1A,CH_M1B,M1B);
  Serial.printf("[PWM] M2A ch%d -> GPIO%d | M2B ch%d -> GPIO%d\n", CH_M2A,M2A,CH_M2B,M2B);

  // LED & LDR
  pinMode(LED_PIN,OUTPUT); digitalWrite(LED_PIN,LOW);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);

  stopMotors();
  Serial.begin(115200);
  delay(500);

  // DHT11 — 2s warm-up so first read isn't NaN
  dht.begin();
  delay(2000);

  // ── BLE ──
  BLEDevice::init("ESP32_Robot_KMUTT");
  BLEDevice::setPower(ESP_PWR_LVL_P9);  // max TX power for range/stability

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE    |
    BLECharacteristic::PROPERTY_WRITE_NR |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new CharCallbacks());
  pService->start();

  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->setMinPreferred(0x06);
  pAdv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("=================================");
  Serial.println("  ESP32_Robot_KMUTT — BLE READY");
  Serial.printf("  Motor Speed: %d/255\n", MOTOR_SPEED);
  Serial.printf("  DHT Pin    : GPIO%d\n", DHT_PIN);
  Serial.printf("  LDR Pin    : GPIO%d\n", LDR_PIN);
  Serial.printf("  LED Pin    : GPIO%d\n", LED_PIN);
  Serial.println("=================================");
}

// ════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════
void loop() {

  // Re-advertise after disconnect — called here NOT inside BLE callback (prevents GATT crash)
  if (doReconnect) {
    doReconnect = false;
    delay(500);
    BLEDevice::startAdvertising();
    Serial.println("[BLE] Re-advertising — waiting for client...");
  }

  // Watchdog — stop motors if no command received within 300ms
  if (isMoving && millis() - lastCmdTime > WATCHDOG_MS) {
    stopMotors();
    Serial.println("[WDG] Timeout — motors stopped");
  }

  // LDR every 500ms
  static unsigned long lastLDR = 0;
  if (millis() - lastLDR >= 500) {
    lastLDR = millis();
    checkLDR();
  }

  // DHT11 every 2000ms — notify BLE client
  static unsigned long lastDHT = 0;
  if (millis() - lastDHT >= 2000) {
    lastDHT = millis();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      char buf[32];
      snprintf(buf, sizeof(buf), "H:%.1f:%.1f", h, t);
      Serial.printf("[DHT] %.1f%% | %.1f C\n", h, t);
      if (bleConnected) {
        pCharacteristic->setValue(buf);
        pCharacteristic->notify();
      }
    } else {
      Serial.println("[DHT] Read failed — check wiring on GPIO14");
    }
  }

  delay(10);
}
