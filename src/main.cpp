//                      Projeto Detecção de Anomalias com MPU6050

// Bibliotecas necessárias para o funcionamento do MPU6050
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
Adafruit_MPU6050 mpu;

// ---- Configuração Pinos do ESP32 para comunicação I2C com o MPU6050 ----
#define SDA_PIN 17
#define SCL_PIN 16

// Bibliotecas necessárias para a comunicação dos dados via MQTT
#include <WiFi.h>
#include <PubSubClient.h>

//  ---- Definições da rede WiFi ----
const char* ssid = "brisa-1590757";         // Nome da rede WiFi
const char* password = "tlofkric";          // Senha da rede WiFi
WiFiClient espClient;

// ---- Definições do servidor MQTT ----
const char* mqtt_server = "test.mosquitto.org";       // Endereço do servidor MQTT público
const int   mqtt_port   = 1883;                       // Porta padrão do MQTT
const char* mqtt_topic  = "iot/murilo/mpu/movimento"; // Tópico para publicar os dados de movimento
PubSubClient client(espClient);

// ---- Timers ----
unsigned long lastSample          = 0;      // controle de amostragem do sensor
const unsigned long sampleInterval = 10;    // 10 ms -> ~100 Hz (pode aumentar se quiser aliviar)

// Timer para reconexão MQTT não bloqueante
unsigned long lastMqttReconnectAttempt  = 0;
const unsigned long mqttReconnectInterval = 5000;  // tenta reconectar a cada 5 s

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando em ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // BLOQUEIA APENAS NO INÍCIO, É ACEITÁVEL NO SETUP
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ---- Função de reconexão MQTT NÃO BLOQUEANTE ----
bool reconnectMQTT() {
  if (client.connected()) {
    return true;
  }

  unsigned long now = millis();
  // Só tenta reconectar de tempos em tempos para não travar
  if (now - lastMqttReconnectAttempt < mqttReconnectInterval) {
    return false;
  }

  lastMqttReconnectAttempt = now;

  Serial.print("Tentando conexão MQTT... ");
  String clientId = "ESP32Client-";
  clientId += String(random(0xffff), HEX);

  if (client.connect(clientId.c_str())) {
    Serial.println("conectado!");
    return true;
  } else {
    Serial.print("falhou, rc=");
    Serial.println(client.state());
    // Não tem delay aqui, só espera o próximo ciclo de 5 s pelo millis()
    return false;
  }
}

void setup(void) {
  Serial.begin(115200); // iniciar o monitor serial
  Wire.begin(SDA_PIN, SCL_PIN); // Wire begin (SDA,SCL) pinos do ESP32

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Verifica se o módulo GY-521 (MPU6050) está conectado
  if (!mpu.begin()) {
    Serial.println("Falha ao conectar o módulo");
    while (1) {
      delay(10); // aqui pode bloquear, é erro crítico de hardware
    }
  }
  Serial.println("Módulo conectado");

  // Definição da variação do chip.
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Variação do acelerômetro para: ");
  switch (mpu.getAccelerometerRange()) {
    case MPU6050_RANGE_2_G:  Serial.println("+-2G");  break;
    case MPU6050_RANGE_4_G:  Serial.println("+-4G");  break;
    case MPU6050_RANGE_8_G:  Serial.println("+-8G");  break;
    case MPU6050_RANGE_16_G: Serial.println("+-16G"); break;
  }

  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Variação do giroscópio para: ");
  switch (mpu.getGyroRange()) {
    case MPU6050_RANGE_250_DEG:  Serial.println("+- 250 deg/s");  break;
    case MPU6050_RANGE_500_DEG:  Serial.println("+- 500 deg/s");  break;
    case MPU6050_RANGE_1000_DEG: Serial.println("+- 1000 deg/s"); break;
    case MPU6050_RANGE_2000_DEG: Serial.println("+- 2000 deg/s"); break;
  }

  mpu.setFilterBandwidth(MPU6050_BAND_184_HZ);
  Serial.print("Filtro: ");
  switch (mpu.getFilterBandwidth()) {
    case MPU6050_BAND_260_HZ: Serial.println("260 Hz"); break;
    case MPU6050_BAND_184_HZ: Serial.println("184 Hz"); break;
    case MPU6050_BAND_94_HZ:  Serial.println("94 Hz");  break;
    case MPU6050_BAND_44_HZ:  Serial.println("44 Hz");  break;
    case MPU6050_BAND_21_HZ:  Serial.println("21 Hz");  break;
    case MPU6050_BAND_10_HZ:  Serial.println("10 Hz");  break;
    case MPU6050_BAND_5_HZ:   Serial.println("5 Hz");   break;
  }

  Serial.println("");
  delay(100);
}

void loop() {
  unsigned long now = millis();

  // 1) Garantir conexão WiFi (aqui só checamos; se cair, você pode recomeçar o WiFi.begin se quiser)
  if (WiFi.status() != WL_CONNECTED) {
    // Opcional: tentar reconectar WiFi de forma não bloqueante também
    // Exemplo simples (não obrigatório):
    // WiFi.disconnect();
    // WiFi.begin(ssid, password);
  }

  // 2) Garantir conexão MQTT de forma NÃO bloqueante
  if (!client.connected()) {
    reconnectMQTT();   // Tenta reconectar de tempos em tempos, sem while + delay
  } else {
    client.loop();     // Processa keep-alive e callbacks normalmente
  }

  // 3) Leitura do sensor com tempo fixo (100 Hz)
  if (now - lastSample >= sampleInterval) {
    lastSample = now;

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    char msg[200];
    snprintf(msg, sizeof(msg),
             "{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"temp\":%.2f}",
             a.acceleration.x,
             a.acceleration.y,
             a.acceleration.z,
             temp.temperature);

    Serial.println(msg);

    // Só tenta publicar se estiver conectado
    if (client.connected()) {
      client.publish(mqtt_topic, msg);
    }
  }
}
