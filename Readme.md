# 🔍 Detecção de Anomalias em Motores com ESP32, MPU6050 e TinyML

Sistema completo de **Machine Learning Embarcado (TinyML)** para detecção de anomalias em motores de ventiladores em tempo real. O projeto utiliza um ESP32 com acelerômetro/giroscópio MPU6050 colado ao motor para monitorar vibrações e instabilidades (tremores e irregularidades), processando os dados localmente com modelos de aprendizado supervisionado e enviando diagnósticos via MQTT.

## 🎯 Objetivo do Projeto

O objetivo principal é fazer o microcontrolador ESP32 nos informar **se a instabilidade medida no motor é uma anomalia ou não**. Um motor irregular tende a tremer e sair de harmonia, gerando padrões de vibração distintos do funcionamento normal.

Para isso, foram desenvolvidas **duas abordagens de Machine Learning**:

1. **Regressão Logística** - Modelo estatístico clássico, leve e interpretável
2. **Rede Neural MLP (Multi-Layer Perceptron)** - Modelo de Deep Learning via TensorFlow Lite Micro (TinyML)

---

## 📋 Visão Geral do Sistema

O sistema opera em **duas fases distintas**:

### Fase 1: Coleta e Treinamento (Offline)

```
[ESP32 + MPU6050] ──MQTT──▶ [data_consumer.py] ──▶ [mpu_data.csv] ──▶ [eda.ipynb] ──▶ Modelos Treinados
      │                                                                    │
      │                                                                    ├──▶ Regressão Logística (coeficientes)
      └── Sensor colado ao motor                                           └──▶ Rede Neural MLP (anomaly_model.h)
          do ventilador
```

### Fase 2: Inferência em Tempo Real (Online)

```
[ESP32 + MPU6050] ──▶ [FFT + Extração de Features] ──▶ [Modelo Embarcado] ──▶ [Predição] ──MQTT──▶ [Monitoramento]
                                                              │
                                                              ├── predictor.cpp (Regressão Logística)
                                                              └── predictor_tinyML.cpp (Rede Neural MLP)
```

---

## 📂 Estrutura do Projeto

```text
detector_anomalias_embarcado/
├── src/
│   └── main.cpp                    # Firmware de produção ativo
├── test/
│   ├── data_producer.cpp           # Firmware para coleta de dados de treinamento
│   ├── predictor.cpp               # Inferência com Regressão Logística
│   └── predictor_tinyML.cpp        # Inferência com TensorFlow Lite Micro (MLP)
├── include/
│   └── anomaly_model.h             # Modelo TFLite exportado como array C++
├── data_tools/
│   ├── data_consumer.py            # Cliente MQTT que coleta e salva dados em CSV
│   ├── eda.ipynb                   # Notebook completo: EDA, treinamento e exportação
│   ├── data/
│   │   ├── mpu_data.csv            # Dataset principal coletado
│   │   ├── mpu_data_zero.csv       # Dados de calibração (motor parado)
│   │   └── mpu_data_uno.csv        # Dados adicionais de treinamento
│   └── models/
│       ├── anomaly_detector_float32.tflite   # Modelo TFLite (Float32)
│       └── anomaly_detector_int8.tflite      # Modelo TFLite (Quantizado INT8)
├── lib/                            # Bibliotecas customizadas
├── platformio.ini                  # Configuração do PlatformIO
└── Readme.md                       # Esta documentação
```

---

## 🛠️ Tecnologias Utilizadas

| Camada               | Tecnologia                                                         |
| -------------------- | ------------------------------------------------------------------ |
| **Hardware**         | ESP32 DevKit, MPU6050 (GY-521)                                     |
| **Firmware**         | C++ (Arduino/PlatformIO), Adafruit MPU6050, PubSubClient           |
| **Comunicação**      | MQTT (test.mosquitto.org:1883)                                     |
| **Data Science**     | Python, Pandas, NumPy, Matplotlib, SciPy, Scikit-learn             |
| **Machine Learning** | Regressão Logística (sklearn) + Rede Neural MLP (TensorFlow/Keras) |
| **TinyML**           | TensorFlow Lite Micro (TensorFlowLite_ESP32)                       |

---

## ⚙️ Pipeline Completo de Desenvolvimento

---

### 📡 Etapa 1: Coleta de Dados Brutos (`test/data_producer.cpp`)

O firmware `data_producer.cpp` é utilizado na **fase de treinamento** para coletar dados brutos do acelerômetro.

**Como funciona:**

1. Inicializa o MPU6050 via I2C (pinos SDA=17, SCL=16)
2. Conecta ao WiFi e ao broker MQTT (`test.mosquitto.org`)
3. Lê o sensor a **100 Hz** (1 amostra a cada 10ms)
4. Publica cada leitura em JSON no tópico: `iot/murilo/mpu/movimento`

**Formato do JSON publicado:**

```json
{ "ax": 0.12, "ay": 9.81, "az": 0.05, "temp": 25.3 }
```

Onde:

- `ax`, `ay`, `az`: Aceleração nos 3 eixos (m/s²)
- `temp`: Temperatura do sensor (°C)

**Para usar:**

```bash
# Mova data_producer.cpp para src/main.cpp temporariamente
cp test/data_producer.cpp src/main.cpp
pio run --target upload
```

---

### 💾 Etapa 2: Consumo e Armazenamento (`data_tools/data_consumer.py`)

O script Python atua como **subscriber MQTT** para coletar os dados enviados pelo ESP32.

**Como funciona:**

1. Conecta ao broker `test.mosquitto.org:1883`
2. Assina o tópico `iot/murilo/mpu/movimento`
3. Cada mensagem JSON recebida é parseada e armazenada em memória
4. Ao pressionar `CTRL+C`, salva todos os dados em `data/mpu_data.csv`

**Execução:**

```bash
cd data_tools
pip install paho-mqtt pandas
python data_consumer.py
# Deixe coletando dados por alguns minutos com o motor em diferentes estados
# Pressione CTRL+C para salvar o CSV
```

**Dica:** Colete dados em diferentes condições:

- Motor funcionando normalmente
- Motor com carga anormal
- Motor desbalanceado/com problemas

---

### 📊 Etapa 3: Análise Exploratória e Treinamento (`data_tools/eda.ipynb`)

O notebook Jupyter realiza **todo o pipeline de Data Science** desde a exploração até a exportação dos modelos.

---

#### 3.1 Exploração de Dados (EDA)

- **Visualização das séries temporais** de aceleração (X, Y, Z)
- **Gráficos de distribuição**: Histogramas e boxplots para identificar outliers
- **Análise visual** para entender o comportamento do motor em diferentes estados

---

#### 3.2 Engenharia de Features

O sinal bruto do acelerômetro não é útil diretamente. É necessário extrair **features significativas**.

**Cálculo da Magnitude:**

A magnitude combina os 3 eixos em um único valor, representando a intensidade total da aceleração:

$$\text{magnitude} = \sqrt{ax^2 + ay^2 + az^2}$$

**Janelamento:**

Os dados são divididos em **janelas de 1 segundo** (100 amostras, pois a coleta é a 100Hz). Cada janela representa um "snapshot" do estado do motor.

**Transformada Rápida de Fourier (FFT):**

A FFT converte o sinal do **domínio do tempo** para o **domínio da frequência**, revelando quais frequências de vibração estão presentes. É implementada via função `extract_fft_features()`:

1. **Remove o componente DC** (média do sinal)
2. **Calcula a DFT** (Discrete Fourier Transform) para cada frequência
3. **Extrai as features:**
   - `peak_frequency`: Frequência com maior amplitude (Hz) - indica a frequência dominante de vibração
   - `energy`: Soma dos quadrados das amplitudes de todas as frequências (energia espectral total)

**Por que usar FFT?**

- Vibrações anormais geralmente aparecem em frequências específicas
- A energia total indica a intensidade geral das vibrações
- Essas features são mais robustas que valores brutos de aceleração

---

#### 3.3 Rotulagem Estatística com Z-Score

Como não temos rótulos manuais de "normal" ou "anômalo", usamos um **método estatístico baseado em Z-Score** para criar as labels automaticamente.

**O que é Z-Score?**

O Z-Score mede quantos desvios padrão um valor está distante da média:

$$z = \frac{x - \mu}{\sigma}$$

Onde $\mu$ é a média e $\sigma$ é o desvio padrão.

**Cálculo do Score de Anomalia:**

Para cada janela, calculamos:

$$z_{freq} = \frac{f_{peak} - \mu_{freq}}{\sigma_{freq}}$$

$$z_{energy} = \frac{E - \mu_{energy}}{\sigma_{energy}}$$

Os dois z-scores são combinados em um **score único** usando a norma euclidiana:

$$S = \sqrt{z_{freq}^2 + z_{energy}^2}$$

**Regra de Decisão:**

$$
\text{isAnomaly} = \begin{cases}
\text{True} & \text{se } S > T \\
\text{False} & \text{se } S \leq T
\end{cases}
$$

Onde $T = 3$ é o limiar (threshold) escolhido, representando 3 desvios padrão da normalidade.

---

#### 3.4 Modelo 1: Regressão Logística

A Regressão Logística é um modelo de classificação binária que calcula a **probabilidade** de uma amostra pertencer à classe positiva (anomalia).

**Como funciona:**

1. **Combinação Linear das Features:**
   $$z = w_1 \cdot x_1 + w_2 \cdot x_2 + b$$

2. **Função Sigmoide (Logística):**
   $$p = \sigma(z) = \frac{1}{1 + e^{-z}}$$

A sigmoide "espreme" qualquer valor real para o intervalo [0, 1], interpretado como probabilidade.

**Pipeline de Treinamento:**

```python
logreg_pipeline = Pipeline([
    ("scaler", StandardScaler()),    # Normaliza features para média=0, std=1
    ("model", LogisticRegression(
        class_weight='balanced',      # Compensa desbalanceamento de classes
        random_state=42,
        max_iter=1000
    ))
])
```

**Resultados Obtidos:**

```
              precision    recall  f1-score   support
      Normal       0.99      0.99      0.99       849
     Anômalo       0.79      0.71      0.75        31
    accuracy                           0.98       880
```

**Matriz de Confusão:**

|                | Pred: Normal | Pred: Anomalia |
| -------------- | ------------ | -------------- |
| Real: Normal   | 843          | 6              |
| Real: Anomalia | 9            | 22             |

**Parâmetros Exportados para o ESP32:**

```cpp
// StandardScaler
const float MEAN_PEAK_FREQ = 20.04262575f;
const float MEAN_ENERGY = 8737.57146521f;
const float SCALE_PEAK_FREQ = 3.43938731f;
const float SCALE_ENERGY = 8547.02116f;

// Regressão Logística
const float W_PEAK_FREQ = 0.35955423f;
const float W_ENERGY = 0.96707036f;
const float INTERCEPT = -0.75556951f;
```

---

#### 3.5 Modelo 2: Rede Neural MLP (TinyML)

**O que é uma MLP (Multi-Layer Perceptron)?**

Uma MLP é um tipo de **Rede Neural Artificial** composta por múltiplas camadas de neurônios conectados. Diferente da Regressão Logística (um único neurônio), a MLP pode aprender **padrões não-lineares** complexos.

**Arquitetura da Rede:**

```
Entrada (2 features) → Camada Oculta 1 (8 neurônios, ReLU) → Camada Oculta 2 (4 neurônios, ReLU) → Saída (1 neurônio, Sigmoid)
```

| Camada    | Neurônios | Ativação | Parâmetros |
| --------- | --------- | -------- | ---------- |
| hidden1   | 8         | ReLU     | 24         |
| hidden2   | 4         | ReLU     | 36         |
| output    | 1         | Sigmoid  | 5          |
| **Total** |           |          | **65**     |

**O que são as funções de ativação?**

- **ReLU (Rectified Linear Unit):** $f(x) = \max(0, x)$ - Introduz não-linearidade, permitindo aprender padrões complexos
- **Sigmoid:** $f(x) = \frac{1}{1+e^{-x}}$ - Converte a saída em probabilidade [0, 1]

**Treinamento:**

```python
model.compile(
    optimizer=Adam(learning_rate=0.001),   # Otimizador adaptativo
    loss='binary_crossentropy',             # Função de perda para classificação binária
    metrics=['accuracy', AUC()]             # Métricas de avaliação
)
```

- **EarlyStopping**: Interrompe o treinamento quando a performance no conjunto de validação para de melhorar
- **Class Weights**: Compensa o desbalanceamento entre classes normais (maioria) e anomalias (minoria)

**Resultados Obtidos:**

```
              precision    recall  f1-score   support
      Normal       1.00      1.00      1.00       849
    Anomalia       0.97      1.00      0.98        31
    accuracy                           1.00       880
```

| Métrica  | Valor  |
| -------- | ------ |
| Loss     | 0.0030 |
| Accuracy | 0.9989 |
| AUC      | 1.0000 |

**Por que a MLP teve melhor desempenho?**

A Rede Neural pode aprender **fronteiras de decisão não-lineares**, capturando relações mais complexas entre peak_frequency e energy que a Regressão Logística (linear) não consegue.

---

#### 3.6 Conversão para TensorFlow Lite

Para executar a rede neural no ESP32, é necessário converter o modelo Keras para o formato **TensorFlow Lite**, otimizado para microcontroladores.

**Processo de Conversão:**

```python
# Converter para TFLite
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

# Salvar modelo Float32
with open("anomaly_detector_float32.tflite", "wb") as f:
    f.write(tflite_model)
```

**Modelos Gerados:**

| Arquivo                           | Tamanho | Descrição                         |
| --------------------------------- | ------- | --------------------------------- |
| `anomaly_detector_float32.tflite` | 2.3 KB  | Pesos em float32 (maior precisão) |
| `anomaly_detector_int8.tflite`    | 2.7 KB  | Pesos quantizados INT8 (menor)    |

**Validação Pós-Conversão:**

```
Modelo Keras Original: 0.9989
Modelo TFLite Float32: 0.9989
Diferença: 0.0000
```

A conversão não perdeu precisão!

---

#### 3.7 Exportação para C++ (Header File)

O modelo TFLite é convertido em um **array de bytes C++** para ser incluído diretamente no firmware:

```python
# Exportar como header C++
with open("anomaly_model.h", "w") as f:
    f.write("// Auto-generated TensorFlow Lite model\n")
    f.write(f"const unsigned int anomaly_model_len = {len(tflite_model)};\n")
    f.write("alignas(8) const unsigned char anomaly_model[] = {\n")
    for i, byte in enumerate(tflite_model):
        f.write(f"0x{byte:02x}, ")
        if (i + 1) % 12 == 0:
            f.write("\n")
    f.write("\n};\n")
```

**Resultado (`include/anomaly_model.h`):**

```cpp
// Tamanho do modelo em bytes
const unsigned int anomaly_model_len = 2304;

// Dados do modelo TensorFlow Lite (65 parâmetros + metadados)
alignas(8) const unsigned char anomaly_model[] = {
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33, ...
};
```

---

### 🤖 Etapa 4: Inferência com Regressão Logística (`test/predictor.cpp`)

O firmware `predictor.cpp` implementa o **modelo de Regressão Logística diretamente no ESP32**.

#### Fluxo de Processamento:

```
┌─────────────────────────────────────────────────────────────────────┐
│  LOOP PRINCIPAL (100Hz)                                             │
├─────────────────────────────────────────────────────────────────────┤
│  1. Ler MPU6050 (ax, ay, az)                                        │
│  2. Calcular magnitude = √(ax² + ay² + az²)                         │
│  3. Armazenar no buffer circular [100 amostras]                     │
│  4. Quando buffer cheio (1 segundo de dados):                       │
│     └── processWindow()                                             │
│         ├── extractFFTFeatures() → peak_frequency, energy           │
│         ├── predictAnomaly() → probabilidade, is_anomaly            │
│         └── publishData() → JSON via MQTT                           │
└─────────────────────────────────────────────────────────────────────┘
```

#### Funções Principais:

**`extractFFTFeatures(float* signal, int n, float* peak_frequency, float* energy)`**

Implementa a DFT (Discrete Fourier Transform) manualmente:

1. Remove o componente DC (média)
2. Calcula as partes real e imaginária para cada frequência k
3. Encontra a frequência com maior magnitude (peak)
4. Soma a energia total

**`predictAnomaly(float peak_frequency, float energy, float* probability)`**

Aplica o modelo de Regressão Logística:

```cpp
// 1. Normalizar (StandardScaler)
float z_freq = (peak_frequency - MEAN_PEAK_FREQ) / SCALE_PEAK_FREQ;
float z_energy = (energy - MEAN_ENERGY) / SCALE_ENERGY;

// 2. Combinação linear
float z = W_PEAK_FREQ * z_freq + W_ENERGY * z_energy + INTERCEPT;

// 3. Sigmoid
*probability = 1.0f / (1.0f + expf(-z));

// 4. Decisão
return *probability > THRESHOLD;  // THRESHOLD = 0.5
```

**`publishData()`**

Monta e publica JSON completo via MQTT:

```json
{
  "ax": 0.12,
  "ay": 9.81,
  "az": 0.05,
  "magnitude": 9.82,
  "peak_frequency": 20.0,
  "energy": 8500.0,
  "probability": 0.32,
  "is_anomaly": false
}
```

---

### 🧠 Etapa 5: Inferência com TinyML (`test/predictor_tinyML.cpp`)

O firmware `predictor_tinyML.cpp` implementa a **Rede Neural MLP usando TensorFlow Lite Micro**.

#### O que é TensorFlow Lite Micro?

É uma versão otimizada do TensorFlow para **microcontroladores** com recursos limitados (memória, processamento). Permite executar modelos de Deep Learning em dispositivos como o ESP32.

#### Arquitetura do Código:

```
┌─────────────────────────────────────────────────────────────────────┐
│  SETUP                                                              │
├─────────────────────────────────────────────────────────────────────┤
│  1. Conectar WiFi e MQTT                                            │
│  2. setupTFLite()  ← Inicializa o interpretador TensorFlow Lite     │
│  3. Inicializar MPU6050                                             │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  LOOP (100Hz) - Mesmo do predictor.cpp                              │
├─────────────────────────────────────────────────────────────────────┤
│  Quando buffer cheio:                                               │
│  └── processWindow()                                                │
│      ├── extractFFTFeatures() → peak_frequency, energy              │
│      ├── predictAnomalyTFLite() ← USA REDE NEURAL                   │
│      └── publishData()                                              │
└─────────────────────────────────────────────────────────────────────┘
```

#### Função `setupTFLite()` - Inicialização do TensorFlow Lite Micro:

```cpp
bool setupTFLite() {
    // 1. Carregar o modelo do array de bytes
    tflite_model = tflite::GetModel(anomaly_model);

    // 2. Verificar compatibilidade de versão
    if (tflite_model->version() != TFLITE_SCHEMA_VERSION) {
        return false;  // Modelo incompatível
    }

    // 3. Criar o interpretador com alocação de memória
    static tflite::MicroInterpreter static_interpreter(
        tflite_model,           // Modelo carregado
        resolver,               // Operações suportadas (AllOpsResolver)
        tensor_arena,           // Buffer de memória (8KB)
        kTensorArenaSize,       // Tamanho do buffer
        error_reporter          // Para debug de erros
    );
    interpreter = &static_interpreter;

    // 4. Alocar memória para os tensores
    interpreter->AllocateTensors();

    // 5. Obter ponteiros para entrada e saída
    input_tensor = interpreter->input(0);   // Shape: [1, 2]
    output_tensor = interpreter->output(0); // Shape: [1, 1]

    return true;
}
```

**Componentes do TFLite Micro:**

| Componente         | Descrição                                               |
| ------------------ | ------------------------------------------------------- |
| `GetModel()`       | Deserializa o modelo do array de bytes                  |
| `AllOpsResolver`   | Registra todas as operações (Dense, ReLU, etc.)         |
| `tensor_arena`     | Buffer estático onde os tensores são alocados (8KB)     |
| `MicroInterpreter` | Executa o modelo - faz forward pass através das camadas |
| `input_tensor`     | Ponteiro para alimentar os dados de entrada             |
| `output_tensor`    | Ponteiro para ler a saída (probabilidade)               |

#### Função `predictAnomalyTFLite()` - Inferência com a Rede Neural:

```cpp
bool predictAnomalyTFLite(float peak_frequency, float energy, float* probability) {
    // 1. Normalizar as features (mesmo StandardScaler do treinamento)
    float z_freq = (peak_frequency - MEAN_PEAK_FREQ) / SCALE_PEAK_FREQ;
    float z_energy = (energy - MEAN_ENERGY) / SCALE_ENERGY;

    // 2. Preencher o tensor de entrada
    input_tensor->data.f[0] = z_freq;
    input_tensor->data.f[1] = z_energy;

    // 3. Executar inferência (forward pass pela rede neural)
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        return false;  // Erro na inferência
    }

    // 4. Ler a probabilidade da saída (já passou pela sigmoid)
    *probability = output_tensor->data.f[0];

    // 5. Decisão baseada no threshold
    return *probability > THRESHOLD;
}
```

**O que acontece no `interpreter->Invoke()`?**

1. Os dados de entrada são propagados pela **Camada 1** (8 neurônios + ReLU)
2. O resultado passa pela **Camada 2** (4 neurônios + ReLU)
3. Por fim, pela **Camada de Saída** (1 neurônio + Sigmoid)
4. A saída é a probabilidade de ser anomalia [0, 1]

#### JSON de Saída (TinyML):

```json
{
  "model": "TinyML-MLP",
  "ax": 0.12,
  "ay": 9.81,
  "az": 0.05,
  "magnitude": 9.82,
  "peak_frequency": 20.0,
  "energy": 8500.0,
  "probability": 0.98,
  "is_anomaly": true
}
```

---

## 🔄 Comparação: Regressão Logística vs Rede Neural MLP

| Característica          | Regressão Logística    | Rede Neural MLP          |
| ----------------------- | ---------------------- | ------------------------ |
| **Arquivo**             | `predictor.cpp`        | `predictor_tinyML.cpp`   |
| **Tamanho do código**   | ~11 KB                 | ~13 KB                   |
| **Dependência externa** | Nenhuma                | TensorFlowLite_ESP32     |
| **Modelo**              | 7 parâmetros hardcoded | 65 parâmetros (2.3KB .h) |
| **RAM adicional**       | ~400 bytes             | ~8KB (tensor_arena)      |
| **Flash adicional**     | ~0                     | ~77.5% do ESP32          |
| **Precisão (Anomalia)** | 79%                    | 97%                      |
| **Recall (Anomalia)**   | 71%                    | 100%                     |
| **F1-Score (Anomalia)** | 75%                    | 98%                      |
| **Padrões aprendidos**  | Lineares               | Não-lineares             |
| **Generalização**       | Limitada               | Melhor                   |

**Quando usar cada um?**

- **Regressão Logística**: Quando memória é crítica, modelo simples é suficiente, ou para baseline
- **Rede Neural MLP**: Quando precisa de maior precisão, padrões são complexos, ou tem memória disponível

---

## 🧮 Fundamentos Matemáticos

### Transformada de Fourier (FFT)

A FFT transforma o sinal do domínio do tempo para o domínio da frequência:

$$X_k = \sum_{n=0}^{N-1} x_n \cdot e^{-i \cdot 2\pi \cdot k \cdot n / N}$$

Onde:

- $x_n$ são as amostras no tempo
- $X_k$ são os coeficientes de frequência
- $k$ é o índice de frequência: $f_k = k \cdot \frac{f_s}{N}$ Hz

### Modelo de Regressão Logística

1. **Normalização Z-Score (StandardScaler):**
   $$z_{freq} = \frac{f - \mu_f}{\sigma_f}, \quad z_{energy} = \frac{E - \mu_E}{\sigma_E}$$

2. **Combinação Linear:**
   $$z = w_{freq} \cdot z_{freq} + w_{energy} \cdot z_{energy} + b$$

3. **Função Sigmoide:**
   $$p = \sigma(z) = \frac{1}{1 + e^{-z}}$$

4. **Decisão:**
   $$\hat{y} = \begin{cases} 1 \text{ (Anomalia)} & \text{se } p > 0.5 \\ 0 \text{ (Normal)} & \text{se } p \leq 0.5 \end{cases}$$

### Rede Neural MLP

**Forward Pass:**

$$\mathbf{h}_1 = \text{ReLU}(\mathbf{W}_1 \cdot \mathbf{x} + \mathbf{b}_1)$$
$$\mathbf{h}_2 = \text{ReLU}(\mathbf{W}_2 \cdot \mathbf{h}_1 + \mathbf{b}_2)$$
$$\hat{y} = \sigma(\mathbf{W}_3 \cdot \mathbf{h}_2 + b_3)$$

Onde:

- $\mathbf{x} = [z_{freq}, z_{energy}]^T$ (entrada normalizada)
- $\mathbf{W}_1 \in \mathbb{R}^{8 \times 2}$, $\mathbf{W}_2 \in \mathbb{R}^{4 \times 8}$, $\mathbf{W}_3 \in \mathbb{R}^{1 \times 4}$
- $\text{ReLU}(x) = \max(0, x)$
- $\sigma(x) = \frac{1}{1+e^{-x}}$

---

## 📦 Como Executar

### Pré-requisitos

- VS Code com extensão PlatformIO
- Python 3.8+ com pip
- Jupyter Notebook ou VS Code com extensão Jupyter

### Instalação de Dependências Python

```bash
cd data_tools
pip install paho-mqtt pandas numpy matplotlib scipy scikit-learn tensorflow
```

### 1. Fase de Coleta de Dados

```bash
# 1. Upload do data_producer.cpp no ESP32
cp test/data_producer.cpp src/main.cpp
pio run --target upload

# 2. Em outro terminal, inicie a coleta de dados
cd data_tools
python data_consumer.py
# Colete dados por 5-10 minutos com motor em diferentes estados
# Pressione CTRL+C para salvar o CSV
```

### 2. Fase de Treinamento

```bash
# Execute o notebook para treinar os modelos
cd data_tools
jupyter notebook eda.ipynb
# Execute todas as células
# Os modelos serão exportados automaticamente
```

### 3. Fase de Inferência

**Opção A: Regressão Logística**

```bash
cp test/predictor.cpp src/main.cpp
pio run --target upload
```

**Opção B: Rede Neural (TinyML)**

```bash
cp test/predictor_tinyML.cpp src/main.cpp
pio run --target upload
```

### 4. Monitoramento

```bash
# Via Serial
pio device monitor

# Via MQTT
mosquitto_sub -h test.mosquitto.org -t "iot/murilo/mpu/anomalias"
```

---

## � Resultados Comparativos

### Métricas de Classificação

| Modelo              | Accuracy | Precision (Anomalia) | Recall (Anomalia) | F1 (Anomalia) | AUC   |
| ------------------- | -------- | -------------------- | ----------------- | ------------- | ----- |
| Regressão Logística | 98.0%    | 79%                  | 71%               | 75%           | ~0.85 |
| Rede Neural MLP     | 99.9%    | 97%                  | 100%              | 98%           | 1.00  |

### Uso de Recursos no ESP32

| Recurso  | Regressão Logística | Rede Neural MLP |
| -------- | ------------------- | --------------- |
| Flash    | ~15%                | ~77.5%          |
| RAM      | ~5%                 | ~18.3%          |
| Latência | < 1ms               | ~2-5ms          |

---

## 🔮 Próximos Passos

- [x] Coleta de dados via MQTT
- [x] Análise exploratória (EDA)
- [x] Modelo estatístico de detecção (Z-Score)
- [x] Treinamento de Regressão Logística
- [x] Embarque do modelo no ESP32
- [x] Implementação com **TensorFlow Lite Micro** (Rede Neural MLP)
- [x] Conversão e exportação do modelo TFLite
- [ ] Dashboard de monitoramento em tempo real (Grafana/InfluxDB)
- [ ] Alertas via Telegram/Email
- [ ] Otimização com quantização INT8
- [ ] Teste com mais cenários de anomalia

---

## 📚 Referências

- [TensorFlow Lite for Microcontrollers](https://www.tensorflow.org/lite/microcontrollers)
- [TensorFlowLite_ESP32 Library](https://github.com/tanakamasayuki/Arduino_TensorFlowLite_ESP32)
- [Adafruit MPU6050 Library](https://github.com/adafruit/Adafruit_MPU6050)
- [PlatformIO Documentation](https://docs.platformio.org/)

---

## 👤 Autor

**Murilo Costa**  
Projeto desenvolvido para a disciplina de **Plataformas de Hardware para IoT** - 2025/2
