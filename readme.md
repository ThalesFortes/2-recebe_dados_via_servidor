# Control LED RGB With MQTT

Este programa recebe de dados via servidor o recebimento de mensagens.
pelo servidor HiveMQ, utilizando o protocolo MQTT. Os dados recebidos devem 
ligar ou desligar um LED, que deve ser ligado a placa.

#Funcionalidades
Leitura do pino e envio do status, controle do led RGB, mudança no status do pino para alto ou baixo

#Configuração do WiFi
Para configurar as credenciais do WiFi, edite o arquivo. Localize as seguintes linhas no início do arquivo:

#WiFi credentials
#define WIFI_SSID "WIFI" #define WIFI_PASSWORD "SENHA-DO-WIFI"

Substitua:

Nome da Rede pelo nome da sua rede WiFi (Ex.: "Multilaser 2.4") Senha da Rede pela senha da sua rede WiFi (Ex.: "12345678@")

##USO
Como Usar Compile e envie o executável (.uf2) para o Pico W / BitDogLab Caso seja apenas o Pico W, será necessário conectar os periféricos usados 

Acesse  HiveMQ WebSocket Client https://www.hivemq.com/demos/websocket-client/ 
Conectar-se ao broker.hivemq.com.
Assinar o tópico bitdoglab/status (para ver confirmações).
Publicar em bitdoglab/control com payloads como "R_ON" ou "PIN_OFF".
