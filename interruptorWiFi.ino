#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

// Configuracoes do MQTT
#define mqtt_server "192.168.0.95"
#define mqtt_user "admin"
#define mqtt_password "admin"

#define board_topic "board/state"
#define temperature_topic "sensor/temperature"

WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "TECNEQUIP";
const char* password = "Bauru2015";

const int led    =  4; //led GPIO4
const int botao1 = 13;
const int botao2 = 12;
const int botao3 = 14;

struct strBoardState {
  // Indica se o led esta ligado
  int isLedON;

  // Indica se o interruptor da Web esta ligado
  int isWebSwitchON;

  // Estado dos paralelos;
  union {
    int value;
    struct {
      int isP1ON : 1;
      int isP2ON : 1;
      int isP3ON : 1;
    } bits;
  } parallel;
} boardState;

ESP8266WebServer server ( 80 );

const char temp[] = "<html>\
<head>\
<style type='text/css'>\
        .sensor {\
            float: left;\
            margin: 5px;\
            padding: 10px;\
            width: 300px;\
            height: 300px;\
            border: 2px solid black;\
        }\
        h1   {\
            line-height: 0.6;\
            color: white;\
            font-family:Arial\
        }\
        h2   {\
            line-height: 0.1;\
            color: white;\
            font-family:Arial\
        }\
        p    {\
            color: white;\
            font-family:Arial\
        }\
        html,body {\
            padding: 0;\
            margin: 0;\
            background-color: #3FBFEF;\
            font-family:Arial\
        }\
        header {\
            position:fixed;\
            top:0;\
            width:100%;\
            height:30px;\
            background-color:#333;\
            padding:10px;\
        }\
        footer {\
            background-color: #333;\
            width: 100%;\
            bottom: 0;\
            position: relative;\
        }\
        #main{\
            padding-top:50px;\
            text-align:center;\
        }\
        .buttonON {\
            background-color: #4CAF50;\
            border: none;\
            color: white;\
            width: 100px;\
            padding: 15px 32px;\
            text-align: center;\
            text-decoration: none;\
            display: inline-block;\
            font-size: 20px;\
            margin: 4px 2px;\
            cursor: pointer;\
            border-radius: 12px;\
            box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19);\
        }\
        .buttonOFF {\
            background-color: #F44336;\
            border: none;\
            color: white;\
            width: 100px;\
            padding: 15px 32px;\
            text-align: center;\
            text-decoration: none;\
            display: inline-block;\
            font-size: 20px;\
            margin: 4px 2px;\
            cursor: pointer;\
            border-radius: 12px;\
            box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19);\
        }\
       .buttonON:hover {background-color: #3e8e41}\
       .buttonON:active {\
            background-color: #3e8e41;\
            box-shadow: 0 2px #666;\
            transform: translateY(4px);\
       }\
       .buttonOFF:hover {background-color: #D53520}\
       .buttonOFF:active {\
            background-color: #F44336;\
            box-shadow: 0 2px #666;\
            transform: translateY(4px);\
       }\
</style>\
<script type='text/javascript'>\
var xmlHttp=createXmlHttpObject();\n\
function checkMQTT() {\
    if(myCheck.value == 0){\
       document.getElementById(\"IPMQTT\").disabled = true;\
       document.getElementById(\"USUARIOMQTT\").disabled = true;\
       document.getElementById(\"SENHAMQTT\").disabled = true;\
       myCheck.value=1;\
    } else {\
       document.getElementById(\"IPMQTT\").disabled = false;\
       document.getElementById(\"USUARIOMQTT\").disabled = false;\
       document.getElementById(\"SENHAMQTT\").disabled = false;\
       myCheck.value=0;\
    }\
}\
function ValidateIPaddress(inputText){  \
  var ipformat = /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;  \
  if(inputText.value.match(ipformat)){  \
     document.form.text1.focus();  \
     return true;  \
  }else{  \
     alert(\"IP invalido!\");  \
     document.form.text1.focus(); return false;  \
  }  \
}\
function boardSendData(params,fn)\
{\
    var request = createXmlHttpObject();\
    request.onreadystatechange = fn;\
    request.open('POST', 'update', true);\
    request.setRequestHeader(\"Content-type\", \"application/x-www-form-urlencoded\");\
    request.setRequestHeader(\"Content-length\", params.length);\
    request.setRequestHeader(\"Connection\", \"close\");\
    request.send(params);\
}\
function mudaBotao(nome) {\
    var params;\
    var botao = document.getElementById(nome);\
    if(botao.value == \"ON\") {\
       params=\"led=0\";\
    } else {\
       params=\"led=1\";\
    }\
    boardSendData(params,null);\
}\
function createXmlHttpObject(){\n\
  if(window.XMLHttpRequest){\n\
    xmlHttp=new XMLHttpRequest();\n\
  }else{\n\
    xmlHttp=new ActiveXObject('Microsoft.XMLHTTP');\n\
  }\n\
  return xmlHttp;\n\
}\n\
function boardProcess()\
{\
   if(xmlHttp.readyState==0 || xmlHttp.readyState==4){\n\
     xmlHttp.open('PUT','xml',true);\n\
     xmlHttp.onreadystatechange=handleServerResponse;\n\
     xmlHttp.send(null);\n\
   }\n\
   setTimeout('boardProcess()',1000);\n\
}\n\
function handleServerResponse(){\n\
   var color = [ 'white', 'yellow' ];\
   if(xmlHttp.readyState==4 && xmlHttp.status==200){\n\
     xmlResponse=xmlHttp.responseXML;\n\
     xmldoc = xmlResponse.getElementsByTagName('led');\n\
     document.getElementById(\"sala_circle\").style.fill=color[xmldoc[0].firstChild.nodeValue];\
     xmlResponse=xmlHttp.responseXML;\n\
     xmldoc = xmlResponse.getElementsByTagName('switch');\n\
     var botao = document.getElementById('sala');\
     if(xmldoc[0].firstChild.nodeValue == 0) {\
       botao.value = \"OFF\";\
       botao.className = \"buttonOFF\";\
     } else {\
       botao.value = \"ON\";\
       botao.className = \"buttonON\";\
     }\
   }\n\
}\n\
</script>\
</head>\
<body onload='boardProcess()'>\
<br/>\
<h1>Configura&ccedil;&otilde;es</h1>\
<div class=\"sensor\">\
  <h1><b>Lampada</b></h1>\
  <p><input id='sala' type=\"button\" class=\"buttonON\" value=\"ON\" onclick=\"mudaBotao('sala')\">  28&#176;C</p>\
  <p><svg height=\"66\" width=\"66\">\
  <circle id=\"sala_circle\" cx=\"33\" cy=\"33\" r=\"30\" stroke=\"black\" stroke-width=\"3\"  fill=\"yellow\"/>\
  Sorry, your browser does not support inline SVG.  \
  </svg></p>\
<form action=\"/\" method='post'>\
  Desligar<br>\
  <input type=\"text\" name=\"led\" value=\"\" maxlength=\"4\">\
  minutos<br>\
  <input type=\"submit\" id=\"APLICAR\" value=\"Aplicar\">\
</form>\
</div>\
<div class=\"sensor\">\
  <h2><b>WI-FI Configura&ccedil;&atilde;o</b></h2>\
     <form action=\"action_page.php\">\
     <input type=\"text\" name=\"redewifi\" value=\"TECNEQUIP\" maxlength=\"12\">\
     Rede Wi-FI\
     <br>\
     <input type=\"password\" name=\"senhawifi\" value=\"psw\" maxlength=\"12\" autocomplete=\"off\">\
     Senha:\
     <br><br>\
     <input onclick=\"checkMQTT()\" type=\"checkbox\" id=\"myCheck\" name=\"MQTT\" value=1> \
     Conectar com Servidor MQTT?<br>\
     <br>\
     <input type=\"text\" id=\"IPMQTT\" name=\"IPMQTT\" value=\"192.168.0.95\" disabled maxlength=\"15\">\
     IP do Servidor:\
     <br>\
     <input type=\"text\" id=\"USUARIOMQTT\" name=\"USUARIOMQTT\" value=\"admin\" disabled maxlength=\"12\">\
     MQTT Usu&aacute;rio:\
     <br>\
     <input type=\"password\" id=\"SENHAMQTT\" name=\"SENHAMQTT\" value=\"test\" disabled maxlength=\"12\" autocomplete=\"off\">\
     Senha:\
     <br><br>\
     <input type=\"submit\" id=\"APLICAR_MQTT\" value=\"Aplicar\" onclick=\"ValidateIPaddress(document.form.IPMQTT)\">\
     </form>\
     <br>\
     Conectado no IP:\
</div>\
</body>\
</html>";

void handleRoot() {
  char estado[][10] = { "Ligado", "Desligado" };
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  // Primeiro atualizamos o estado da placa, conforme parametros recebidos
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if (server.argName(i) == "led") {
      boardState.isWebSwitchON = (atoi(server.arg(i).c_str()) != 0);
    } else if (server.argName(i) == "p2") {
      boardState.parallel.bits.isP2ON = (atoi(server.arg(i).c_str()) != 0);
    } else if (server.argName(i) == "p3") {
      boardState.parallel.bits.isP3ON = (atoi(server.arg(i).c_str()) != 0);
    }
  }

  // Agora exibe a pagina
  server.send ( 200, "text/html", temp );
}

void handleUpdate() {
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if (server.argName(i) == "led") {
      boardState.isWebSwitchON = (atoi(server.arg(i).c_str()) != 0);
    }
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}

String buildXML()
{
  String xml = "<?xml version='1.0'?><start>";
  xml += "<led>";
  xml += boardState.isLedON;
  xml += "</led>";
  xml += "<switch>";
  xml += boardState.isWebSwitchON;
  xml += "</switch>";
  xml += "</start>";

  return xml;
}

void handleXML() {
  server.send(200, "text/xml", buildXML());
}

void setup ( void ) {
  pinMode ( led, OUTPUT );
  pinMode ( botao1, INPUT_PULLUP );
  pinMode ( botao2, INPUT_PULLUP );
  pinMode ( botao3, INPUT_PULLUP );
  digitalWrite ( led, 0 );

  Serial.begin ( 115200 );
  WiFi.begin ( ssid, password );
  Serial.println ( "" );

  // Wait for connection
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  Serial.println ( "" );
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );

  if ( MDNS.begin ( "esp8266" ) ) {
    Serial.println ( "MDNS responder started" );
  }

  server.on ( "/", handleRoot );
  server.on ( "/xml", handleXML );
  server.on ( "/update", handleUpdate );
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.onNotFound ( handleNotFound );
  server.begin();
  Serial.println ( "HTTP server started" );

  // Inicializa o estado da placa
  boardState.isLedON         = 0;
  boardState.isWebSwitchON   = 0;
  boardState.parallel.bits.isP1ON = 0;
  boardState.parallel.bits.isP2ON = 0;
  boardState.parallel.bits.isP3ON = 0;

  //  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop ( void ) {
  static int initOK = 0;
  static struct strBoardState lastBoardState = boardState;

  server.handleClient();

  /* if (!client.connected()) {
     reconnect();
    }
    client.loop();*/

  boardState.parallel.bits.isP1ON = (digitalRead(botao1) != 0);   // turn the LED on (HIGH is the voltage level)
  boardState.parallel.bits.isP2ON = (digitalRead(botao2) != 0);   // turn the LED on (HIGH is the voltage level)
  boardState.parallel.bits.isP3ON = (digitalRead(botao3) != 0);   // turn the LED on (HIGH is the voltage level)

  // Nao calcula a saida no primeiro loop
  if (initOK) {
    // Se houve alteracao na chave Web, o estado dela representa o estado do led
    // Se houve alteracao no estado do paralelo, inverte o estado do led. Mas se a chave Web estiver ativa, mantem o led ativo.
    if (boardState.isWebSwitchON != lastBoardState.isWebSwitchON) {
      boardState.isLedON = boardState.isWebSwitchON;
    } else if (boardState.parallel.value != lastBoardState.parallel.value) {
      boardState.isLedON = !boardState.isLedON || boardState.isWebSwitchON;
    }
  } else {
    initOK = 1;
  }

  digitalWrite(led, boardState.isLedON);    // turn the LED off by making the voltage LOW

  if ( (boardState.isWebSwitchON != lastBoardState.isWebSwitchON) || (boardState.parallel.value != lastBoardState.parallel.value) ) {
    client.publish(board_topic, "teste", true);
  }

  lastBoardState = boardState;
}

