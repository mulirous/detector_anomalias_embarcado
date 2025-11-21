//                      Projeto Detecção de Anomalias com MPU6050

// Bibliotecas necessárias para o funcionamento do MPU6050
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
Adafruit_MPU6050 mpu;

// ---- Configuração Pinos do ESP32 para comunicação I2C com o MPU6050 ----
#define SDA_PIN 17
#define SCL_PIN 16

//Variaveis para receber os valores de aceleração dos eixos
int valoranteriorX = 0 ;
int valoranteriorY = 0 ;
int valoranteriorZ = 0 ;

// Bibliotecas necessárias para a comunicação dos dados via MQTT
#include <WiFi.h>
#include <PubSubClient.h>

//  ---- Definições da rede WiFi ----
const char* ssid = "brisa-1590757";         // Nome da rede WiFi
const char* password = "tlofkric";     // Senha da rede WiFi
WiFiClient espClient;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando em ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
}

// ---- Definições do servidor MQTT ----
const char* mqtt_server = "test.mosquitto.org"; // Endereço do servidor MQTT público
const int mqtt_port = 1883;                     // Porta padrão do MQTT
const char* mqtt_topic = "iot/murilo/mpu/movimento"; // Tópico para publicar os dados de movimento
PubSubClient client(espClient);

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("conectado!");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

// ---- Definição para o timer de leitura dos dados do MPU6050 ----
unsigned long lastSample = 0; 
const unsigned long sampleInterval = 10; // Intervalo de amostragem em milissegundos (100 vezes por segundo, ou 100Hz)

void setup(void) {
  Serial.begin(115200); // iniciar o monitor serial 
  Wire.begin(SDA_PIN, SCL_PIN); // Wire begin (SDA,SCL) pinos do ESP32

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

 //Verifica se o módulo GY-521 (MPU6050) está conectado 
  if (!mpu.begin()) {
    Serial.println("Falha ao conectar o módulo");// caso não encontre
    while (1) {
      delay(10);
    }
  }
  Serial.println("Módulo conectado"); // caso encontre
// Definição da variação do chip.
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Variação do aceleremetro para: ");
  switch (mpu.getAccelerometerRange()) {
  case MPU6050_RANGE_2_G:
    Serial.println("+-2G");
    break;
  case MPU6050_RANGE_4_G:
    Serial.println("+-4G");
    break;
  case MPU6050_RANGE_8_G:
    Serial.println("+-8G");
    break;
  case MPU6050_RANGE_16_G:
    Serial.println("+-16G");
    break;
  }
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Variação do Giroscópio para: ");
  switch (mpu.getGyroRange()) {
  case MPU6050_RANGE_250_DEG:
    Serial.println("+- 250 deg/s");
    break;
  case MPU6050_RANGE_500_DEG:
    Serial.println("+- 500 deg/s");
    break;
  case MPU6050_RANGE_1000_DEG:
    Serial.println("+- 1000 deg/s");
    break;
  case MPU6050_RANGE_2000_DEG:
    Serial.println("+- 2000 deg/s");
    break;
  }

  mpu.setFilterBandwidth(MPU6050_BAND_184_HZ);
  Serial.print("Filtro: ");
  switch (mpu.getFilterBandwidth()) {
  case MPU6050_BAND_260_HZ:
    Serial.println("260 Hz");
    break;
  case MPU6050_BAND_184_HZ:
    Serial.println("184 Hz");
    break;
  case MPU6050_BAND_94_HZ:
    Serial.println("94 Hz");
    break;
  case MPU6050_BAND_44_HZ:
    Serial.println("44 Hz");
    break;
  case MPU6050_BAND_21_HZ:
    Serial.println("21 Hz");
    break;
  case MPU6050_BAND_10_HZ:
    Serial.println("10 Hz");
    break;
  case MPU6050_BAND_5_HZ:
    Serial.println("5 Hz");
    break;
  }

  Serial.println("");
  delay(100);
}

void loop() {

  // Iniciando conexão com o servidor MQTT e Wifi
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastSample >= sampleInterval) {
    lastSample = now;

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    char msg[200];
    snprintf(msg, 200, "{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"temp\":%.2f}",
             a.acceleration.x,
             a.acceleration.y,
             a.acceleration.z,
             temp.temperature);

    Serial.print(msg);
    Serial.println();

    client.publish(mqtt_topic, msg);
  }
}