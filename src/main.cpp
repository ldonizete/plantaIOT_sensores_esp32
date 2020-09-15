#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <NTPClient.h>
#include "DHT.h"
#include "Wire.h"
#include "SPI.h"
#include <ArduinoJson.h> //ArduinoJSON6
#include <Adafruit_Sensor.h>

DynamicJsonDocument CONFIG(2048);

//Não usar o pino 12

#define DHTPIN 14
#define DHTTYPE DHT11

int pinMoisture = 34;
int pinoSensorBoia = 25;
int pinBomb = 33;
int pinLight = 32;
int pinFan = 26;

DHT dht(DHTPIN, DHTTYPE);

//NTP
WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000);

//WiFi
const char* SSID = "timHabay";
const char* PASSWORD = "@@32822776@@";
WiFiClient wifiClient;

//ID Product
const char* IDProduct = "5f104a6d3500f222d0086512";

//MQTT
const char* BROKER_MQTT = "mqtt.eclipse.org";
int BROKER_PORT = 1883;

#define ID_MQTT "UMID01"
#define TOPIC_ID_PRODUCT "topIdProduct"
#define TOPIC_UMIDADE_AR "topHumidity"
#define TOPIC_UMIDADE_SOLO "topUmidadeSolo"
#define TOPIC_LDR "topSensorLDR"
#define TOPIC_NIVEL_BOIA "topFloatSwitch"
#define TOPIC_TEMP "topTemperature"
#define TOPIC_SUBSCRIBE_WATERBOMB "topWaterBomb"
#define TOPIC_SUBSCRIBE_LIGHT "topLight"
#define TOPIC_SUBSCRIBE_FAN "topFan"
#define TOPIC_SUBSCRIBE_EXHAUST "topExhaust"
#define TOPIC_SUBSCRIBE_AUTO "topAutomatic"

PubSubClient MQTT(wifiClient);

float valueMoisture();
void valueHumidity();
void valueTemperature();
void isTanqueVazio();
void callback();
void mantemConexoes();
void conectaWiFi();
void conectaMQTT();
void recebePacote(char* topic, byte* payload, unsigned int length);

//Automatic
int moistureSoilMin = 40;
int startLight = 6;
int endLight = 18;
int weeks = 4;
int hourPhoto = 8;
int lastUploadTime = 0;
float moisture = 0;
bool automatic = true;

void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(pinoSensorBoia, INPUT);

  pinMode(pinBomb, OUTPUT);
  digitalWrite(pinBomb, HIGH);

  pinMode(pinFan, OUTPUT);
  digitalWrite(pinFan, HIGH);
  
  pinMode(pinLight, OUTPUT);
  digitalWrite(pinLight, HIGH);

  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(recebePacote);
  ntp.begin();               // Inicia o protocolo
  ntp.forceUpdate();
}

void loop() {
  int hour = ntp.getHours(); 
 
  if(lastUploadTime==0 || (hour-lastUploadTime)==1)
  {
    //Envia o ID
    MQTT.publish(TOPIC_ID_PRODUCT, IDProduct);
    lastUploadTime = hour;
    Serial.println("Primeiro envio");
    Serial.println(lastUploadTime);
    mantemConexoes();
    isTanqueVazio();
    valueHumidity();
    valueTemperature();
    moisture = valueMoisture();
  }

  if(automatic==true)
  {
    if(moisture < moistureSoilMin)
    {
      digitalWrite(pinBomb, LOW);
    }else
    {
      digitalWrite(pinBomb, HIGH);
    }
    
    if(hour >= startLight && hour <= endLight )
    {
      digitalWrite(pinLight, LOW);
      digitalWrite(pinFan, LOW);
    }
    else {
      digitalWrite(pinLight, HIGH);
      digitalWrite(pinFan, HIGH);
    }
  }

  MQTT.loop();
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

  WiFi.mode(WIFI_STA);//pesquisar depois
  WiFi.setHostname(ID_MQTT);
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
      MQTT.subscribe(TOPIC_SUBSCRIBE_WATERBOMB);
      MQTT.subscribe(TOPIC_SUBSCRIBE_LIGHT);
      MQTT.subscribe(TOPIC_SUBSCRIBE_FAN);
      MQTT.subscribe(TOPIC_SUBSCRIBE_EXHAUST);
      MQTT.subscribe(TOPIC_SUBSCRIBE_AUTO);
    }
    else {
      Serial.println("Não foi possivel se conectar ao broker.");
      Serial.println("Nova tentativa de conexao em 10s");
      Serial.print(MQTT.state());
      delay(10000);
    }
  }
}

float valueMoisture()
{
  int ValorADC;
  float UmidadePercentual;

  ValorADC = analogRead(pinMoisture);

  UmidadePercentual = 100 * ((4095-(float)ValorADC) / 4095);

  char umidadeSolo[16];
  sprintf(umidadeSolo, "%.3f", UmidadePercentual);
  
  MQTT.publish(TOPIC_UMIDADE_SOLO, umidadeSolo);

  return UmidadePercentual;
}

void valueHumidity()
{
  float humidity = dht.readHumidity();
  char humidityString[16];
  sprintf(humidityString, "%.3f", humidity);
  MQTT.publish(TOPIC_UMIDADE_AR, humidityString);
}

void valueTemperature()
{
  float temp = dht.readTemperature();
  char tempString[16];
  sprintf(tempString, "%.3f", temp);
  MQTT.publish(TOPIC_TEMP, tempString);
}

void isTanqueVazio()
{
  int isVazio = digitalRead(pinoSensorBoia);

  if(isVazio == 0) //Tanque vazio
  {
    MQTT.publish(TOPIC_NIVEL_BOIA, "vazio");
  } else {
    MQTT.publish(TOPIC_NIVEL_BOIA,"cheio");
  }  
}

void recebePacote(char* topic, byte* payload, unsigned int length)
{
  String msg;

  //obtem a string do payload recebido
  for(int i=0; i < length; i++)
  {
    char c =(char)payload[i];
    msg += c;
  }

  Serial.print("Recebi mensagem: ");
  Serial.println(topic);
  Serial.println(msg);

  if(strcmp("topAutomatic", topic) == 0)
  {
    automatic = (msg == "on") ? true : false;
  }

  if(automatic==false)
  {
    if(strcmp("topWaterBomb", topic) == 0)
    {
      digitalWrite(pinBomb, (msg == "on") ? LOW : HIGH);
    }

    if(strcmp("topFan", topic) == 0)
    {
      digitalWrite(pinFan, (msg == "on") ? LOW : HIGH);
    }

    if(strcmp("topLight", topic) == 0)
    {
      digitalWrite(pinLight, (msg == "on") ? LOW : HIGH);
    }
  }
}
