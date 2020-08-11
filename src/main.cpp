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
#include "esp_camera.h"
#include <ArduinoJson.h> //ArduinoJSON6
DynamicJsonDocument CONFIG(2048);

#define DHTPIN 4
#define DHTTYPE DHT11

// int sensorLDR = 14;
int pinMoisture = 34;
int pinoSensorBoia = 25;
int pinBomb = 23;
int pinLight = 22;
int pinFan = 33;
int pinExhaust = 32;

DHT dht(DHTPIN, DHTTYPE);

//NTP
WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000);

//WiFi
const char* SSID = "teste";
const char* PASSWORD = "12345678";
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
#define TOPIC_SUBSCRIBE_FAN "topFan"
#define TOPIC_SUBSCRIBE_EXHAUST "topExhaust"
#define TOPIC_PHOTO "topTakeAPicture"
#define TOPIC_CONFIG "topJSONConfig"
#define TOPIC_UP "topPICTURE"

PubSubClient MQTT(wifiClient);

float valueMoisture();
void valueHumidity();
void valueTemperature();
void isTanqueVazio();
void take_picture();
void callback();
void camera_init();
void take_picture();
// bool valueLDR();

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
  
  // pinMode(sensorLDR, INPUT);
  pinMode(pinoSensorBoia, INPUT);

  pinMode(pinBomb, OUTPUT);
  digitalWrite(pinBomb, HIGH);

  pinMode(pinFan, OUTPUT);
  digitalWrite(pinFan, HIGH);

  pinMode(pinExhaust, OUTPUT);
  digitalWrite(pinExhaust, HIGH);
  
  pinMode(pinLight, OUTPUT);
  digitalWrite(pinLight, HIGH);
  
  Serial.println("Planta IoT com ESP32");

  conectaWiFi();
  camera_init();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(recebePacote);
  ntp.begin();               // Inicia o protocolo
  ntp.forceUpdate();
}

void loop() {
  mantemConexoes();
  isTanqueVazio();
  // bool dataLDR = valueLDR();
  valueHumidity();
  valueTemperature();
  float moisture = valueMoisture();

  if(isAutomatic==true)
  {
    if(moisture < moistureSoilMin)
    {
      digitalWrite(pinBomb, LOW);
      delay(3000);
      digitalWrite(pinBomb, HIGH);
    }

    int hour = ntp.getHours(); 

    if(hour >= startLight && hour <= endLight )
    {
      digitalWrite(pinLight, LOW);
      digitalWrite(pinFan, LOW);
      digitalWrite(pinExhaust, LOW);
    }
    else {
      digitalWrite(pinLight, HIGH);
      digitalWrite(pinFan, LOW);
      digitalWrite(pinExhaust, LOW);
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
      MQTT.subscribe(TOPIC_PHOTO);
      MQTT.subscribe(TOPIC_UP);
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
    MQTT.publish(TOPIC_NIVEL_BOIA, "empty");
  } else {
    MQTT.publish(TOPIC_NIVEL_BOIA,"full");
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
    digitalWrite(pinBomb, LOW);
    }

    if (msg == "off") {
      digitalWrite(pinBomb, HIGH);
    }
  }

  if(strcmp("topFan", topic) == 0)
  {
    if(msg == "on") {
    digitalWrite(pinFan, LOW);
    }

    if (msg == "off") {
      digitalWrite(pinFan, HIGH);
    }
  }

  if(strcmp("topExhaust", topic) == 0)
  {
    if(msg == "on") {
    digitalWrite(pinExhaust, LOW);
    }

    if (msg == "off") {
      digitalWrite(pinExhaust, HIGH);
    }
  }

  if(strcmp("topLight", topic) == 0)
  {
    if(msg == "on") {
    digitalWrite(pinLight, LOW);
    }

    if(msg == "off") {
      digitalWrite(pinLight, HIGH);
    }
  }

  if(strcmp("topTakeAPicture", topic) == 0)
  {
    Serial.println("PING");
    take_picture();
  }

  if (topic == TOPIC_CONFIG) {
    deserializeJson(CONFIG, msg);
    Serial.println(msg);
    sensor_t * s = esp_camera_sensor_get();
    s->set_framesize(s, FRAMESIZE_VGA); //QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    s->set_vflip(s, CONFIG["vflip"]); //0 - 1
    s->set_hmirror(s, CONFIG["hmirror"]); //0 - 1
    s->set_colorbar(s, CONFIG["colorbar"]); //0 - 1
    s->set_special_effect(s, CONFIG["special_effect"]); // 0 - 6
    s->set_quality(s, CONFIG["quality"]); // 0 - 63
    s->set_brightness(s, CONFIG["brightness"]); // -2 - 2
    s->set_contrast(s, CONFIG["contrast"]); // -2 - 2
    s->set_saturation(s, CONFIG["saturation"]); // -2 - 2
    s->set_sharpness(s, CONFIG["sharpness"]); // -2 - 2
    s->set_denoise(s, CONFIG["denoise"]); // 0 - 1
    s->set_awb_gain(s, CONFIG["awb_gain"]); // 0 - 1
    s->set_wb_mode(s, CONFIG["wb_mode"]); // 0 - 4
  }
}


void camera_init() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = 5;
  config.pin_d1       = 18;
  config.pin_d2       = 19;
  config.pin_d3       = 21;
  config.pin_d4       = 36;
  config.pin_d5       = 39;
  config.pin_d6       = 34;
  config.pin_d7       = 35;
  config.pin_xclk     = 0;
  config.pin_pclk     = 22;
  config.pin_vsync    = 25;
  config.pin_href     = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn     = 32;
  config.pin_reset    = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size   = FRAMESIZE_VGA; // QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
  config.jpeg_quality = 10;           
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void take_picture() {
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  if (MQTT_MAX_PACKET_SIZE == 128) {
    //SLOW MODE (increase MQTT_MAX_PACKET_SIZE)
    MQTT.publish_P(TOPIC_UP, fb->buf, fb->len, false);
  }
  else {
    //FAST MODE (increase MQTT_MAX_PACKET_SIZE)
    MQTT.publish(TOPIC_UP, fb->buf, fb->len, false);
  }
  Serial.println("CLIC");
  esp_camera_fb_return(fb);
}
// bool valueLDR(void)
// {
//   int valueLDR = digitalRead(sensorLDR);

//   if(valueLDR==0)
//   {
//     MQTT.publish(TOPIC_LDR, "light");
//     return true;
//   } else {
//     MQTT.publish(TOPIC_LDR, "dark");
//     return false;
//   }
  // int sensorValue = analogRead(sensorLDR);
  // float voltage = sensorValue * (3.3 /4095);
  // char ldrString[16];
  // sprintf(ldrString, "%.3f", voltage);
  // MQTT.publish(TOPIC_LDR, ldrString);
// }