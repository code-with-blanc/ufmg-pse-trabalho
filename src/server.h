#if !defined(_SERVER_H_)
#define _SERVER_H_

#include "hardware.h"
#include "control.h"


const char *HOST = "esp32";              // Host name to mDNS and SSDP
const char *SSID = "BLINK_CASA_RODRIGO"; // Wifi Network SSID
const char *PASSWORD = "uevd1787";       // Wifi Network Password

IPAddress ip(192, 168, 18, 119);
IPAddress gateway(192, 168, 18, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiServer server(80);

extern control_info_t control_info;
extern sensor_info_t sensor_info;

bool desligarAutoRefresh = false;

// data to be printed from zero crossing
extern volatile unsigned long micros_dif;


void setupWifi_APMode()
{
    Serial.println("\n[*] Creating AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Brassagem");
    Serial.print("[+] AP Created with IP Gateway ");
    Serial.println(WiFi.softAPIP());
}

void setupWifi_StationMode()
{
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
};

void serverHandler()
{
    // Verifica se algum cliente está tentando se conectar
    WiFiClient client = server.available();

    if (client) // se há um cliente...
    {
        // Fazemos a leitura da requisição
        String req = client.readStringUntil('\r');

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
            control_info.setpoint = valor.toFloat();
        }
        else if (req.indexOf("tolerancia=") != -1)
        {
            int start_of_number = req.indexOf("tolerancia=") + 11;
            int end_of_number = req.indexOf("HTTP") - 1;
            String valor = (req.substring(start_of_number, end_of_number));
            control_info.tolerancia = valor.toFloat();
        }

        else if (req.indexOf("controle=Manual") != -1)
        {
            control_info.automatic_control = false;
        }

        else if (req.indexOf("controle=Automatico") != -1)
        {
            control_info.automatic_control = true;
        }

        // Este é o html que iremos retornar para o cliente
        // É composto basicamente de botões numerados indicando os níveis de 0% a 100%
        // A parte que nos interessa é o <a href=' com a ação vinculada a cada botão
        // Quando clicamos em um destes botões essa informação chegará até o ESP para
        // que ele verifique qual ação deve executar
        // A parte dentro de '<style>' é apenas para modificarmos o visual da página
        // que será exibida, você pode alterá-la como quiser
        
        String html_saida = "<p> TODO: reportar dado de saída para SSR </p>";
        
        String html_controle;
        if (control_info.automatic_control)
        {
            html_controle = "<p>Controle Automatico: Ligado</p>";
        }
        else
        {
            html_controle = "<p>Controle Automatico: Desligado</p>";
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
            "<p>Resistencia Canal 1: " + String(sensor_info.resistencia, 1) + " ohm</p>"
            "<p>Temperatura Canal 1: " + String(sensor_info.temperatura, 1) + " C</p>"
            "<p>Setpoint " + String(control_info.setpoint, 1) + " C</p>"
            "<p>Tolerancia " + String(control_info.tolerancia, 1) + " C</p>"
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
               "<input id='setpoint' type='number' name='setpoint' placeholder="
                   + String(control_info.setpoint, 1) + " step='0.1' min='0' max='100' required>"
               "<span class='validity'></span>"
               "<input type='submit'>"
               "</div>"
            "</form>"

            "<form>"
              "<div>"
                  "<label for='setpoint'>Tolerancia de Temperatura: </label>"
                  "<input id='tolerancia' type='number' name='tolerancia' placeholder="
                    + String(control_info.tolerancia, 1) + " step='0.1' min='0,1' max='100' required>"
                  "<span class='validity'></span>"
                  "<input type='submit'>"
              "</div>"
            "</form>"

            "<p><a href='?acao=0'><button>0%</button></a></p>"
            "<p><a href='?acao=1'><button>10%</button></a></p>"
            "</body>"
            "</html>";
        
        // Escreve o html no buffer que será enviado para o cliente
        client.print(html);
        // Envia os dados do buffer para o cliente
        // client.flush();
        client.stop(); // descomente para liberar a conexão com o navegador
    }
};

#endif // _SERVER_H_
