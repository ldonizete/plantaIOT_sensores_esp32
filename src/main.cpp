#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <float.h>
#include "DHT.h"
#include "Wire.h"
#include "SPI.h"

#define DHTPIN 4
#define DHTTYPE DHT11

int sensorLDR = 14;
int sensorSolo = 34;
int pinoSensorBoia = 25;
int pinoBomba = 23;

DHT dht(DHTPIN, DHTTYPE);

//WiFi
const char* SSID = "timHabay";
const char* PASSWORD = "@@32822776@@";
WiFiClient wifiClient;

//MQTT
const char* BROKER_MQTT = "mqtt.eclipse.org";
int BROKER_PORT = 1883;

#define ID_MQTT "UMID01"
#define TOPIC_UMIDADE_AR "TopUmidadeAR"
#define TOPIC_UMIDADE_SOLO "TopUmidadeSolo"
#define TOPIC_LDR "TopLDR"
#define TOPIC_NIVEL_BOIA "TopNivelBoia"
#define TOPIC_TEMP "TopTemperatura"
#define TOPIC_SUBSCRIBE "TopBomba"

PubSubClient MQTT(wifiClient);

void fazLeituraUmidadeSolo(void);
void fazLeituraUmidadeTempAR();
void isTanqueVazio(void);
void fazLeituraLDR();

void mantemConexoes();
void conectaWiFi();
void conectaMQTT();
void recebePacote(char* topic, byte* payload, unsigned int length);

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(pinoSensorBoia, INPUT);
  pinMode(pinoBomba, OUTPUT);
  digitalWrite(pinoBomba, HIGH);
  Serial.println("Planta IoT com ESP32");

  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(recebePacote);

}

void loop() {

  mantemConexoes();
  isTanqueVazio();
  fazLeituraLDR();
  fazLeituraUmidadeTempAR();
  fazLeituraUmidadeSolo();

  MQTT.loop();
  
  delay(5000);
}

void mantemConexoes() {
  if(!MQTT.connected()) {
    conectaMQTT();
  }
  conectaWiFi();
}

void conectaWiFi() {
  if(WiFi.status() == WL_CONNECTED){
    return;
  }

  Serial.print("Conectando-se na rede: ");
  Serial.print(SSID);
  Serial.println(" Aguarde!");

  WiFi.begin(SSID, PASSWORD);
  while(WiFi.status() != WL_CONNECTED){
    delay(100);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Conectado com sucesso, na rede: ");
  Serial.print(SSID);
  Serial.print(" IP obtido: ");
  Serial.println(WiFi.localIP());
}

void conectaMQTT(){
  while(!MQTT.connected()) {
    Serial.print("Conectando ao BROKER MQTT: ");
    Serial.println(BROKER_MQTT);
    if(MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado ao Broker com sucesso!");
      MQTT.subscribe(TOPIC_SUBSCRIBE);
    }
    else {
      Serial.println("Não foi possivel se conectar ao broker.");
      Serial.println("Nova tentativa de conexao em 10s");
      delay(10000);
    }
  }
}

void fazLeituraUmidadeSolo(void)
{
  int ValorADC;
  float UmidadePercentual;

  ValorADC = analogRead(sensorSolo);
  UmidadePercentual = 100 * ((4095-(float)ValorADC) / 4095);

  char umidadeSolo[16];
  sprintf(umidadeSolo, "%.3f", UmidadePercentual);
  MQTT.publish(TOPIC_UMIDADE_SOLO, umidadeSolo);
}

void fazLeituraUmidadeTempAR(void)
{
  float temperatura = dht.readTemperature();
  char tempString[16];
  sprintf(tempString, "%.3f", temperatura);
  MQTT.publish(TOPIC_TEMP, tempString);

  float umidadeAR = dht.readHumidity();
  char umidadeArString[16];
  sprintf(umidadeArString, "%.3f", umidadeAR);
  MQTT.publish(TOPIC_UMIDADE_AR, umidadeArString);
}

void fazLeituraLDR(void)
{
  int sensorValue = analogRead(sensorLDR);
  float voltage = sensorValue * (3.3 /4095);
  char ldrString[16];
  sprintf(ldrString, "%.3f", voltage);
  MQTT.publish(TOPIC_LDR, ldrString);
}

void isTanqueVazio(void)
{
  int isVazio = digitalRead(pinoSensorBoia);

  if(isVazio == 0) //Tanque vazio
  {
    MQTT.publish(TOPIC_NIVEL_BOIA,"Tanque vazio");
  }

  MQTT.publish(TOPIC_NIVEL_BOIA,"Tanque Cheio");
}

void recebePacote(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Recebi mensagem: ");
  Serial.println(topic);

  String msg;

  //obtem a string do payload recebido
  for(int i=0; i < length; i++)
  {
    char c =(char)payload[i];
    msg += c;
  }

  if(msg == "0") {
    digitalWrite(pinoBomba, LOW);
  }

  if (msg == "1") {
    digitalWrite(pinoBomba, HIGH);
  }
}