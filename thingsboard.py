'''
IE-0624 Laboratorio de Microcontroladores
Alexa Carmona Buzo B91643
Raquel Corrales Marin B92378
Laboratorio 4
Sismógrafo
'''
# Este script se encarga de leer los data enviados desde
# el microcontrolador y los publica en el tópico de telemetría
# de Thingsboard para ser desplegados en un dasboard

#Se importan las librerias
import time
import json
import serial
import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.connected = True
        print("La conexión ha sido exitosa")
    else: 
        print("Conexión no exitosa. Código: ", rc)
        client.loop_stop()
def on_disconnect(client, userdata, rc):
    if(rc == 0):
        print("La desconexión ha sido exitosa")
    else:
        print("Sistema desconectado mediante el código: ", rc)
def on_publish(client, userdata, mid):
    print("Código: ", mid, " ha abandonado el cliente")

# Leer datos del puerto
data = serial.Serial("/dev/ttyACM1",115200,timeout=1) 
print("Se ha conectado al puerto serial /dev/ttyACM1")
client = mqtt.Client("stm_client")
client.connected = False
client.on_connect = on_connect
client.on_disconnect = on_disconnect
client.on_publish = on_publish

# Agregar credenciales del servidor
broker ="iot.eie.ucr.ac.cr"
port = 1883
topico = "v1/tokens/me/telemetry"
token = "isHSmhJKPJPSTBGx61wj"
client.username_pw_set(token)
client.connect(broker, port)

# Definir estructura json
dict = dict()

# Si no se conecta, dormir
while client.connected != True:
    client.loop()
    time.sleep(2)

# En caso de conexión exitosa
while (1):
    data = data.readline().decode('utf-8').replace('\r', "").replace('\n', "")
    data = data.split('\t')
    dict["Eje x"] = data[0]
    dict["Eje y"] = data[1]
    dict["Eje z"] = data[2]
    dict["Voltaje en la bateria"] = data[3]

    if(float( data[3]) < 7):
        dict["Bateria Baja"] = "Si"
    else:
        dict["Bateria Baja"] = "No"
    # Convertir a json
    output = json.dumps(dict)

    # Imprimir en consola y enviar
    print(output)
    client.publish(topico, output)
    time.sleep(5)
    
