#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

/* Configuracoes do MQTT*/
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

struct strLampState {
  /* Estado da lampada*/
  int isON;

  /* Indica se esta forcando nivel alto ou baixo*/
  int isForceON;
  int isForceOFF;

  /* Estado dos paralelos;*/
  union {
    int value; // Valor que agrega o estado de todos os paralelos
    struct {
      int isWebON: 1; // Interruptor ativado pela Web (Browser)
      int isP1ON : 1; // Primeiro paralelo fisico
      int isP2ON : 1; // Segundo paralelo fisico
      int isP3ON : 1; // Terceiro paralelo fisico
    } bits;
  } parallel;
  int last_parallel; // Estado do ultimo valor para o paralelo

  /* Timers para ligar / desligar a lampada*/
  int tmrON; // Tempo restante ate acionamento
  int tmrOFF; // Tempo restante ate desligamento
};

void lampInit(struct strLampState &lamp)
{
  memset(&lamp, 0, sizeof(lamp));
}

void lampUpdateOutput(struct strLampState &lamp, bool updateTimers)
{
  bool reachedTimerON = false, reachedTimerOFF = false;

  /* Atualiza os timers.*/
  if (updateTimers) {
    /* Primeiro checa timer para desligar*/
    if (lamp.tmrOFF > 0) {
      lamp.tmrOFF--;
      if (lamp.tmrOFF == 0) {
        reachedTimerOFF = true;
      }
    }

    /* Agora checa timer para ligar*/
    if (lamp.tmrON > 0) {
      lamp.tmrON--;
      if (lamp.tmrON == 0) {
        reachedTimerON = true;
      }
    }
  }

  // Se variaveis para forcar liga ou desliga estiverem ativadas, configura o estado da saida de acordo
  // Se houve alteracao no estado do paralelo, inverte o estado da lampada.
  if (lamp.isForceON || reachedTimerON) {
    lamp.isON = 1;
  } else if (lamp.isForceOFF || reachedTimerOFF) {
    lamp.isON = 0;
  } else if (lamp.parallel.value != lamp.last_parallel) {
    lamp.isON = !lamp.isON;
  }

  lamp.last_parallel = lamp.parallel.value;
}

struct strBoardState {
  struct strLampState lamp[2]; // A placa suporta 2 lampadas
} boardState;

ESP8266WebServer server ( 80 ); /*server: http://192.168.4.1 */

const char temp[] = "<!DOCTYPE HTML><html> <head> <title>POP-0</title> <style type='text/css'> .sensor{float: left; margin: 5px; padding: 10px; width: 300px; height: 300px; border: 2px solid black;}.middle{vertical-align: middle}.timer{float: left; margin: 5px; padding: 10px; width: 100px; height: 145px;}h1{line-height: 0.6; color: white; font-family: Arial}h2{line-height: 0.1; color: white; font-family: Arial}p{color: white; font-family: Arial}html, body{padding: 0; margin: 0; background-color: #3FBFEF; font-family: Arial}header{position: fixed; top: 0; width: 100%; height: 30px; background-color: #333; padding: 10px;}footer{background-color: #333; width: 100%; bottom: 0; position: relative;}#main{padding-top: 50px; text-align: center;}.buttonON{background-color: #4CAF50; border: none; color: white; width: 100px; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 20px; margin: 4px 2px; cursor: pointer; border-radius: 12px; box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(0, 0, 0, 0.19);}.buttonOFF{background-color: #F44336; border: none; color: white; width: 100px; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 20px; margin: 4px 2px; cursor: pointer; border-radius: 12px; box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(0, 0, 0, 0.19);}.buttonON:hover{background-color: #3e8e41}.buttonON:active{background-color: #3e8e41; box-shadow: 0 2px #666; transform: translateY(4px);}.buttonOFF:hover{background-color: #D53520}.buttonOFF:active{background-color: #F44336; box-shadow: 0 2px #666; transform: translateY(4px);}</style> <script type='text/javascript'> var xmlHttp=createXmlHttpObject(); function checkMQTT(){if (myCheck.value==0){document.getElementById(\"IPMQTT\").disabled=true; document.getElementById(\"USUARIOMQTT\").disabled=true; document.getElementById(\"SENHAMQTT\").disabled=true; myCheck.value=1;}else{document.getElementById(\"IPMQTT\").disabled=false; document.getElementById(\"USUARIOMQTT\").disabled=false; document.getElementById(\"SENHAMQTT\").disabled=false; myCheck.value=0;}}function ValidateIPaddress(inputText){var ipformat=/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?).(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?).(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?).(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/; if (inputText.value.match(ipformat)){document.form.text1.focus(); return true;}else{alert(\"IP invalido!\"); document.form.text1.focus(); return false;}}function boardSendData(params, fn){var request=createXmlHttpObject(); request.onreadystatechange=fn; request.open('POST', 'update', true); request.setRequestHeader(\"Content-type\", \"application/x-www-form-urlencoded\"); request.setRequestHeader(\"Content-length\", params.length); request.setRequestHeader(\"Connection\", \"close\"); request.send(params);}function boardSetTimer(tag){var params; var txt=document.getElementById(\"timer\" + tag); if(txt.value >=0){params=tag + \"=\" + (txt.value * 60); boardSendData(params, null);}txt.value=\"0\";}function mudaBotao(nome){boardSendData(\"led=-1\", null);}function createXmlHttpObject(){if (window.XMLHttpRequest){xmlHttp=new XMLHttpRequest();}else{xmlHttp=new ActiveXObject('Microsoft.XMLHTTP');}return xmlHttp;}function boardProcess(){if (xmlHttp.readyState==0 || xmlHttp.readyState==4){xmlHttp.open('PUT', 'json', true); xmlHttp.onreadystatechange=handleServerResponse; xmlHttp.send(null);}setTimeout('boardProcess()', 1000);}function handleServerResponse(){var color=['white', 'yellow']; if (xmlHttp.readyState==4 && xmlHttp.status==200){var jObject=JSON.parse(xmlHttp.responseText); document.getElementById('boardIP' ).innerHTML=jObject.board.ip; document.getElementById('boardNetwork').innerHTML=jObject.board.network; document.getElementById('sala_circle' ).style.fill=color[jObject.led]; if(jObject.tmrON > 0){var secs=(jObject.tmrON%60); if(secs < 10){secs=\"0\" + secs;}var textRemaining=Math.floor(jObject.tmrON/60) + \":\" + secs; document.getElementById('remainingTimerON').innerHTML=textRemaining; document.getElementById('showTimerON').style.display='inline'; document.getElementById('setTimerON' ).style.display='none'; document.getElementById('svgTimerON' ).style.fill='red';}else{document.getElementById('showTimerON').style.display='none'; document.getElementById('setTimerON' ).style.display='inline'; document.getElementById('svgTimerON' ).style.fill='black';}if(jObject.tmrOFF > 0){var secs=(jObject.tmrOFF%60); if(secs < 10){secs=\"0\" + secs;}var textRemaining=Math.floor(jObject.tmrOFF/60) + \":\" + secs; document.getElementById('remainingTimerOFF').innerHTML=textRemaining; document.getElementById('showTimerOFF').style.display='inline'; document.getElementById('setTimerOFF' ).style.display='none'; document.getElementById('svgTimerOFF' ).style.fill='red';}else{document.getElementById('showTimerOFF').style.display='none'; document.getElementById('setTimerOFF' ).style.display='inline'; document.getElementById('svgTimerOFF' ).style.fill='black';}var botao=document.getElementById('sala'); if (jObject.led !=0){botao.value=\"OFF\"; botao.className=\"buttonOFF\";}else{botao.value=\"ON\"; botao.className=\"buttonON\";}}}</script> </head> <body onload='boardProcess()'> <br/> <h1>Configura&ccedil;&otilde;es</h1> <div class=\"sensor\"> <h1><b>Lampada</b></h1> <div> <input id='sala' type=\"button\" class=\"buttonON\" value=\"ON\" onclick=\"mudaBotao('sala')\" style=\"vertical-align:top;\"> <svg height=\"66\" width=\"66\"> <circle id=\"sala_circle\" cx=\"33\" cy=\"33\" r=\"30\" stroke=\"black\" stroke-width=\"3\" fill=\"yellow\"/> </svg> <\/div><div class='timer'> <svg id=\"svgTimerON\" height=\"66\" width=\"66\" fill=\"black\" enable-background=\"new 0 0 87.506 100\" viewBox=\"0 0 87.506 100\" xml:space=\"preserve\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"m43,26c-1.656,0-3,1.343-3,3v28c0,1.657 1.344,3 3,3s3-1.343 3-3v-28c0-1.657-1.344-3-3-3z\"></path> <path d=\"m87.506,23.807c0,0-1.414-4.242-4.243-7.07s-7.07-4.243-7.07-4.243l-8.988,8.988c-4.797-3.28-10.289-5.606-16.205-6.724v-12.758c0,0-4-2-8-2s-8,2-8,2v12.758c-19.898,3.762-35,21.266-35,42.242 0,23.71 19.29,43 43,43s43-19.29 43-43c0-8.971-2.765-17.305-7.482-24.205l8.988-8.988zm-44.506,70.193c-20.402, 0-37-16.598-37-37s16.598-37 37-37 37,16.598 37,37-16.598,37-37,37z\"></path> </svg><br/> <div id='setTimerON' >Ligar em<br/> <input id='timeron' type=\"text\" name=\"led\" value=\"\" maxlength=\"4\" size='9'/> <br/>minutos<br/> <input type=\"submit\" value=\"Aplicar\" onclick=\"boardSetTimer('on')\"/><\/div><div id='showTimerON' style=\"display:none\">Liga em<br/> <a id='remainingTimerON'></a> <br/>minutos<br/> <input type=\"submit\" value=\"Cancelar\" onclick=\"boardSetTimer('on')\"/><\/div><\/div><div class='timer'> <svg id=\"svgTimerOFF\" height=\"66\" width=\"66\" fill=\"black\" enable-background=\"new 0 0 87.506 100\" viewBox=\"0 0 87.506 100\" xml:space=\"preserve\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"m43,26c-1.656,0-3,1.343-3,3v28c0,1.657 1.344,3 3,3s3-1.343 3-3v-28c0-1.657-1.344-3-3-3z\"></path> <path d=\"m87.506,23.807c0,0-1.414-4.242-4.243-7.07s-7.07-4.243-7.07-4.243l-8.988,8.988c-4.797-3.28-10.289-5.606-16.205-6.724v-12.758c0,0-4-2-8-2s-8,2-8,2v12.758c-19.898,3.762-35,21.266-35,42.242 0,23.71 19.29,43 43,43s43-19.29 43-43c0-8.971-2.765-17.305-7.482-24.205l8.988-8.988zm-44.506,70.193c-20.402, 0-37-16.598-37-37s16.598-37 37-37 37,16.598 37,37-16.598,37-37,37z\"></path> </svg><br/> <div id='setTimerOFF' >Desligar em<br/> <input id='timeroff' type=\"text\" name=\"led\" value=\"\" maxlength=\"4\" size='9'/> <br/>minutos<br/> <input type=\"submit\" value=\"Aplicar\" onclick=\"boardSetTimer('off')\"/><\/div><div id='showTimerOFF' style=\"display:none\">Desliga em<br/> <a id='remainingTimerOFF'></a> <br/>minutos<br/> <input type=\"submit\" value=\"Cancelar\" onclick=\"boardSetTimer('off')\"/><\/div><\/div><\/div><div class=\"sensor\"> <h2><b>Configura&ccedil;&atilde;o do WI-FI</b></h2> <form action=\"action_page.php\"> <input type=\"text\" name=\"redewifi\" value=\"TECNEQUIP\" maxlength=\"12\"/> Rede Wi-FI <br/> <input type=\"password\" name=\"senhawifi\" value=\"psw\" maxlength=\"12\" autocomplete=\"off\"/> Senha: <br/><br/> <input type=\"checkbox\" id=\"myCheck\" name=\"MQTT\" value=1 onclick=\"checkMQTT()\"/> Servidor de Monitoramento? <br/> <br/> <input type=\"text\" id=\"IPMQTT\" name=\"IPMQTT\" value=\"192.168.0.95\" disabled maxlength=\"15\"/> IP do Servidor: <br/> <input type=\"text\" id=\"USUARIOMQTT\" name=\"USUARIOMQTT\" value=\"admin\" disabled maxlength=\"12\"/> Usu&aacute;rio: <br/> <input type=\"password\" id=\"SENHAMQTT\" name=\"SENHAMQTT\" value=\"test\" disabled maxlength=\"12\" autocomplete=\"off\"/> Senha: <br/> <br/> <input type=\"submit\" id=\"APLICAR_MQTT\" value=\"Aplicar\" onclick=\"ValidateIPaddress(document.form.IPMQTT)\"/> </form> Conectado: <a id='boardNetwork'></a> <br/> IP:<a id='boardIP'></a> <\/div></body></html>";

void handleRoot() {
  char estado[][10] = { "Ligado", "Desligado" };
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  /* Primeiro atualizamos o estado da placa, conforme parametros recebidos*/
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if (server.argName(i) == "led") {
      boardState.lamp[0].parallel.bits.isWebON = (atoi(server.arg(i).c_str()) != 0);
    }
  }

  /* Agora exibe a pagina */
  server.send ( 200, "text/html", temp );
}

void handleUpdate() {
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if (server.argName(i) == "led") {
      int valueLed = atoi(server.arg(i).c_str());
      if(valueLed < 0) {
        boardState.lamp[0].parallel.bits.isWebON = !boardState.lamp[0].parallel.bits.isWebON;
      } else if(valueLed == 0) {
        boardState.lamp[0].parallel.bits.isWebON = 0;
      } else {
        boardState.lamp[0].parallel.bits.isWebON = 1;
      }
    } else if (server.argName(i) == "on") {
      boardState.lamp[0].tmrON = atoi(server.arg(i).c_str());
    } else if (server.argName(i) == "off") {
      boardState.lamp[0].tmrOFF = atoi(server.arg(i).c_str());
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

String buildJSON()
{
  String xml, sIP;
  IPAddress ip = WiFi.localIP();
  sIP  = ip[0];
  sIP += ".";
  sIP += ip[1];
  sIP += ".";
  sIP += ip[2];
  sIP += ".";
  sIP += ip[3];

  xml = "{\"board\":{\"ip\":\"";
  xml += sIP;
  xml += "\",\"network\":\"";
  xml += ssid;
  xml += "\"},\"led\":";
  xml += boardState.lamp[0].isON;
  xml += ",\"switch\":";
  xml += boardState.lamp[0].parallel.bits.isWebON;
  xml += ",\"tmrON\":";
  xml += boardState.lamp[0].tmrON;
  xml += ",\"tmrOFF\":";
  xml += boardState.lamp[0].tmrOFF;
  xml += "}";

  return xml;
}

void handleJSON() {
  server.send(200, "text/json", buildJSON());
}

void setup ( void ) {
  IPAddress myIP;

  pinMode ( led, OUTPUT );
  pinMode ( botao1, INPUT_PULLUP );
  pinMode ( botao2, INPUT_PULLUP );
  pinMode ( botao3, INPUT_PULLUP );
  digitalWrite ( led, 0 );

  Serial.begin ( 115200 );
  Serial.println ( "" );

  if (true) {
    WiFi.begin ( ssid, password );
    /* Espera pela conexão*/
    while ( WiFi.status() != WL_CONNECTED ) {
      delay ( 500 );
      Serial.print ( "." );
    }
    myIP = WiFi.localIP();
  } else {
    WiFi.mode(WIFI_AP); //aceita WIFI_AP / WIFI_AP_STA / WIFI_STA
    WiFi.softAP(ssid, password);
    myIP = WiFi.softAPIP();
  }

  Serial.println ( "" );
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
  Serial.print ( "IP address: " );
  Serial.println ( myIP );

  if ( MDNS.begin ( "esp8266" ) ) {
    Serial.println ( "MDNS responder started" );
  }

  server.on ( "/", handleRoot );
  server.on ( "/json", handleJSON );
  server.on ( "/update", handleUpdate );
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.onNotFound ( handleNotFound );
  server.begin();
  Serial.println ( "HTTP server started" );

  /* Inicializa o estado da placa*/
  lampInit(boardState.lamp[0]);

  client.setServer(mqtt_server, 1883);
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
      /*Espera 5s para reconectar*/
      delay(5000);
    }
  }
}

void loop ( void ) {
  bool updateTimer = false;

  unsigned long currms;
  static unsigned long lastms = 0;

  server.handleClient();

  if (!client.connected()) {
     reconnect();
    }
  client.loop();

  boardState.lamp[0].parallel.bits.isP1ON = (digitalRead(botao1) != 0);   // turn the LED on (HIGH is the voltage level)
  boardState.lamp[0].parallel.bits.isP2ON = (digitalRead(botao2) != 0);   // turn the LED on (HIGH is the voltage level)
  boardState.lamp[0].parallel.bits.isP3ON = (digitalRead(botao3) != 0);   // turn the LED on (HIGH is the voltage level)

  /*Verifica se passou o inervalo de 1 segundo desde a ultima atualizacao do timer*/
  currms = millis();
  if (currms < lastms || (currms - lastms) >= 1000) {
    updateTimer = true;
    lastms = currms;
    client.publish(board_topic, buildJSON().c_str(), true);
  }

  /*Se passou 1 segundo os timers serao atualizados e, se houver atingido o valor programado, a saida sera configurada de acordo.*/
  lampUpdateOutput(boardState.lamp[0], updateTimer);

  digitalWrite(led, boardState.lamp[0].isON);    /*Turn the LED off by making the voltage LOW*/
}

