#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <string.h>
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
int pinoLuz = 22;

DHT dht(DHTPIN, DHTTYPE);

//WiFi
const char* SSID = "timHabay";
const char* PASSWORD = "@@32822776@@";
WiFiClient wifiClient;

//MQTT
const char* BROKER_MQTT = "mqtt.eclipse.org";
int BROKER_PORT = 1883;

#define ID_MQTT "UMID01"
#define TOPIC_UMIDADE_AR "topHumidity"
#define TOPIC_UMIDADE_SOLO "topUmidadeSolo"
#define TOPIC_LDR "topSensorLDR"
#define TOPIC_NIVEL_BOIA "topFloatSwitch"
#define TOPIC_TEMP "topTemperature"
#define TOPIC_SUBSCRIBE_WATERBOMB "topWaterBomb"
#define TOPIC_SUBSCRIBE_LIGHT "topLight"

PubSubClient MQTT(wifiClient);

float valueMoisture();
float valueHumidity();
float valueTemperature();
bool isTanqueVazio();
bool valueLDR();

void mantemConexoes();
void conectaWiFi();
void conectaMQTT();
void recebePacote(char* topic, byte* payload, unsigned int length);

//Automatic
bool isAutomatic = true;
int moistureSoilMin = 40;
int startLight = 6;
int endLight = 18;
int weeks = 4;

void setup() {
  Serial.begin(9600);
  dht.begin();
  
  pinMode(sensorLDR, INPUT);
  pinMode(pinoSensorBoia, INPUT);

  pinMode(pinoBomba, OUTPUT);
  digitalWrite(pinoBomba, HIGH);
  
  pinMode(pinoLuz, OUTPUT);
  digitalWrite(pinoLuz, HIGH);
  
  Serial.println("Planta IoT com ESP32");

  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(recebePacote);
}

void loop() {
  mantemConexoes();
  bool isEmpty = isTanqueVazio();
  bool dataLDR = valueLDR();
  float humidity = valueHumidity();
  float temperature = valueTemperature();
  float moisture = valueMoisture();

  //Get system hour
  time_t rawtime = time(NULL);
  struct tm *ptm = localtime(&rawtime);
  int hour = ptm->tm_hour;

  if(isAutomatic==true)
  {
    if(moisture < moistureSoilMin)
    {
      digitalWrite(pinoBomba, LOW);
      delay(3000);
      digitalWrite(pinoBomba, HIGH);
    }

    if(hour >= startLight && hour <= endLight )
    {
      digitalWrite(pinoLuz, LOW);
    }
    else {
      digitalWrite(pinoLuz, HIGH);
    }

  }

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
      MQTT.subscribe(TOPIC_SUBSCRIBE_WATERBOMB);
      MQTT.subscribe(TOPIC_SUBSCRIBE_LIGHT);
    }
    else {
      Serial.println("Não foi possivel se conectar ao broker.");
      Serial.println("Nova tentativa de conexao em 10s");
      delay(10000);
    }
  }
}

float valueMoisture()
{
  int ValorADC;
  float UmidadePercentual;

  ValorADC = analogRead(sensorSolo);
  UmidadePercentual = 100 * ((4095-(float)ValorADC) / 4095);

  char umidadeSolo[16];
  sprintf(umidadeSolo, "%.3f", UmidadePercentual);
  MQTT.publish(TOPIC_UMIDADE_SOLO, umidadeSolo);

  return UmidadePercentual;
}

float valueHumidity()
{
  float humidity = dht.readHumidity();
  char humidityString[16];
  sprintf(humidityString, "%.3f", humidity);
  MQTT.publish(TOPIC_UMIDADE_AR, humidityString);

  return humidity;
}

float valueTemperature()
{
  float temp = dht.readTemperature();
  char tempString[16];
  sprintf(tempString, "%.3f", temp);
  MQTT.publish(TOPIC_TEMP, tempString);

  return temp;
}

bool valueLDR(void)
{
  int valueLDR = digitalRead(sensorLDR);

  if(valueLDR==0)
  {
    MQTT.publish(TOPIC_LDR, "light");
    return true;
  } else {
    MQTT.publish(TOPIC_LDR, "dark");
    return false;
  }
  // int sensorValue = analogRead(sensorLDR);
  // float voltage = sensorValue * (3.3 /4095);
  // char ldrString[16];
  // sprintf(ldrString, "%.3f", voltage);
  // MQTT.publish(TOPIC_LDR, ldrString);
}

bool isTanqueVazio(void)
{
  int isVazio = digitalRead(pinoSensorBoia);

  if(isVazio == 0) //Tanque vazio
  {
    MQTT.publish(TOPIC_NIVEL_BOIA, "empty");
    return true;
  } else {
    MQTT.publish(TOPIC_NIVEL_BOIA,"full");
    return false;
  }  
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

  if(strcmp("topWaterBomb", topic) == 0)
  {
    if(msg == "on") {
    digitalWrite(pinoBomba, LOW);
    }

    if (msg == "off") {
      digitalWrite(pinoBomba, HIGH);
    }
  }

  if(strcmp("topLight", topic) == 0)
  {
    if(msg == "on") {
    digitalWrite(pinoLuz, LOW);
    }

    if(msg == "off") {
      digitalWrite(pinoLuz, HIGH);
    }
  }
}