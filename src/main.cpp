#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <math.h>
#include <Thermistor.h>
#include <ESP32TimerInterrupt.h>
#include <Nanoshield_ADC.h>

#include "hardware.h"
#include "server.h"
#include "temperature.h"


/* #region VARIABLES DECLARATIONS */

bool
    uploading = false,
    saidaRele2 = false,
    saidaRele3 = false,
    saidaRele4 = false;

int
    pulsosContados,
    tempoAnterior,
    tempoAtual,
    tempoDif;

float
    leituraVcc,
    leituraVcc2,
    resistencia,
    resistencia2,
    temperatura,
    temperatura2;

volatile float
    SAIDA_1 = 0.0,
    SAIDA_2 = 0.0;

volatile bool
    detected = false,
    flag_100ms,
    flag_1s;

volatile int
    count_detected,
    count_timer_1s;

volatile unsigned long
    micros_dif,
    micros_count,
    micros_count_ant,
    micro_rising_edge,
    tempo_pulso,
    tempo_atraso_total;
/* #endregion */

/* #region CLASS DECLARATIONS */

Nanoshield_ADC adc;

//*
hw_timer_t *timer_saida_1 = NULL;
hw_timer_t *timer_saida_2 = NULL;
hw_timer_t *timer_100ms = NULL;
//*/

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
/* #endregion */

void IRAM_ATTR ISR_detect_zero()
{
  portENTER_CRITICAL_ISR(&mux);
  if ((micros() - micros_count) > (2 * tempo_pulso))
  {
    count_detected++;
    micros_count = micros();
    // timerAlarmDisable(timer_saida_1);
    // timerAlarmEnable(timer_saida_1);
  }
  portEXIT_CRITICAL_ISR(&mux);
}

void IRAM_ATTR ISR_pulsar_saida_1()
{
  // delayMicroseconds(random(100));
  micros_dif = micros() - micros_count;

  digitalWrite(PIN_DISPARO_1, HIGH);
  delayMicroseconds(20);
  digitalWrite(PIN_DISPARO_1, LOW);
}

void IRAM_ATTR ISR_timer_100ms()
{
  count_timer_1s++;
  flag_100ms = true;
  if (count_timer_1s >= 10)
  {
    count_timer_1s = 0;
    flag_1s = true;
  }
}

void setup()
{
  delay(1000);
  // Blink 3 sec to indicate setup initilization and wait circuits initialize
  pinMode(LED_PIN, OUTPUT); // Configure LED pin
  digitalWrite(LED_PIN, HIGH);
  delay(2000);
  digitalWrite(LED_PIN, LOW);

  // Comfigure Serial
  Serial.begin(115200);
  delay(50);

  Serial.println("\n/*** Initializing Setup ***/");

  Serial.print("\nConfiguring digital pins... ");
  /* #region   */

  pinMode(PIN_DETECT, INPUT);
  pinMode(PIN_DISPARO_1, OUTPUT);
  pinMode(PIN_DISPARO_2, OUTPUT);
  pinMode(PIN_RELE_1, OUTPUT);
  pinMode(PIN_RELE_2, OUTPUT);
  pinMode(PIN_RELE_3, OUTPUT);
  pinMode(PIN_RELE_4, OUTPUT);

  digitalWrite(PIN_DISPARO_1, LOW); // Inicia o GPIO em nível baixo
  digitalWrite(PIN_DISPARO_2, LOW);
  digitalWrite(PIN_RELE_1, HIGH);
  digitalWrite(PIN_RELE_2, HIGH);
  digitalWrite(PIN_RELE_3, HIGH);
  digitalWrite(PIN_RELE_4, HIGH);

  delay(5); // Wait digital output take effect
  Serial.println("Ok");
  /* #endregion */

  setupWifi_APMode();

  Serial.print("\nInitializing OTA... ");
  /* #region   */
  ArduinoOTA.onStart([]()
                     {
                          detachInterrupt(PIN_DETECT);
                          //timerEnd(timer_100ms);
                          server.stop();
                          uploading = true;
                           String type;
                           if (ArduinoOTA.getCommand() == U_FLASH)
                           {
                               type = "sketch";
                           }
                           else
                           { // U_FS
                               type = "filesystem";
                           }
                           // NOTE: if updating FS this would be the place to unmount FS using FS.end()
                           Serial.println("Start updating " + type); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
                              digitalWrite(LED_PIN, HIGH);
                              Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                              digitalWrite(LED_PIN, LOW); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
                           Serial.printf("Error[%u]: ", error);
                           if (error == OTA_AUTH_ERROR)
                           {
                               Serial.println("Auth Failed");
                           }
                           else if (error == OTA_BEGIN_ERROR)
                           {
                               Serial.println("Begin Failed");
                           }
                           else if (error == OTA_CONNECT_ERROR)
                           {
                               Serial.println("Connect Failed");
                           }
                           else if (error == OTA_RECEIVE_ERROR)
                           {
                               Serial.println("Receive Failed");
                           }
                           else if (error == OTA_END_ERROR)
                           {
                               Serial.println("End Failed");
                           } });
  ArduinoOTA.setHostname(HOST);
  ArduinoOTA.begin();
  Serial.println("OK");
  Serial.printf(".Hostname: %s.local\n", ArduinoOTA.getHostname());
  /* #endregion */

  Serial.print("\nInitializing Web Server...");
  server.begin();
  Serial.println("OK");

  //*
  // Serial.println("Initializing Zero Cross Detector... ");

  // while (digitalRead(PIN_DETECT))
  //   ;
  // while (!digitalRead(PIN_DETECT))
  //   ;
  // micro_rising_edge = micros();
  // while (digitalRead(PIN_DETECT))
  //   ;
  // tempo_pulso = micros() - micro_rising_edge;

  // Serial.print("Tempo de duracao do pulso na entrada ");
  // Serial.println(tempo_pulso);

  //*/
  Serial.print("Initializing ADC ");
  adc.begin();
  Serial.println("OK");

  Serial.print("Initializing Interrupts");
  /* #region   */
  timer_100ms = timerBegin(0, 80, true);
  timerAttachInterrupt(timer_100ms, &ISR_timer_100ms, true);
  timerAlarmWrite(timer_100ms, 100000, true);
  timerAlarmEnable(timer_100ms);

  timer_saida_1 = timerBegin(1, 80, true);
  timerAttachInterrupt(timer_saida_1, &ISR_pulsar_saida_1, true);
  timerAlarmWrite(timer_saida_1, 1000, false);
  timerAlarmEnable(timer_saida_1);
  //*/
  attachInterrupt(digitalPinToInterrupt(PIN_DETECT), ISR_detect_zero, RISING);
  Serial.println("OK");
  /* #endregion */

  Serial.println("Setup Ended");
}

void loop()
{
  ArduinoOTA.handle();
  while (uploading)
  {
    ArduinoOTA.handle();
  };
  serverHandler();

  
  float adcReading = adc.readVoltage(0);
  resistencia = resistanceFromAdc(adcReading);
  temperatura = temperatureFromResistance(resistencia);
  
  float adcReading2 = adc.readVoltage(0);
  resistencia2 = resistanceFromAdc(adcReading);
  temperatura2 = temperatureFromResistance(resistencia);

  if (controleAutomatico)
  {
    if (saidaRele1 && (temperatura >= (setpoint + tolerancia)))
    {
      saidaRele1 = false;
      digitalWrite(PIN_RELE_1, HIGH);
    }
    if (!saidaRele1 && (temperatura <= (setpoint - tolerancia)))
    {
      saidaRele1 = true;
      digitalWrite(PIN_RELE_1, LOW);
    }
  }

  if (flag_100ms)
  {
    flag_100ms = false;
  }

  if (flag_1s)
  {
    pulsosContados = count_detected;
    count_detected = 0;
    tempoAnterior = tempoAtual;
    tempoAtual = millis();
    tempoDif = tempoAtual - tempoAnterior;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    flag_1s = false;
  }
}