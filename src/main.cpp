#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <math.h>
#include <Thermistor.h>
#include <ESP32TimerInterrupt.h>
#include <Nanoshield_ADC.h>

/* #region DEFINES  */

const char *HOST = "esp32";              // Host name to mDNS and SSDP
const char *SSID = "BLINK_CASA_RODRIGO"; // Wifi Network SSID
const char *PASSWORD = "uevd1787";       // Wifi Network Password

#define CONSTANTE_RESISTENCIA_1 10000
#define CONSTANTE_A_TEMP_1 0.001129148
#define CONSTANTE_B_TEMP_1 0.000234125
#define CONSTANTE_C_TEMP_1 0.0000000876741

#define CONSTANTE_RESISTENCIA_2 10000
#define CONSTANTE_A_TEMP_2 0.001129148
#define CONSTANTE_B_TEMP_2 0.000234125
#define CONSTANTE_C_TEMP_2 0.0000000876741

// Configurações do IP fixo.
// Você pode alterar conforme a sua rede.

IPAddress ip(192, 168, 18, 119);
IPAddress gateway(192, 168, 18, 1);
IPAddress subnet(255, 255, 255, 0);

#define PIN_DETECT 4     // GPIO usada para detecção
#define PIN_DISPARO_1 18 // GPIO usada para disparo do TRIAC
#define PIN_DISPARO_2 19
#define PIN_RELE_1 13
#define PIN_RELE_2 12
#define PIN_RELE_3 14
#define PIN_RELE_4 27
#define LED_PIN 2
/* #endregion */

/* #region VARIABLES DECLARATIONS */

bool
    controleManual,
    controleAutomatico,
    uploading = false,
    saidaRele1 = false,
    saidaRele2 = false,
    saidaRele3 = false,
    saidaRele4 = false,
    desligarAutoRefresh = false;

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
    invTemperaturaK,
    invTemperaturaK2,
    temperatura,
    temperatura2,
    setpoint = 20.0,
    tolerancia = 0.5;

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
    micros_count,
    micros_count_ant,
    micros_dif,
    micro_rising_edge,
    tempo_pulso,
    tempo_atraso_total;
/* #endregion */

/* #region CLASS DECLARATIONS */

Nanoshield_ADC adc;

// Cria um servidor na porta 80
WiFiServer server(80);

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

void server_handler()
{
  // Verifica se algum cliente está tentando se conectar
  WiFiClient client = server.available();

  if (client) // se há um cliente...
  {
    // Fazemos a leitura da requisição
    String req = client.readStringUntil('\r');
    // Serial.println(req);

    // A partir daqui, verificamos se a requisição possui algum comando de
    // ajuste de sinal
    if (req.indexOf("acao=0") != -1)
    {
      Serial.println("0%");
    }
    else if (req.indexOf("acao=1") != -1)
    {
      Serial.println("10%");
    }
    else if (req.indexOf("autorefresh=on") != -1)
    {
      desligarAutoRefresh = false;
    }
    else if (req.indexOf("autorefresh=off") != -1)
    {
      desligarAutoRefresh = true;
    }
    else if (req.indexOf("setpoint=") != -1)
    {
      int start_of_number = req.indexOf("setpoint=") + 9;
      int end_of_number = req.indexOf("HTTP") - 1;
      String valor = (req.substring(start_of_number, end_of_number));
      setpoint = valor.toFloat();
    }
    else if (req.indexOf("tolerancia=") != -1)
    {
      int start_of_number = req.indexOf("tolerancia=") + 11;
      int end_of_number = req.indexOf("HTTP") - 1;
      String valor = (req.substring(start_of_number, end_of_number));
      tolerancia = valor.toFloat();
    }

    else if (req.indexOf("controle=Manual") != -1)
    {
      controleAutomatico = false;
      controleManual = true;
    }

    else if (req.indexOf("controle=Automatico") != -1)
    {
      controleAutomatico = true;
      controleManual = false;
    }

    // Este é o html que iremos retornar para o cliente
    //É composto basicamente de botões numerados indicando os níveis de 0% a 100%
    // A parte que nos interessa é o <a href=' com a ação vinculada a cada botão
    // Quando clicamos em um destes botões essa informação chegará até o ESP para
    // que ele verifique qual ação deve executar
    // A parte dentro de '<style>' é apenas para modificarmos o visual da página
    // que será exibida, você pode alterá-la como quiser
    String html_saida;
    if (saidaRele1)
    {
      html_saida = "<p>Saida Ligada</p>";
    }
    else
    {
      html_saida = "<p>Saida Desligada</p>";
    }
    String html_controle;
    if (controleAutomatico)
    {
      html_controle = "<p>Controle Automatico</p>";
    }
    else if (controleManual)
    {
      html_controle = "<p>Controle Manual</p>";
    }
    else
    {
      html_controle = "<p>Controle Desligado</p>";
    }

    String html_refresh;
    if (desligarAutoRefresh)
    {
      html_refresh = "";
    }
    else
    {
      html_refresh = "<meta http-equiv='refresh' content='2;url=http://esp32.local'/>";
    }

    String html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/>" +
        html_refresh +
        "<title>DIMMER WiFi Http</title>"
        "<style>"
        "body{"
        "text-align: center;"
        "font-family: sans-serif;"
        "font-size:25px;"
        "padding: 25px;"
        "}"
        "p{"
        "color:#444;"
        "}"
        "button{"
        "outline: none;"
        "border: 2px solid #1fa3ec;"
        "border-radius:18px;"
        "background-color:#FFF;"
        "color: #1fa3ec;"
        "padding: 5px 25px;"
        "}"
        "button:active{"
        "color: #fff;"
        "background-color:#1fa3ec;"
        "}"
        "button:hover{"
        "border-color:#0000ff;"
        "}"
        "</style>"
        "</head>"
        "<body>" +
        html_saida + html_controle +
        "<p>Atraso Pulso " + String(micros_dif) + "</p>"
                                                  "<p>Resistencia Canal 1: " +
        String(resistencia, 1) + " ohm</p>"
                                 "<p>Temperatura Canal 1: " +
        String(temperatura, 1) + " C</p>"
                                 "<p>Resistencia Canal 2: " +
        String(resistencia2, 1) + " ohm</p>"
                                  "<p>Temperatura Canal 2: " +
        String(temperatura2, 1) + " C</p>"
                                  "<p>Setpoint " +
        String(setpoint, 1) + " C</p>"
                              "<p>Tolerancia " +
        String(tolerancia, 1) + " C</p>"
                                "<p>"
                                "<a href='?autorefresh=on'><button>Ligar Autorefresh</button></a>  "
                                "<a href='?autorefresh=off'><button>Desligar Autorefresh</button></a>"
                                "</p>"
                                "<p>"
                                "<a href='?controle=Manual'><button>Controle Manual</button></a>  "
                                "<a href='?controle=Automatico'><button>Controle Automatico</button></a>"
                                "</p>"
                                "<form>"
                                "<div>"
                                "<label for='setpoint'>Setpoint de Temperatura: </label>"
                                "<input id='setpoint' type='number' name='setpoint' placeholder=" +
        String(setpoint, 1) + " step='0.1' min='0' max='100' required>"
                              "<span class='validity'></span>"
                              "<input type='submit'>"
                              "</div>"
                              "</form>"

                              "<form>"
                              "<div>"
                              "<label for='setpoint'>Tolerancia de Temperatura: </label>"
                              "<input id='tolerancia' type='number' name='tolerancia' placeholder=" +
        String(tolerancia, 1) + " step='0.1' min='0,1' max='100' required>"
                                "<span class='validity'></span>"
                                "<input type='submit'>"
                                "</div>"
                                "</form>"

                                "<p><a href='?acao=0'><button>0%</button></a></p>"
                                "<p><a href='?acao=1'><button>10%</button></a></p>"
                                "</body>"
                                "</html>";
    /*

    */
    // Escreve o html no buffer que será enviado para o cliente
    client.print(html);
    // Envia os dados do buffer para o cliente
    // client.flush();
    client.stop(); // descomente para liberar a conexão com o navegador
  }
};

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

  Serial.println("\nInitializing Wifi");
  /* #region   */

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".Trying to connect");
    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(150);
      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);
      Serial.print(".");
    }
    Serial.println(" Connected");
  }
  else
  {
    Serial.println(".Already Connected");
  }

  Serial.print(".SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print(".IP ADDRESS: ");
  Serial.println(WiFi.localIP());
  Serial.print(".MAC ADDRESS: ");
  Serial.println(WiFi.macAddress());
  /* #endregion */

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
    Serial.println("Initializing Zero Cross Detector... ");

    while(digitalRead(PIN_DETECT));
    while(!digitalRead(PIN_DETECT));
    micro_rising_edge = micros ();
    while(digitalRead(PIN_DETECT));
    tempo_pulso = micros()-micro_rising_edge;

    Serial.print("Tempo de duracao do pulso na entrada ");
    Serial.println(tempo_pulso);
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
  server_handler();

  leituraVcc = adc.readVoltage(0);
  if (leituraVcc > 3.28) leituraVcc = 2;
  resistencia = CONSTANTE_RESISTENCIA_1 * leituraVcc / (3.29 - leituraVcc);
  invTemperaturaK = CONSTANTE_A_TEMP_1 + CONSTANTE_B_TEMP_1 * log(resistencia) + CONSTANTE_C_TEMP_1 * pow(log(resistencia), 3);
  if (invTemperaturaK != 0.0) temperatura = -273.15 + 1.0 / invTemperaturaK;

  leituraVcc2 = adc.readVoltage(1);
  if (leituraVcc2 > 3.28) leituraVcc2 = 2;
  resistencia2 = CONSTANTE_RESISTENCIA_2 * leituraVcc2 / (3.29 - leituraVcc2);
  invTemperaturaK2 = CONSTANTE_A_TEMP_2 + CONSTANTE_B_TEMP_2 * log(resistencia2) + CONSTANTE_C_TEMP_2 * pow(log(resistencia2), 3);
  if (invTemperaturaK2 != 0.0) temperatura2 = -273.15 + 1.0 / invTemperaturaK2;

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