#include <Arduino.h>
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


float fazLeituraUmidadeSolo(void);
void fazLeituraUmidadeTempAR();
int isTanqueVazio(void);
void fazLeituraLDR();

float fazLeituraUmidadeSolo(void)
{
  int ValorADC;
  float UmidadePercentual;

  ValorADC = analogRead(sensorSolo);
  UmidadePercentual = 100 * ((4095-(float)ValorADC) / 4095);

  return UmidadePercentual;
}

void fazLeituraUmidadeTempAR(void)
{
  float temperatura = dht.readTemperature();
  Serial.printf("Temperatura: %f\n", temperatura);
  float umidadeAR = dht.readHumidity();
  Serial.printf("Umidade AR: %f\n", umidadeAR);
}

void fazLeituraLDR(void)
{
  int sensorValue = analogRead(sensorLDR);
  float voltage = sensorValue * (3.3 /4095);
  Serial.printf("Sensor LDR VOLT: %f\n", voltage);
}

int isTanqueVazio(void)
{
  int isVazio = digitalRead(pinoSensorBoia);
  Serial.printf("Estado sensor: %d\n", isVazio);

  return isVazio;
}

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(pinoSensorBoia, INPUT);
  pinMode(pinoBomba, OUTPUT);
  Serial.println("Planta IoT com ESP32");
}

void loop() {
  float UmidadePercentualLida;
  isTanqueVazio();
  fazLeituraLDR();
  UmidadePercentualLida = fazLeituraUmidadeSolo();
  Serial.printf("Umidade do solo: %f\n", UmidadePercentualLida);
  fazLeituraUmidadeTempAR();
  digitalWrite(pinoBomba, HIGH);
  Serial.print("teste1");
  //digitalWrite(pinoBomba, LOW);
  delay(5000);
}