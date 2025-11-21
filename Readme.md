# 🔍 Detecção de Anomalias por meio de frequências de aceleração com ESP32 e MPU6050

Este projeto visa implementar um sistema de **Machine Learning Embarcado (TinyML)** em um ESP32 para identificar anomalias em motores em tempo real. O sistema utiliza um acelerômetro/giroscópio **MPU6050** para monitorar vibrações e temperatura, processando os dados localmente para detectar falhas operacionais.

Atualmente, o projeto encontra-se na fase de **Coleta e Exploração de Dados**, com um modelo estatístico preliminar desenvolvido para validação.

---

## 🚀 Objetivo

Desenvolver um dispositivo IoT capaz de:

1. **Coletar dados de vibração** (aceleração nos eixos X, Y, Z) e temperatura de um motor.
2. **Detectar anomalias** em tempo real utilizando um modelo de Machine Learning embarcado (futuramente Regressão Logística).
3. **Enviar dados e diagnósticos** via protocolo **MQTT** para um dashboard de monitoramento.

---

## 📂 Estrutura do Projeto

O projeto está organizado da seguinte forma:

```text
projeto_final_mpu/
├── src/
│   └── main.cpp           # Firmware do ESP32 (C++). Leitura do sensor e envio MQTT.
├── data_tools/            # Ferramentas de análise e coleta de dados (Python).
│   ├── data_coletor.py    # Script para coletar dados do MQTT e salvar em CSV.
│   ├── eda.ipynb          # Notebook de Exploração de Dados (EDA) e prototipagem de modelos.
│   └── data/
│       └── mpu_data.csv   # Dataset coletado contendo aceleração e temperatura.
├── platformio.ini         # Configuração do ambiente PlatformIO.
└── Readme.md              # Documentação do projeto.
```

---

## 🛠️ Componentes e Tecnologias

- **Hardware**:
  - ESP32 (Microcontrolador)
  - MPU6050 (Acelerômetro e Giroscópio)
- **Firmware**:
  - C++ (PlatformIO / Arduino Framework)
  - Bibliotecas: `Adafruit MPU6050`, `PubSubClient` (MQTT), `WiFi`.
- **Data Science & Backend**:
  - Python 3
  - Pandas, Matplotlib, Seaborn (Análise de Dados)
  - Paho-MQTT (Coleta de dados)
  - Jupyter Notebook (Prototipagem)

---

## ⚙️ Como Funciona

### 1. Coleta de Dados (Firmware)

O código em `src/main.cpp` configura o ESP32 para ler o sensor MPU6050 a uma taxa de amostragem definida. Os dados brutos (`ax`, `ay`, `az`, `temp`) são formatados em JSON e publicados no tópico MQTT `iot/murilo/mpu/movimento`.

### 2. Armazenamento (Python)

O script `data_tools/data_coletor.py` atua como um cliente MQTT no computador. Ele:

- Assina o tópico de movimento.
- Recebe os pacotes JSON em tempo real.
- Armazena os dados em memória.
- Ao interromper a execução (CTRL+C), salva todo o histórico no arquivo `data_tools/data/mpu_data.csv`.

### 3. Análise e Modelagem (Jupyter)

O notebook `data_tools/eda.ipynb` realiza:

- **EDA (Exploratory Data Analysis)**: Visualização dos dados brutos e identificação de padrões.
- **Engenharia de Features**: Cálculo da magnitude da aceleração e aplicação de FFT (Fast Fourier Transform) para análise no domínio da frequência.
- **Detecção de Anomalias (Estatística)**: Implementação de um modelo baseado em _Z-Score_ para identificar outliers na frequência de pico e energia espectral.

---

## 🔮 Próximos Passos

- [x] Coleta de dados via MQTT.
- [x] Análise exploratória e validação estatística.
- [ ] Treinamento de um modelo de **Regressão Logística** com os dados coletados.
- [ ] Conversão do modelo para C++ (TinyML).
- [ ] Embarcar o modelo no ESP32 para inferência em tempo real.
- [ ] Criação de Dashboard para visualização dos alertas.

---

## 📦 Como Executar

1. **Firmware**:
   - Abra o projeto no VS Code com a extensão PlatformIO.
   - Conecte o ESP32 e faça o upload do código em `src/main.cpp`.
2. **Coleta de Dados**:

   - Certifique-se de ter Python instalado.
   - Instale as dependências necessárias (`paho-mqtt`, `pandas`, etc.).
   - Execute o coletor:

     ```bash
     python data_tools/data_coletor.py
     ```

   - Deixe rodando enquanto o ESP32 envia dados. Pressione `CTRL+C` para salvar o CSV.

3. **Análise**:
   - Abra o notebook `data_tools/eda.ipynb` para visualizar os gráficos e testar o algoritmo de detecção.

---

**Autor**: Murilo Costa
