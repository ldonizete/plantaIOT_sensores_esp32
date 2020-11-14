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
#include <HTTPClient.h>

DynamicJsonDocument CONFIG(2048);

char json[400] = {0};
StaticJsonDocument<256> doc;

#define DHTPIN 14
#define DHTTYPE DHT11

//Pinos configurados
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
const char* SSID = "Desktop";
const char* PASSWORD = "32822776";
WiFiClient wifiClient;

//ID Product
const char* IDProduct = "5f7d97d6845a092fec07cc6b";//"5f104a6d3500f222d0086512";

//MQTT
const char* BROKER_MQTT = "mqtt.eclipse.org";
int BROKER_PORT = 1883;

//Canais MQTT
#define ID_MQTT "UMID01"
#define TOPIC_ID_PRODUCT "topIdProduct"
#define TOPIC_UMIDADE_AR "topHumidity"
#define TOPIC_UMIDADE_SOLO "topUmidadeSolo"
#define TOPIC_NIVEL_BOIA "topFloatSwitch"
#define TOPIC_TEMP "topTemperature"
#define TOPIC_SUBSCRIBE_WATERBOMB "topWaterBomb"
#define TOPIC_SUBSCRIBE_LIGHT "topLight"
#define TOPIC_SUBSCRIBE_FAN "topFan"
#define TOPIC_SUBSCRIBE_UPDATE_CONFIG "topUpdateConfig"

PubSubClient MQTT(wifiClient);

//Declaração das funcoes
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
int moistureSoilMin = 0;
int moistureSoilMax = 0;
int startLight = 0;
int endLight = 0;
int lastUploadTime = 0;
float moisture = 0;
bool irrigar = false;
int hour = 0;
bool isConfigured = false;

//Seta as variaveis com a configuração do HTTP
void resultOfGet(String msg)
{
  memset(json,0,sizeof(json));
  msg.toCharArray(json, 400);
  deserializeJson(doc, json);
  JsonObject config = doc["config"];
  moistureSoilMin = config["moistureSoilMin"]; 
  moistureSoilMax = config["moistureSoilMax"]; 
  startLight = config["startLight"]; 
  endLight = config["endLight"]; 
}

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
  //Passo a configuracao do MQTT Broker e a porta
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(recebePacote);//Recebe os dados 
  ntp.begin();// Inicia o protocolo de hora
  ntp.forceUpdate();

  const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(8) + 146;
  DynamicJsonDocument doc(capacity);
}

void loop() {
  hour = ntp.getHours(); //Pega a hora atual
  moisture = valueMoisture();
  
  //Envia os dados dos sensores via MQTT de 1 em 1 hora
  if(lastUploadTime==0 || (hour-lastUploadTime)==1)
  {
    //Envia o ID do produto
    MQTT.publish(TOPIC_ID_PRODUCT, IDProduct);
    lastUploadTime = hour;
    mantemConexoes();
    isTanqueVazio();
    valueHumidity();
    valueTemperature();

    char umidadeSolo[16];
    sprintf(umidadeSolo, "%.2f", moisture);
    
    MQTT.publish(TOPIC_UMIDADE_SOLO, umidadeSolo);
  }

  //Faz o irrigamento automatico baseado na umidade do solo
  if(irrigar==true)
  {
    if(moistureSoilMax>=moisture)
    {
      digitalWrite(pinBomb, LOW);
    }
    else {irrigar=false;}
  }
  else if(irrigar==false)
  {
    if(moisture < moistureSoilMin)
    {
      irrigar=true;
    }else
    {
      digitalWrite(pinBomb, HIGH);
    }
  }

  //Deixa ligado até passar da hora
  if(hour >= startLight && hour < endLight )
  {
    digitalWrite(pinLight, LOW);
    digitalWrite(pinFan, LOW);
  }
  else {
    digitalWrite(pinLight, HIGH);
    digitalWrite(pinFan, HIGH);
  }

  //Faz requisição HTTP para pegar as configurações da estufa
  if(isConfigured==false)
  {
    HTTPClient http;

    http.begin("https://node-grow.herokuapp.com/plants/config/" + String(IDProduct));
    int httpCode = http.GET();

    if (httpCode > 0) 
    { 
      isConfigured = true;
      String payload = http.getString();
      resultOfGet(payload);
    }else {
      Serial.println("Error on HTTP request");
    }

    http.end(); 
  }
  
  MQTT.loop();
}
//Mantem a conexao do wifi
void mantemConexoes() {
  if(!MQTT.connected()) {
    conectaMQTT();
  }
  conectaWiFi();
}
//Conecta no wifi
void conectaWiFi() {
  if(WiFi.status() == WL_CONNECTED){
    return;
  }

  Serial.print("Conectando-se na rede: ");
  Serial.print(SSID);
  Serial.println(" Aguarde!");

  WiFi.mode(WIFI_STA);
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
//Conecta no broker do MQTT
void conectaMQTT(){
  while(!MQTT.connected()) {
    Serial.print("Conectando ao BROKER MQTT: ");
    Serial.println(BROKER_MQTT);
    if(MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado ao Broker com sucesso!");
      MQTT.subscribe(TOPIC_SUBSCRIBE_UPDATE_CONFIG);
    }
    else {
      Serial.println("Não foi possivel se conectar ao broker.");
      Serial.println("Nova tentativa de conexao em 10s");
      Serial.print(MQTT.state());
      delay(10000);
    }
  }
}
//Verifica a umidade do solo
float valueMoisture()
{
  int ValorADC;
  float UmidadePercentual;

  //Faz a leitura do analógica do sensor 
  ValorADC = analogRead(pinMoisture);

  //Faz uma conversão para saber a umidade do solo em %
  UmidadePercentual = 100 * ((4095-(float)ValorADC) / 4095);

  return UmidadePercentual;
}
//Verifica a umidado do ar
void valueHumidity()
{
  //Faz a leitura da umidade do ar
  float humidity = dht.readHumidity();
  char humidityString[16];
  
  //Converte o dado de float para string
  sprintf(humidityString, "%.2f", humidity);

  //Publica os dados via MQTT
  MQTT.publish(TOPIC_UMIDADE_AR, humidityString);
}
//Verifica a temperatura do ambiente
void valueTemperature()
{
  //Faz a leitura da temperatura do ar
  float temp = dht.readTemperature();
  char tempString[16];
  //Converte o dado de float para string
  sprintf(tempString, "%.2f", temp);
  //Publica os dados via MQTT
  MQTT.publish(TOPIC_TEMP, tempString);
}
//Verifica se o tanque ta vazio
void isTanqueVazio()
{
  //Faz a leitura digital da boia
  int isVazio = digitalRead(pinoSensorBoia);

  if(isVazio == 0) //Tanque vazio
  {
    MQTT.publish(TOPIC_NIVEL_BOIA, "vazio");
  } else {
    MQTT.publish(TOPIC_NIVEL_BOIA,"cheio");
  }  
}
//Metódo que recebe pacote via MQTT
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

  //Se a mensagem vim desse tópico assinado
  if(strcmp("topUpdateConfig", topic) == 0)
  {
    //Seta como false a variavel, para fazer a requisição
    //da configuração da estufa denovo
    if(msg == "on")
    {
      isConfigured = false;
    }
   
    // digitalWrite(pinBomb, (msg == "on") ? LOW : HIGH);
  }  
}
