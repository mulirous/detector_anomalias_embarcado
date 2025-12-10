// ============================================================
//   DETECTOR DE ANOMALIAS COM TINYML (TensorFlow Lite Micro)
// ============================================================
// Este código demonstra como:
// 1. Coletar dados do MPU6050
// 2. Calcular magnitude, FFT, peak_frequency e energy
// 3. Fazer inferência com Rede Neural via TensorFlow Lite Micro
// 4. Enviar JSON via MQTT com todos os dados
// ============================================================

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>

// ============================================================
// TENSORFLOW LITE MICRO
// ============================================================
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Modelo TFLite gerado pelo notebook eda.ipynb
#include "anomaly_model.h"

Adafruit_MPU6050 mpu;

// ---- Configuração Pinos do ESP32 para comunicação I2C ----
#define SDA_PIN 17
#define SCL_PIN 16

// ---- Definições da rede WiFi ----
const char* ssid = "brisa-1590757";
const char* password = "tlofkric";
WiFiClient espClient;

// ---- Definições do servidor MQTT ----
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_topic = "iot/murilo/mpu/anomalias";
PubSubClient client(espClient);

// ============================================================
// PARÂMETROS DO STANDARDSCALER
// (Extraídos do notebook eda.ipynb - mesmos usados no treinamento)
// ============================================================
const float MEAN_PEAK_FREQ = 20.04262575f;
const float MEAN_ENERGY = 8737.57146521f;
const float SCALE_PEAK_FREQ = 3.43938731f;
const float SCALE_ENERGY = 8547.02116f;

// Threshold de decisão
const float THRESHOLD = 0.5f;

// ============================================================
// CONFIGURAÇÃO TENSORFLOW LITE MICRO
// ============================================================

// Arena de memória para o TensorFlow Lite (ajuste se necessário)
constexpr int kTensorArenaSize = 8 * 1024;  // 8KB
uint8_t tensor_arena[kTensorArenaSize];

// Ponteiros globais do TFLite
const tflite::Model* tflite_model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input_tensor = nullptr;
TfLiteTensor* output_tensor = nullptr;

// Resolver de operações (inclui todas as ops)
tflite::AllOpsResolver resolver;

// Error Reporter para debug
tflite::MicroErrorReporter micro_error_reporter;
tflite::ErrorReporter* error_reporter = &micro_error_reporter;

// ============================================================
// CONFIGURAÇÃO FFT E BUFFERS
// ============================================================
const int WINDOW_SIZE = 100;  // 100 amostras = 1 segundo a 100Hz
const int FS = 100;           // Frequência de amostragem

// Buffers para armazenar as amostras de magnitude
float magnitude_buffer[WINDOW_SIZE];
int buffer_index = 0;
bool buffer_ready = false;

// Variáveis para armazenar última leitura
float last_ax = 0, last_ay = 0, last_az = 0;
float last_magnitude = 0;
float last_peak_frequency = 0;
float last_energy = 0;
float last_probability = 0;
bool last_is_anomaly = false;

// ---- Timers ----
unsigned long lastSample = 0;
const unsigned long sampleInterval = 10; // 10 ms -> 100 Hz

unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 5000;

// ============================================================
// FUNÇÕES AUXILIARES
// ============================================================

// Calcular magnitude da aceleração
float calcMagnitude(float ax, float ay, float az) {
    return sqrtf(ax * ax + ay * ay + az * az);
}

// ============================================================
// INICIALIZAÇÃO DO TENSORFLOW LITE MICRO
// ============================================================
bool setupTFLite() {
    Serial.println("Inicializando TensorFlow Lite Micro...");

    // 1. Carregar o modelo
    tflite_model = tflite::GetModel(anomaly_model);
    if (tflite_model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("Erro: Versão do modelo (%d) incompatível com schema (%d)!\n",
                      tflite_model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }
    Serial.println("  ✓ Modelo carregado");

    // 2. Criar o interpretador
    static tflite::MicroInterpreter static_interpreter(
        tflite_model, resolver, tensor_arena, (size_t)kTensorArenaSize, error_reporter
    );
    interpreter = &static_interpreter;
    Serial.println("  ✓ Interpretador criado");

    // 3. Alocar memória para tensores
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.println("Erro: Falha ao alocar tensores!");
        return false;
    }
    Serial.printf("  ✓ Tensores alocados (arena usada: ~%d bytes)\n", kTensorArenaSize);

    // 4. Obter ponteiros para entrada/saída
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);

    // 5. Verificar dimensões
    Serial.printf("  Input:  shape=[%d, %d], type=%d\n",
                  input_tensor->dims->data[0],
                  input_tensor->dims->data[1],
                  input_tensor->type);
    Serial.printf("  Output: shape=[%d, %d], type=%d\n",
                  output_tensor->dims->data[0],
                  output_tensor->dims->data[1],
                  output_tensor->type);

    Serial.println("TensorFlow Lite Micro inicializado com sucesso!\n");
    return true;
}

// ============================================================
// EXTRAÇÃO DE FEATURES (FFT usando DFT)
// ============================================================
void extractFFTFeatures(float* signal, int n, float* peak_frequency, float* energy) {
    // 1. Calcular média e remover componente DC
    float mean = 0;
    for (int i = 0; i < n; i++) {
        mean += signal[i];
    }
    mean /= n;

    // Buffer para sinal sem DC
    float detrended[WINDOW_SIZE];
    for (int i = 0; i < n; i++) {
        detrended[i] = signal[i] - mean;
    }

    // 2. Calcular DFT (simplificado - para produção use arduinoFFT)
    int n_freqs = n / 2 + 1;
    float max_magnitude = 0;
    int peak_idx = 0;
    *energy = 0;

    for (int k = 1; k < n_freqs; k++) {  // k=0 é DC, ignoramos
        float real_part = 0;
        float imag_part = 0;

        for (int i = 0; i < n; i++) {
            float angle = 2.0f * PI * k * i / n;
            real_part += detrended[i] * cosf(angle);
            imag_part -= detrended[i] * sinf(angle);
        }

        float mag = sqrtf(real_part * real_part + imag_part * imag_part);
        *energy += mag * mag;

        if (mag > max_magnitude) {
            max_magnitude = mag;
            peak_idx = k;
        }
    }

    // Converter índice para frequência em Hz
    *peak_frequency = (float)peak_idx * FS / n;
}

// ============================================================
// INFERÊNCIA COM TENSORFLOW LITE MICRO
// ============================================================
bool predictAnomalyTFLite(float peak_frequency, float energy, float* probability) {
    // 1. Normalizar as features (StandardScaler - mesmo do treinamento)
    float z_freq = (peak_frequency - MEAN_PEAK_FREQ) / SCALE_PEAK_FREQ;
    float z_energy = (energy - MEAN_ENERGY) / SCALE_ENERGY;

    // 2. Preencher o tensor de entrada
    input_tensor->data.f[0] = z_freq;
    input_tensor->data.f[1] = z_energy;

    // 3. Executar inferência
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        Serial.println("Erro na inferência TFLite!");
        *probability = -1.0f;
        return false;
    }

    // 4. Obter a probabilidade de anomalia (saída da sigmoid)
    *probability = output_tensor->data.f[0];

    // 5. Decisão baseada no threshold
    return *probability > THRESHOLD;
}

// ============================================================
// PROCESSAR JANELA E FAZER PREDIÇÃO
// ============================================================
void processWindow() {
    // Extrair features FFT
    extractFFTFeatures(magnitude_buffer, WINDOW_SIZE, &last_peak_frequency, &last_energy);

    // Fazer predição com TensorFlow Lite
    last_is_anomaly = predictAnomalyTFLite(last_peak_frequency, last_energy, &last_probability);
}

// ============================================================
// ENVIAR DADOS VIA MQTT (JSON)
// ============================================================
void publishData() {
    if (!client.connected()) return;

    // Montar JSON com todos os dados
    char json_buffer[400];
    snprintf(json_buffer, sizeof(json_buffer),
             "{"
             "\"model\":\"TinyML-MLP\","
             "\"ax\":%.4f,"
             "\"ay\":%.4f,"
             "\"az\":%.4f,"
             "\"magnitude\":%.4f,"
             "\"peak_frequency\":%.2f,"
             "\"energy\":%.2f,"
             "\"probability\":%.4f,"
             "\"is_anomaly\":%s"
             "}",
             last_ax,
             last_ay,
             last_az,
             last_magnitude,
             last_peak_frequency,
             last_energy,
             last_probability,
             last_is_anomaly ? "true" : "false");

    // Publicar no tópico MQTT
    client.publish(mqtt_topic, json_buffer);

    // Debug no Serial
    Serial.println("--- Dados Publicados (TinyML) ---");
    Serial.println(json_buffer);
    if (last_is_anomaly) {
        Serial.println("⚠️  ANOMALIA DETECTADA!");
    }
    Serial.println();
}

// ============================================================
// FUNÇÕES DE CONEXÃO
// ============================================================
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
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

bool reconnectMQTT() {
    if (client.connected()) return true;

    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt < mqttReconnectInterval) return false;

    lastMqttReconnectAttempt = now;
    Serial.print("Tentando conexão MQTT... ");

    String clientId = "ESP32TinyML-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
        Serial.println("conectado!");
        return true;
    } else {
        Serial.print("falhou, rc=");
        Serial.println(client.state());
        return false;
    }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);

    Serial.println("==========================================");
    Serial.println("  DETECTOR DE ANOMALIAS - TinyML (TFLite)");
    Serial.println("==========================================");

    // Conectar WiFi
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);

    // Inicializar TensorFlow Lite Micro
    if (!setupTFLite()) {
        Serial.println("ERRO FATAL: Falha ao inicializar TFLite!");
        while (1) delay(1000);
    }

    // Inicializar MPU6050
    if (!mpu.begin()) {
        Serial.println("Falha ao conectar o MPU6050!");
        while (1) delay(10);
    }
    Serial.println("MPU6050 conectado!");

    // Configurar MPU6050
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_184_HZ);

    Serial.println("Coletando dados...\n");
    delay(100);
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {
    unsigned long now = millis();

    // Manter conexão MQTT
    if (!client.connected()) {
        reconnectMQTT();
    } else {
        client.loop();
    }

    // Coletar amostra a 100Hz
    if (now - lastSample >= sampleInterval) {
        lastSample = now;

        // Ler sensor
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        // Armazenar últimos valores de aceleração
        last_ax = a.acceleration.x;
        last_ay = a.acceleration.y;
        last_az = a.acceleration.z;

        // Calcular magnitude
        last_magnitude = calcMagnitude(last_ax, last_ay, last_az);

        // Adicionar ao buffer
        magnitude_buffer[buffer_index] = last_magnitude;
        buffer_index++;

        // Quando o buffer estiver cheio (1 segundo de dados)
        if (buffer_index >= WINDOW_SIZE) {
            buffer_index = 0;
            buffer_ready = true;

            // Processar janela e fazer predição com TinyML
            processWindow();

            // Publicar dados via MQTT
            publishData();
        }
    }
}
