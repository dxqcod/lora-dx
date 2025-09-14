#include <SoftwareSerial.h>
#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Пины подключения LoRa (TX на RX модуля, RX на TX модуля)
#define LORA_RX D5
#define LORA_TX D6

#define RELAY_PIN D1
#define SERVO_PIN D3  // Пин для сервопривода
#define D2_PIN D2     // Пин для считывания состояния
#define D0_PIN D0     // Пин для подключения второго реле
#define D4_PIN D4     // Пин для подключения реле камеры
#define ONE_WIRE_BUS D7 // Пин для подключения DS18B20

SoftwareSerial LoRaSerial(LORA_RX, LORA_TX); // Создаем программный UART
bool relayState = false;
bool relayStateCam = false;
Servo servo; // Экземпляр для управления сервоприводом

OneWire oneWire(ONE_WIRE_BUS); // Создаем объект для работы с OneWire
DallasTemperature sensors(&oneWire); // Создаем объект для работы с DS18B20

unsigned long lastTempCheck = 0; // Время последней проверки температуры
const unsigned long tempCheckInterval = 500; // Интервал проверки температуры (500 мс)
float lastTemperature = 0.0; // Последняя измеренная температура
bool relayStateTemp = false; // Состояние реле для температуры

void setup() {
  Serial.begin(115200);          // Для мониторинга через Serial
  LoRaSerial.begin(9600);        // Скорость UART для LoRa

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(D2_PIN, INPUT_PULLUP); // Включаем внутренний подтягивающий резистор для D2
  pinMode(D0_PIN, OUTPUT);       // Пин для второго реле
  pinMode(D4_PIN, OUTPUT);       // Пин для реле камеры
  digitalWrite(D4_PIN, HIGH);     // Устанавливаем реле камеры в начальное состояние
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP); // Включаем внутренний подтягивающий резистор для D7

  // Увеличиваем разрядность ШИМ и настраиваем сервопривод
  analogWriteResolution(16); // Увеличиваем разрядность ШИМ до 16 бит
  servo.attach(SERVO_PIN, 544, 2444); // Указываем пин и диапазон ширины импульса

  servo.write(90); // Устанавливаем сервопривод в центральное положение

  sensors.begin(); // Инициализируем датчик температуры
  
  Serial.println("LoRa Test Started in RX Mode");
}

float getTemperature() {
  sensors.requestTemperatures(); // Запрашиваем температуру
  float temperature = sensors.getTempCByIndex(0); // Получаем температуру с первого датчика
  return temperature != DEVICE_DISCONNECTED_C ? temperature : -127.0; // Возвращаем температуру или -127, если датчик не подключён
}

void sendTemperature() {
  float temperature = getTemperature();
  if (temperature != -127.0) {
    String tempMessage = "TEMP:" + String(temperature, 1) + "C"; // Формируем сообщение
    LoRaSerial.println(tempMessage); // Отправляем через LoRa
    Serial.println(tempMessage); // Отправляем в монитор порта
  } else {
    Serial.println("Temperature sensor not connected!");
  }
}

void controlRelayByTemperature(float temperature) {
  if (temperature > 32 && !relayStateTemp) {
    digitalWrite(D0_PIN, HIGH); // Включаем реле
    relayStateTemp = true;
    Serial.println("Relay ON due to high temperature");
  } else if (temperature < 30 && relayStateTemp) {
    digitalWrite(D0_PIN, LOW); // Выключаем реле
    relayStateTemp = false;
    Serial.println("Relay OFF due to low temperature");
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Проверяем наличие данных через LoRa
  if (LoRaSerial.available()) {
    String message = LoRaSerial.readString();  // Считываем данные с LoRa
    Serial.println("Received: " + message);    // Печатаем на Serial монитор

    if (message.indexOf("P") >= 0) {
      // Переключаем состояние реле с задержкой для стабильного измерения состояния D2
      relayState = !relayState;
      digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
      delay(50); // Небольшая задержка, чтобы D2 успел переключиться
       
      // Проверка состояния пина D2 после переключения реле
      if (digitalRead(D2_PIN) == HIGH) {
        Serial.println("D2 is HIGH");
        LoRaSerial.println("D2 is HIGH");
      } else {
        Serial.println("D2 is LOW");
        LoRaSerial.println("D2 is LOW");
      }

      // Отправляем температуру
      sendTemperature();

      // Отправляем сообщение об изменении состояния
      Serial.println(relayState ? "Relay ON" : "Relay OFF");
      LoRaSerial.println("STATE_CHANGED");
      Serial.println("Sent confirmation: STATE_CHANGED");
    }

    // Если сообщение содержит "Swich_Cam"
    if (message.startsWith("Swich_Cam")) {
      // Переключаем состояние реле на противоположное
      relayStateCam = !relayStateCam;
      digitalWrite(D4_PIN, relayStateCam ? HIGH : LOW);      
    }

    // Если сообщение содержит "A:<угол>"
    if (message.startsWith("A:")) {
      int angle = message.substring(2).toInt(); // Извлекаем угол из сообщения
      if (angle >= 0 && angle <= 180) {
        servo.write(angle); // Поворачиваем сервопривод на указанный угол
        Serial.println("Servo moved to angle: " + String(angle));

        // Проверка состояния пина D2
        if (digitalRead(D2_PIN) == HIGH) {
          Serial.println("D2 is HIGH");
          LoRaSerial.println("D2 is HIGH");
        } else {
          Serial.println("D2 is LOW");
          LoRaSerial.println("D2 is LOW");
        }

        // Отправляем температуру
        sendTemperature();

        // Отправляем подтверждение установки угла
        LoRaSerial.println("ANGLE_SET:" + String(angle));
        Serial.println("Sent confirmation: ANGLE_SET:" + String(angle));
      } else {
        Serial.println("Invalid angle received: " + String(angle));
      }
    }
  }

  // Мониторинг температуры с интервалом 500 мс
  if (currentMillis - lastTempCheck >= tempCheckInterval) {
    lastTempCheck = currentMillis;

    float temperature = getTemperature();
    if (temperature != -127.0) {
      controlRelayByTemperature(temperature);
    }
  }
}

