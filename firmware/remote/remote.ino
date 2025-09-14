#include <SoftwareSerial.h>
#include <Wire.h>
#include "SSD1306Wire.h"

// Инициализация дисплея
SSD1306Wire display(0x3c, SDA, SCL);

#define BUTTON_PIN D7         // Кнопка подключена к D7 (GPIO13)
#define BUTTON_CAM_PIN D3         // Кнопка камеры подключена к D3
#define LED_PIN LED_BUILTIN   // Встроенный светодиод
#define POTENTIOMETER_PIN A0  // Пин для потенциометра (A0)

// Пины для LoRa (RX - D6, TX - D5)
#define LORA_RX D5
#define LORA_TX D6

// Настроим SoftwareSerial для LoRa
SoftwareSerial loraSerial(LORA_RX, LORA_TX);

bool ledState = false;        // Состояние светодиода (выключен по умолчанию)
bool StateCam = false;        // Состояние кнопки камеры (выключен по умолчанию)
bool lastButtonState = HIGH;  // Предыдущее состояние кнопки
bool lastButtonStateCam = HIGH;  // Предыдущее состояние кнопки камеры
unsigned long buttonPressStart = 0;  // Время начала нажатия кнопки
unsigned long buttonPressStartCam = 0;  // Время начала нажатия кнопки камеры
unsigned long buttonPressDuration = 1000; // Длительность нажатия для переключения (1 секунда)
unsigned long buttonPressDurationCam = 1000; // Длительность нажатия для переключения камеры (1 секунда)
bool buttonTriggered = false; // Флаг для переключения светодиода
bool buttonTriggeredCam = false; // Флаг для переключения кнопки камеры

// Для потенциометра
int lastPotValue = -1;         // Последнее известное значение потенциометра
unsigned long movementEndTime = 0; // Время, когда движение остановилось
bool messageSent = false;       // Флаг, указывающий, было ли отправлено сообщение
const unsigned long stabilizationDelay = 500; // Задержка для определения остановки (в мс)

int lastSentAngle = -1; // Переменная для хранения последнего отправленного угла

// Переменные для хранения текущего состояния
String currentAngle = "N/A";  // Текущий угол (по умолчанию "N/A")
String currentReleState = "N/A"; // Текущее состояние реле (по умолчанию "N/A")
String currentTemperature = "N/A"; // Текущая температура (по умолчанию "N/A")
String currentstatetext = "N/A"; // переменная для состояния реле камеры

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON_CAM_PIN, INPUT_PULLUP);  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  loraSerial.begin(9600);

  // Инициализация дисплея
  display.init();
  display.clear();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  // Первоначальный вывод на дисплей
  display.drawString(10, 5, "ANGLE: " + currentAngle);
  display.drawString(10, 25, "RELE: " + currentReleState);
  display.drawString(10, 45, "TEMP: " + currentTemperature);
  display.display();
}

void loop() {
  handleButton();
  handleButtonCam();  
  handleLoRaReception();
  handlePotentiometer();
}

// Обработка кнопки
void handleButton() {
  int buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW) {
    if (lastButtonState == HIGH) {
      buttonPressStart = millis();
      buttonTriggered = false;
    }
    if (millis() - buttonPressStart >= buttonPressDuration && !buttonTriggered) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);

      Serial.println(ledState ? "Светодиод включен." : "Светодиод выключен.");
      loraSerial.println("P");

      buttonTriggered = true;
    }
  } else if (lastButtonState == LOW) {
    buttonPressStart = 0;
    buttonTriggered = false;
  }
  lastButtonState = buttonState;
}


// Обработка кнопки камеры
void handleButtonCam() {
  int buttonStateCam = digitalRead(BUTTON_CAM_PIN);
  if (buttonStateCam == LOW) {
    if (lastButtonStateCam == HIGH) {
      buttonPressStartCam = millis();
      buttonTriggeredCam = false;
    }
    if (millis() - buttonPressStartCam >= buttonPressDurationCam && !buttonTriggeredCam) {
      StateCam = !StateCam;
      //ledState = !ledState;
      //digitalWrite(LED_PIN, ledState ? LOW : HIGH);

      Serial.println(StateCam ? "камера тригернута" : "камера тригернута обратно");
      String statetext = String(StateCam ? "ON" : "OFF"); // преобразование типа bool в сторку
      Serial.println(statetext);
      currentstatetext = statetext;
      loraSerial.println("Swich_Cam");
      updateDisplay();  // Обновляем дисплей

      buttonTriggeredCam = true;
    }
  } else if (lastButtonStateCam == LOW) {
    buttonPressStartCam = 0;
    buttonTriggeredCam = false;
  }
  lastButtonStateCam = buttonStateCam;
}


// Обработка сообщений через LoRa
void handleLoRaReception() {
  if (loraSerial.available()) {
    String receivedMessage = loraSerial.readString();
    Serial.println("Принято сообщение: " + receivedMessage);

 // Проверяем на "ANGLE_SET:"
int anglePos = receivedMessage.indexOf("ANGLE_SET:");
if (anglePos != -1) {
  String angleValue = receivedMessage.substring(anglePos + 10);
  angleValue.trim();

  // Преобразуем строку в целое число
  int angle = angleValue.toInt();

  // Проверяем, является ли углом корректное целое значение
  if (angle >= 0 && angle <= 180) {
    // Ремапинг: 180 на 0, 0 на 360, остальные значения пропорционально
    int remappedAngle = map(angle, 0, 180, 360, 0);

    // Преобразуем remappedAngle в строку для сравнения
    String remappedAngleStr = String(remappedAngle);

    // Если угол изменился, обновляем его
    if (currentAngle != remappedAngleStr) {
      currentAngle = remappedAngleStr;
      updateDisplay();
    }
  }
}


    // Проверяем на "D2 is LOW" или "D2 is HIGH"
    int pinStateLowPos = receivedMessage.indexOf("D2 is LOW");
    int pinStateHighPos = receivedMessage.indexOf("D2 is HIGH");

    if (pinStateLowPos != -1 && currentReleState != "OFF") {
      currentReleState = "OFF";
      updateDisplay();
      digitalWrite(LED_PIN, HIGH);  // Выключаем светодиод
    } else if (pinStateHighPos != -1 && currentReleState != "ON") {
      currentReleState = "ON";
      updateDisplay();
      digitalWrite(LED_PIN, LOW);  // Включаем светодиод
    }

    // Проверяем на "STATE_CHANGED"
    if (receivedMessage.indexOf("STATE_CHANGED") != -1) {
      blinkLED(5);  // Мигаем светодиодом 5 раз
    }

// Проверяем на "TEMP:"
int tempPos = receivedMessage.indexOf("TEMP:");
if (tempPos != -1) {
  String tempValue = receivedMessage.substring(tempPos + 5);  // Извлекаем строку после "TEMP:"
  tempValue.trim();  // Убираем лишние пробелы
  int cPos = tempValue.indexOf("C");  // Находим позицию символа "C"
  
  if (cPos != -1) {
    tempValue = tempValue.substring(0, cPos);  // Обрезаем строку до символа "C"
  }

  // Проверяем, изменилась ли температура
  if (tempValue != currentTemperature) {
    currentTemperature = tempValue;  // Обновляем температуру
    updateDisplay();  // Обновляем дисплей
  }
}

  }
}

// Обновление данных на дисплее
void updateDisplay() {
  display.clear();
  display.drawString(10, 5, "ANGLE: " + currentAngle);
  display.drawString(10, 25, "RELE: " + currentReleState);
  display.drawString(10, 45, "TEMP: " + currentTemperature);
  display.drawString(70, 45, "cam: " + currentstatetext);
  display.display();
}

// Чтение и отправка данных с потенциометра
void handlePotentiometer() {
  int potValue = analogRead(POTENTIOMETER_PIN);
  if (abs(potValue - lastPotValue) > 5) {
    lastPotValue = potValue;
    movementEndTime = millis();
    messageSent = false;
  }
  if (!messageSent && millis() - movementEndTime >= stabilizationDelay) {
    int angle = map(lastPotValue, 0, 1023, 180, 0);
    if (abs(angle - lastSentAngle) > 1) {
      loraSerial.println(String("A:") + angle);
      Serial.println("Отправлен угол: " + String(angle));
      lastSentAngle = angle;
      messageSent = true;
    }
  }
}

// Проверка, является ли строка числом
bool isNumeric(String str) {
  for (int i = 0; i < str.length(); i++) {
    if (!isDigit(str[i])) {
      return false;
    }
  }
  return true;
}

// Функция мигания светодиода
void blinkLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, LOW);  // Включаем светодиод
    delay(100);                   // Задержка 100 мс
    digitalWrite(LED_PIN, HIGH); // Выключаем светодиод
    delay(100);                   // Задержка 100 мс
  }

  // Устанавливаем светодиод в состояние, которое было перед STATE_CHANGED
  if (currentReleState == "ON") {
    digitalWrite(LED_PIN, LOW); // Включаем светодиод
  } else {
    digitalWrite(LED_PIN, HIGH);  // Выключаем светодиод
  }
}

