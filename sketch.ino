#include <DHT.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>

// === CONFIGURACIÓN DE PINES ===
#define DHTPIN 2
#define DHTTYPE DHT22

#define LDR1_PIN A0
#define LDR2_PIN A1
#define SERVO_PIN 9

#define LED_START_PIN 4
#define NUM_LEDS 8

#define IR_RECEIVE_PIN 13

// === OBJETOS ===
DHT dht(DHTPIN, DHTTYPE);
Servo servo;
LiquidCrystal_I2C lcd(0x27, 16, 2);
void initializeReceiver() {
  // set up the receiver to receive input the NEW way- it changed from earlier versions)
  IrReceiver.begin(IR_RECEIVE_PIN);
}

// === ESTADO DEL SISTEMA ===
bool sistemaActivo = true;
bool modoTest = false;
float tempDeseada = 25.0;
int zonaMuerta = 2;
int umbralLuz = 600;

void setup() {
  Serial.begin(9600);
  dht.begin();
  servo.attach(SERVO_PIN);
  initializeReceiver();

  // Iniciar IR Receiver (prioridad máxima)
  

  lcd.begin(16, 2);
  lcd.backlight();
  lcd.print("Iniciando...");

  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_START_PIN + i, OUTPUT);
  }

  delay(2000);
  lcd.clear();
}

void loop() {
  // === PRIORIDAD: RECEPCIÓN DE MANDO IR ===
  if (IrReceiver.decode()) {
    translateIR(); // calls our translate function
    IrReceiver.resume();  // Receive the next value
    Serial.println(IrReceiver.decodedIRData.command);
  }
  // Si el sistema está apagado → standby
  if (!sistemaActivo) {
    modoStandby();
    return;
  }
  
  // === LECTURA DE SENSORES ===
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int luz1 = analogRead(LDR1_PIN);
  int luz2 = analogRead(LDR2_PIN);
  int diff = abs(luz1 - luz2);

  // Reintento si devuelve NaN
  if (isnan(temp) || isnan(hum)) {
    delay(2000);
    temp = dht.readTemperature();
    hum = dht.readHumidity();
  }

  // Validación de sensores
  if (isnan(temp) || isnan(hum)) {
    errorLCD("Fallo DHT");
    return;
  }

  if (diff > 200) {
    errorLCD("LDR desync");
    return;
  }

  // === CONTROL DEL SERVO POR TEMPERATURA ===
  if (temp < tempDeseada - zonaMuerta) {
    servo.write(0);
  } else if (temp > tempDeseada + zonaMuerta) {
    servo.write(180);
  } else {
    servo.write(90);
  }

  // === MODO TEST (PARPADEO LEDS) ===
  if (modoTest) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("MODO TEST");
    for (int i = 0; i < NUM_LEDS; i++) {
      digitalWrite(LED_START_PIN + i, (i % 2 == 0) ? HIGH : LOW);
      delay(50);
    }
    delay(500);
    for (int i = 0; i < NUM_LEDS; i++) {
      digitalWrite(LED_START_PIN + i, (i % 2 != 0) ? HIGH : LOW);
    }
    delay(500);
    return;
  }

  // === CONTROL DE LEDS SEGÚN LUZ ===
  int luz = map(luz1, 0, 1023, 0, 100);
  int ledsEnc = map(luz, 0, 100, NUM_LEDS, 0);

  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_START_PIN + i, (i < ledsEnc) ? HIGH : LOW);
  }

  // === MOSTRAR EN LCD ===
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C H:");
  lcd.print(hum, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Luz:");
  lcd.print(luz);
  lcd.print("%");

  // === MONITOREO SERIAL (simulación IP) ===
  Serial.print("DATA|T:");
  Serial.print(temp);
  Serial.print("|H:");
  Serial.print(hum);
  Serial.print("|L:");
  Serial.println(luz);
  
}


// === FUNCIÓN DE CONTROL IR ===
void translateIR() {
  // Takes command based on IR code received
  switch (IrReceiver.decodedIRData.command) {

    case 162: // POWER
      sistemaActivo = !sistemaActivo;
      Serial.println("IR: POWER → ON/OFF");
      delay(2000);
      break;

    case 2: // VOL+ / PLUS
      umbralLuz += 50;
      Serial.print("IR: PLUS → Umbral: ");
      Serial.println(umbralLuz);
      break;

    case 152: // VOL- / MINUS
      umbralLuz -= 50;
      Serial.print("IR: MINUS → Umbral: ");
      Serial.println(umbralLuz);
      break;

    case 34: // FUNC/STOP → Modo TEST ON/OFF
      modoTest = !modoTest;
      Serial.println("IR: TEST → LEDs modo diagnóstico");
      break;

    default:
      Serial.print("IR: Código no asignado → ");
      Serial.println(IrReceiver.decodedIRData.command);
  }
}

// === MODO STANDBY ===
void modoStandby() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Modo STANDBY");
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_START_PIN + i, LOW);
  }
  servo.write(90);
}

// === MENSAJE DE ERROR EN LCD ===
void errorLCD(String msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("! ERROR !");
  lcd.setCursor(0, 1);
  lcd.print(msg);
  Serial.println("ERROR| " + msg);
  delay(3000);
}
