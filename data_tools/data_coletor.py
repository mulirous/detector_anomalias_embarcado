import json

import pandas as pd

from paho.mqtt import client as mqtt_client
from time import sleep


def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    data = json.loads(payload)
    userdata.append(data)
    print("Recebido:", data)

def connect_mqtt(broker, port, topic, userdata):
    def on_connect(client, userdata_inner, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
            client.subscribe(topic)
        else:
            print("Failed to connect, return code %d\n", rc)

    client = mqtt_client.Client()
    client.user_data_set(userdata)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(broker, port)
    return client

def turn_into_dataframe(data_list, columns=None):
    df = pd.DataFrame(data_list)
    if columns is not None:
        cols_exist = [c for c in columns if c in df.columns]
        df = df[cols_exist]
    return df
    
def main():
    broker = "test.mosquitto.org"
    port = 1883
    topic = "iot/murilo/mpu/movimento"
    
    raw_data = []
    
    client = connect_mqtt(broker, port, topic, raw_data)
    
    client.loop_start()
    
    print("Coletando dados... Pressione Ctrl+C para parar e salvar em CSV.")
    try:
        while True:
            sleep(1)  # só pra não travar CPU. MQTT roda em thread separada.
    except KeyboardInterrupt:
        print("\nParando coleta e salvando CSV...")
    finally:
        client.loop_stop()
        client.disconnect()

        if len(raw_data) == 0:
            print("Nenhum dado coletado, não vou gerar CSV.")
            return

        columns = ['ax', 'ay', 'az', 'temp']
        df = turn_into_dataframe(raw_data, columns)
        df.to_csv("data/mpu_data.csv", index=False)
        print(f"Salvo {len(df)} linhas em data/mpu_data.csv")


if __name__ == "__main__":
    main()