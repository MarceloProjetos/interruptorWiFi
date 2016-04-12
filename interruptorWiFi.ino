#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FS.h>

#define SERIAL_VERBOSE

/* Configuracoes do MQTT*/
#define mqtt_server "192.168.0.95"
#define mqtt_user "admin"
#define mqtt_password "admin"
#define mqtt_timeout (60000)

#define board_topic "board/state"
#define board_commands_topic "board/commands"
#define temperature_topic "sensor/temperature"

#define file_config_wifi "config_wifi.txt"

WiFiClient espClient;
PubSubClient client(espClient);

#define DEFAULT_WIFI_SSID   "TEC_POP_0"
#define DEFAULT_WIFI_PASSWD "tec_pop_0"

String ssid     = DEFAULT_WIFI_SSID;
String password = DEFAULT_WIFI_PASSWD;

const int led    =  4; //led GPIO4
const int led2    =  5; //led GPIO5
const int botao1 = 13;
const int botao2 = 12;
const int botao3 = 14;
const int resetconfig = 16;

ESP8266WebServer server ( 80 ); /*server: http://192.168.4.1 */

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

struct strBoardState {
  /* Estado dos dispositivos */
  bool isWiFiAP;

  /* Configuração do MQTT */
  struct {
    bool   enable;
    String ip;
    String user;
    String password;
  } mqtt;

  /* Estado das Lampadas */
  struct strLampState lamp[2]; // A placa suporta 2 lampadas
} boardState;

void lampInit(struct strLampState &lamp)
{
  memset(&lamp, 0, sizeof(lamp));
  boardState.lamp[0].isON = true;
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

void boardReset(void)
{
  int i = 10;
  Serial.println("Reiniciando");
  while(i-- > 0) {
    Serial.println(".");
    delay(500);
  }

  //pinMode ( reset , OUTPUT );
  //digitalWrite ( reset, 1 );

  //while(true);
}

void boardUpdate(char *json) {
  char tmp[100];
  bool needSaveConfig = false;

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  // Test if parsing succeeds.
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  if (root["led"] != NULL) {
    int valueLed = atoi(root["led"]);
    if (valueLed < 0) {
      boardState.lamp[0].parallel.bits.isWebON = !boardState.lamp[0].parallel.bits.isWebON;
    } else if (valueLed == 0) {
      boardState.lamp[0].parallel.bits.isWebON = 0;
    } else {
      boardState.lamp[0].parallel.bits.isWebON = 1;
    }
  }
  if (root["on"] != NULL) {
    boardState.lamp[0].tmrON = atoi(root["on"]);
  }
  if (root["off"] != NULL) {
    boardState.lamp[0].tmrOFF = atoi(root["off"]);
  }
  if (root["wifi"] != NULL) {
    if (root["wifi"]["ssid"] != NULL) {
      strcpy(tmp, root["wifi"]["ssid"]);
      ssid = tmp;
      needSaveConfig = true;
    }
    if (root["wifi"]["password"] != NULL) {
      strcpy(tmp, root["wifi"]["password"]);
      password = tmp;
      needSaveConfig = true;
    }
  }
  if (root["mqtt"]["enable"] != NULL) {
    Serial.print("mqtt enable existe! ");
    boardState.mqtt.enable = (atoi(root["mqtt"]["enable"]) == 0) ? false : true;
    needSaveConfig = true;
  }
  if (root["mqtt"]["ip"] != NULL) {
    Serial.print("mqtt ip existe! ");
    strcpy(tmp, root["mqtt"]["ip"]);
    Serial.println(tmp);
    boardState.mqtt.ip = tmp;
    needSaveConfig = true;
  }
  if (root["mqtt"]["user"] != NULL) {
    Serial.print("mqtt user existe! ");
    strcpy(tmp, root["mqtt"]["user"]);
    Serial.println(tmp);
    boardState.mqtt.user = tmp;
    needSaveConfig = true;
  }
  if (root["mqtt"]["password"] != NULL) {
    Serial.print("mqtt password existe! ");
    strcpy(tmp, root["mqtt"]["password"]);
    Serial.println(tmp);
    boardState.mqtt.password = tmp;
    needSaveConfig = true;
  }

  if(needSaveConfig) {
    saveConfig(&ssid, &password);
  }
}

const char temp[] = "<!DOCTYPE HTML><html> <head> <title>POP-0</title> <style type=\"text/css\"> .sensor{float: left; margin: 5px; padding: 10px; width: 300px; height: 330px; border: 2px solid black;}.middle{vertical-align: middle}.timer{float: left; margin: 5px; padding: 10px; width: 100px; height: 145px;}h1{line-height: 0.6; color: white; font-family: Arial}h2{line-height: 0.1; color: white; font-family: Arial}p{color: white; font-family: Arial}html, body{padding: 0; margin: 0; background-color: #3FBFEF; font-family: Arial}header{position: fixed; top: 0; width: 100%; height: 30px; background-color: #333; padding: 10px;}label{display: inline-block;float: left;clear: left;width: 130px;text-align: left;}input{display: inline-block;float: left;}footer{background-color: #333; width: 100%; bottom: 0; position: relative;}.wrapper{text-align: center;}.buttonSubmit{background-color: #555555; border: none; color: white; width: 100px; text-align: center; text-decoration: none; display: inline-block; font-size: 20px; margin: 4px 2px; cursor: pointer; border-radius: 12px; box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(0, 0, 0, 0.19);}.buttonSubmit:hover{opacity: 0.6;}.buttonSubmit:active{background-color: #444444; box-shadow: 0 2px #666; transform: translateY(4px);}#main{padding-top: 50px; text-align: center;}.buttonON{background-color: #4CAF50; border: none; color: white; width: 100px; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 20px; margin: 4px 2px; cursor: pointer; border-radius: 12px; box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(0, 0, 0, 0.19);}.buttonOFF{background-color: #F44336; border: none; color: white; width: 100px; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 20px; margin: 4px 2px; cursor: pointer; border-radius: 12px; box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(0, 0, 0, 0.19);}.buttonON:hover{background-color: #3e8e41}.buttonON:active{background-color: #3e8e41; box-shadow: 0 2px #666; transform: translateY(4px);}.buttonOFF:hover{background-color: #D53520}.buttonOFF:active{background-color: #F44336; box-shadow: 0 2px #666; transform: translateY(4px);}</style> <script type=\"text/javascript\">var updateConfig=true; var xmlHttp=createXmlHttpObject(); function checkMQTT(){if (document.getElementById(\"USEMQTT\").checked==false){document.getElementById(\"IPMQTT\").disabled=true; document.getElementById(\"USUARIOMQTT\").disabled=true; document.getElementById(\"SENHAMQTT\").disabled=true;}else{document.getElementById(\"IPMQTT\").disabled=false; document.getElementById(\"USUARIOMQTT\").disabled=false; document.getElementById(\"SENHAMQTT\").disabled=false;}}function boardSendData(params, fn){var request=createXmlHttpObject(); request.onreadystatechange=fn; request.open('POST', 'update', true); request.setRequestHeader(\"Content-type\", \"application/x-www-form-urlencoded\"); request.setRequestHeader(\"Content-length\", params.length); request.setRequestHeader(\"Connection\", \"close\"); request.send(params); if(fn !=null){fn();}}function ValidateIPaddress(ip){var x=ip.split(\".\"), x1, x2, x3, x4; if (x.length==4){x1=parseInt(x[0], 10); x2=parseInt(x[1], 10); x3=parseInt(x[2], 10); x4=parseInt(x[3], 10); if (isNaN(x1) || isNaN(x2) || isNaN(x3) || isNaN(x4)){return false;}if ((x1 >=0 && x1 <=255) && (x2 >=0 && x2 <=255) && (x3 >=0 && x3 <=255) && (x4 >=0 && x4 <=255)){return true;}}return false;}function setBoardNetwork(){var ip=document.getElementById(\"IPMQTT\").value; var mqttEnable=document.getElementById(\"USEMQTT\").checked ? 1 : 0; var ipOK=(mqttEnable !=0) ? ValidateIPaddress(ip) : true; if(ipOK){var ssid=document.getElementById(\"redewifi\"); var senha=document.getElementById(\"senhawifi\"); var senha2=document.getElementById(\"senhawifi2\"); if(senha.value.length < 8){alert(\"Senha deve ter pelo menos 8 caracteres!\");}else if(senha.value !=senha2.value){alert(\"Senha incorreta!\");}else{var networkConfig=\"json={\\\"wifi\\\":{\"; networkConfig +=\"\\\"ssid\\\":\\\"\" + ssid.value; networkConfig +=\"\\\",\\\"password\\\":\\\"\" + senha.value; networkConfig +=\"\\\"},\\\"mqtt\\\":{\\\"enable\\\":\" + mqttEnable; networkConfig +=\",\\\"ip\\\":\\\"\" + ip; networkConfig +=\"\\\",\\\"user\\\":\\\"\" + document.getElementById(\"USUARIOMQTT\").value; networkConfig +=\"\\\",\\\"password\\\":\\\"\" + document.getElementById(\"SENHAMQTT\").value; networkConfig +=\"\\\"}}\"; boardSendData(networkConfig, function(){if (xmlHttp.readyState==4){alert(\"Configuracao atualizada!\"); updateConfig=true;}});}}else{alert(\"IP incorreto: \" + ip);}}function boardSetTimer(tag){var params; var txt=document.getElementById(\"timer\" + tag); if(txt.value >=0){params=\"json={\\\"\" + tag + \"\\\":\" + (txt.value * 60) + \"}\"; boardSendData(params, null);}txt.value=\"0\";}function mudaBotao(nome){boardSendData(\"json={\\\"led\\\":-1}\", null);}function createXmlHttpObject(){if (window.XMLHttpRequest){xmlHttp=new XMLHttpRequest();}else{xmlHttp=new ActiveXObject('Microsoft.XMLHTTP');}return xmlHttp;}function boardProcess(){if (xmlHttp.readyState==0 || xmlHttp.readyState==4){xmlHttp.open('PUT', 'json', true); xmlHttp.onreadystatechange=handleServerResponse; xmlHttp.send(null);}setTimeout('boardProcess()', 1000);}function boardInit(){checkMQTT(); boardProcess();}function handleServerResponse(){var color=['white', 'yellow']; if (xmlHttp.readyState==4 && xmlHttp.status==200){var jObject=JSON.parse(xmlHttp.responseText); if(updateConfig){updateConfig=false; document.getElementById('IPMQTT' ).value=jObject.mqtt.servidor; document.getElementById('USUARIOMQTT' ).value=jObject.mqtt.usuario; document.getElementById('redewifi' ).value=jObject.board.network; document.getElementById('USEMQTT' ).checked=jObject.mqtt.enable; checkMQTT();}document.getElementById('sala_circle' ).style.fill=color[jObject.led]; if(jObject.tmrON > 0){var secs=(jObject.tmrON%60); if(secs < 10){secs=\"0\" + secs;}var textRemaining=Math.floor(jObject.tmrON/60) + \":\" + secs; document.getElementById('remainingTimerON').innerHTML=textRemaining; document.getElementById('showTimerON').style.display='inline'; document.getElementById('setTimerON' ).style.display='none'; document.getElementById('svgTimerON' ).style.fill='red';}else{document.getElementById('showTimerON').style.display='none'; document.getElementById('setTimerON' ).style.display='inline'; document.getElementById('svgTimerON' ).style.fill='black';}if(jObject.tmrOFF > 0){var secs=(jObject.tmrOFF%60); if(secs < 10){secs=\"0\" + secs;}var textRemaining=Math.floor(jObject.tmrOFF/60) + \":\" + secs; document.getElementById('remainingTimerOFF').innerHTML=textRemaining; document.getElementById('showTimerOFF').style.display='inline'; document.getElementById('setTimerOFF' ).style.display='none'; document.getElementById('svgTimerOFF' ).style.fill='red';}else{document.getElementById('showTimerOFF').style.display='none'; document.getElementById('setTimerOFF' ).style.display='inline'; document.getElementById('svgTimerOFF' ).style.fill='black';}var botao=document.getElementById('sala'); if (jObject.led !=0){botao.value=\"OFF\"; botao.className=\"buttonOFF\";}else{botao.value=\"ON\"; botao.className=\"buttonON\";}}}</script> </head> <body onload='boardInit()'> <br/> <h1>Configura&ccedil;&otilde;es</h1> <div class=\"sensor\"> <h1><b>Lampada</b></h1> <div> <input id='sala' type=\"button\" class=\"buttonON\" value=\"ON\" onclick=\"mudaBotao('sala')\" style=\"vertical-align:top;\"> <svg height=\"66\" width=\"66\"> <circle id=\"sala_circle\" cx=\"33\" cy=\"33\" r=\"30\" stroke=\"black\" stroke-width=\"3\" fill=\"yellow\"/> </svg> </div><div class='timer'> <svg id=\"svgTimerON\" height=\"66\" width=\"66\" fill=\"black\" enable-background=\"new 0 0 87.506 100\" viewBox=\"0 0 87.506 100\" xml:space=\"preserve\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"m43,26c-1.656,0-3,1.343-3,3v28c0,1.657 1.344,3 3,3s3-1.343 3-3v-28c0-1.657-1.344-3-3-3z\"></path> <path d=\"m87.506,23.807c0,0-1.414-4.242-4.243-7.07s-7.07-4.243-7.07-4.243l-8.988,8.988c-4.797-3.28-10.289-5.606-16.205-6.724v-12.758c0,0-4-2-8-2s-8,2-8,2v12.758c-19.898,3.762-35,21.266-35,42.242 0,23.71 19.29,43 43,43s43-19.29 43-43c0-8.971-2.765-17.305-7.482-24.205l8.988-8.988zm-44.506,70.193c-20.402, 0-37-16.598-37-37s16.598-37 37-37 37,16.598 37,37-16.598,37-37,37z\"></path> </svg><br/> <div id='setTimerON'>Ligar em<br/> <input id='timeron' type=\"text\" name=\"led\" value=\"\" maxlength=\"4\" size='9'/> <br/>minutos<br/> <input class=\"buttonSubmit\" type=\"submit\" value=\"Aplicar\" onclick=\"boardSetTimer('on')\"/></div><div id='showTimerON' style=\"display:none\">Liga em<br/> <a id='remainingTimerON'></a> <br/>minutos<br/> <input type=\"submit\" value=\"Cancelar\" onclick=\"boardSetTimer('on')\"/></div></div><div class='timer'> <svg id=\"svgTimerOFF\" height=\"66\" width=\"66\" fill=\"black\" enable-background=\"new 0 0 87.506 100\" viewBox=\"0 0 87.506 100\" xml:space=\"preserve\" xmlns=\"http://www.w3.org/2000/svg\"> <path d=\"m43,26c-1.656,0-3,1.343-3,3v28c0,1.657 1.344,3 3,3s3-1.343 3-3v-28c0-1.657-1.344-3-3-3z\"></path> <path d=\"m87.506,23.807c0,0-1.414-4.242-4.243-7.07s-7.07-4.243-7.07-4.243l-8.988,8.988c-4.797-3.28-10.289-5.606-16.205-6.724v-12.758c0,0-4-2-8-2s-8,2-8,2v12.758c-19.898,3.762-35,21.266-35,42.242 0,23.71 19.29,43 43,43s43-19.29 43-43c0-8.971-2.765-17.305-7.482-24.205l8.988-8.988zm-44.506,70.193c-20.402, 0-37-16.598-37-37s16.598-37 37-37 37,16.598 37,37-16.598,37-37,37z\"></path> </svg><br/> <div id='setTimerOFF'>Desligar em<br/> <input id='timeroff' type=\"text\" name=\"led\" value=\"\" maxlength=\"4\" size='9'/> <br/>minutos<br/> <input class=\"buttonSubmit\" type=\"submit\" value=\"Aplicar\" onclick=\"boardSetTimer('off')\"/></div><div id='showTimerOFF' style=\"display:none\">Desliga em<br/> <a id='remainingTimerOFF'></a> <br/>minutos<br/> <input type=\"submit\" value=\"Cancelar\" onclick=\"boardSetTimer('off')\"/></div></div></div><div class=\"sensor\"> <h2><b>Configura&ccedil;&atilde;o do WI-FI</b></h2> <label>Rede Wi-FI:</label> <input type=\"text\" id=\"redewifi\" value=\"\" maxlength=\"12\" autocomplete=\"off\" size='15'/> <br/> <label>Senha:</label> <input type=\"password\" id=\"senhawifi\" value=\"\" maxlength=\"12\" autocomplete=\"off\" size='15'/> <br/> <label>Confirmar Senha:</label><input type=\"password\" id=\"senhawifi2\" value=\"\" maxlength=\"12\" autocomplete=\"off\" size='15'/> <br/><br/> Servidor de Monitoramento? <input type=\"checkbox\" id=\"USEMQTT\" onclick=\"checkMQTT()\"/> <br/><br/> <label>Usu&aacute;rio:</label> <input type=\"text\" id=\"USUARIOMQTT\" value=\"admin\" maxlength=\"12\" size='15'/> <br/> <label>IP do Servidor:</label> <input type=\"text\" id=\"IPMQTT\" value=\"192.168.0.95\" maxlength=\"15\" size='15'/> <br/> <label>Senha:</label> <input type=\"password\" id=\"SENHAMQTT\" value=\"\" maxlength=\"12\" autocomplete=\"off\" size='15'/> <br/> <br/> <br/> <div class='wrapper'><button class=\"buttonSubmit\" id=\"APLICAR_MQTT\" onclick=\"setBoardNetwork()\">Aplicar</button></div></div></body></html>";

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
    Serial.println("parametro: " + server.argName(i) + ", valor: " + server.arg(i));
    if (server.argName(i) == "json") {
      char json[200];
      strcpy(json, server.arg(i).c_str());
      boardUpdate(json);
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

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  boardUpdate((char*)payload);
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
  xml += "\"},\"mqtt\":{\"enable\":";
  xml += boardState.mqtt.enable ? "true" : "false";
  xml += ",\"servidor\":\"";
  xml += boardState.mqtt.ip;
  xml += "\",\"usuario\":\"";
  xml += boardState.mqtt.user;
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

const uint32_t fileMagic = 0xA50F0255;

bool loadULong(File *f, uint32_t *value)
{
  return (f->read((uint8_t *)value, sizeof(uint32_t)) == sizeof(uint32_t));
}

bool loadBool(File *f, bool *value)
{
  uint32_t tmp;
  bool ret = loadULong(f, &tmp);

  if(ret == true) {
    switch(tmp) {
      default:  return false;
      case 0: *value = false; break;
      case 1: *value = true ; break;
    }
  }

 return ret;
}

bool loadString(File *f, String *s)
{
  uint32_t size;
  *s = "";

  if(loadULong(f, &size)) {
    const uint32_t bufsize = 100;
    uint8_t buf[bufsize + 1];
    while(size > 0) {
      uint32_t bytes = (size > bufsize) ? bufsize : size;
      if(f->read(buf, bytes) != bytes) {
        return false;
      }

      buf[bytes] = 0;
      *s += (char *)buf;
      size -= bytes;
    }
  } else {
    return false;
  }

  return true;
}

bool saveULong(File *f, uint32_t *value)
{
  return (f->write((const uint8_t *)value, sizeof(uint32_t)) == sizeof(uint32_t));
}

bool saveBool(File *f, bool *value)
{
  uint32_t tmp = (*value == true) ? 1 : 0;
  return saveULong(f, &tmp);
}

bool saveString(File *f, String *s)
{
  uint32_t size = s->length();
  // Se gravar o tamanho corretamente e o numero de bytes da string escritos for igual ao seu tamanho, gravacao OK !
  if(saveULong(f, &size)) {
    size_t gravado = f->write((uint8_t *)s->c_str(), size);
    if(gravado != size) {
      return false;
    }
    return true;
  }

  return false;
}

/**
   @brief Read WiFi connection information from file system.
   @param ssid String pointer for storing SSID.
   @param pass String pointer for storing PSK.
   @return True or False.

   The config file have to containt the WiFi SSID in the first line
   and the WiFi PSK in the second line.
   Line seperator can be \r\n (CR LF) \r or \n.
*/
bool loadConfig(String *ssid, String *pass)
{
  bool ret = true;
  uint32_t magic;
  // open file for reading.
  File configFile = SPIFFS.open(file_config_wifi, "r");
  if (!configFile)
  {
    Serial.println("Failed to open WiFi Config file.");

    return false;
  }

  loadULong(&configFile, &magic);
  if(magic != fileMagic                                  ||
     !loadString(&configFile, ssid                     ) ||
     !loadString(&configFile, pass                     ) ||
     !loadBool  (&configFile, &boardState.mqtt.enable  ) ||
     !loadString(&configFile, &boardState.mqtt.ip      ) ||
     !loadString(&configFile, &boardState.mqtt.user    ) ||
     !loadString(&configFile, &boardState.mqtt.password)) {
        ret = false;
  }

  configFile.close();

#ifdef SERIAL_VERBOSE
if(ret == true) {
  Serial.println("----- file content -----");
  Serial.println("ssid: '" + *ssid + "'");
  Serial.println("psk:  '" + *pass + "'");
  Serial.println((boardState.mqtt.enable) ? "MQTT Enabled" : "MQTT Disabled");
  Serial.println("MQTT IP    : '" + boardState.mqtt.ip       + "'");
  Serial.println("MQTT User  : '" + boardState.mqtt.user     + "'");
  Serial.println("MQTT Passwd: '" + boardState.mqtt.password + "'");
} else {
  Serial.println("Erro a ocarregar a configuracao da flash!");
}
#endif

  return ret;
} // loadConfig


/**
   @brief Save WiFi SSID and PSK to configuration file.
   @param ssid SSID as string pointer.
   @param pass PSK as string pointer,
   @return True or False.
*/
bool saveConfig(String *ssid, String *pass)
{
  bool ret = true;
  // Open config file for writing.
  File configFile = SPIFFS.open(file_config_wifi, "w");
  if (!configFile)
  {
    Serial.println("Failed to open config file for writing");

    return false;
  }

  // Save SSID and PSK.
  if(!saveULong (&configFile, (uint32_t *)&fileMagic   ) ||
     !saveString(&configFile, ssid                     ) ||
     !saveString(&configFile, pass                     ) ||
     !saveBool  (&configFile, &boardState.mqtt.enable  ) ||
     !saveString(&configFile, &boardState.mqtt.ip      ) ||
     !saveString(&configFile, &boardState.mqtt.user    ) ||
     !saveString(&configFile, &boardState.mqtt.password)) {
        ret = false;
  }

  configFile.close();

#ifdef SERIAL_VERBOSE
if(ret == true) {
    Serial.println("Novos dados salvos na flash:");
    Serial.println("ssid: '" + *ssid + "'");
    Serial.println("psk:  '" + *pass + "'");
    Serial.println((boardState.mqtt.enable) ? "MQTT Enabled" : "MQTT Disabled");
    Serial.println("MQTT IP    : '" + boardState.mqtt.ip       + "'");
    Serial.println("MQTT User  : '" + boardState.mqtt.user     + "'");
    Serial.println("MQTT Passwd: '" + boardState.mqtt.password + "'");
} else {
    Serial.println("Erro durante a gravacao na flash");
}
#endif

  if(ret == true) {
    boardReset();
  }

  return ret;
} /* saveConfig*/

void setup ( void ) {
  pinMode ( led, OUTPUT  );        /*led GPIO4 */
  pinMode ( led2, OUTPUT );        /*led GPIO5 */
  pinMode ( resetconfig , INPUT_PULLUP );/*led GPIO16*/
  pinMode ( botao1, INPUT_PULLUP );/*led GPIO13*/
  pinMode ( botao2, INPUT_PULLUP );/*led GPIO12*/
  pinMode ( botao3, INPUT_PULLUP );/*led GPIO14*/
  /*pinMode ( resetconfig, INPUT );  led GPIO15*/
  digitalWrite ( led, 0 );
  digitalWrite ( led2, 0 );

  Serial.begin ( 115200 );
  Serial.println ( "" );

  /* Inicializa configuracoes da placa */
  boardState.isWiFiAP     = false;

  /* Inicializa o estado da placa*/
  lampInit(boardState.lamp[0]);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
  } else {
    // Load wifi connection information.
    if (!loadConfig(&ssid, &password)) {
      Serial.println("No WiFi connection information available.");

      // Se nao carregou a configuracao, formata a flash
      SPIFFS.format();

      // Inicializa a configuracao com valores default
      ssid     = DEFAULT_WIFI_SSID  ;
      password = DEFAULT_WIFI_PASSWD;

      // Carrega as configuracoes padrao do MQTT
      boardState.mqtt.enable   = false;
      boardState.mqtt.ip       = "";
      boardState.mqtt.user     = "";
      boardState.mqtt.password = "";

      // Salva a configuracao na flash
      saveConfig(&ssid, &password);
    }
  }

  // Inicializa a configuracao com valores default
//  ssid     = DEFAULT_WIFI_SSID  ;
//  password = DEFAULT_WIFI_PASSWD;  
}

void loopNetwork() {
  IPAddress myIP;

  if (WiFi.status() != WL_CONNECTED) {
    static bool isFirstConnection = true;

    if (!boardState.isWiFiAP) {
      int i;

      WiFi.begin ( ssid.c_str(), password.c_str() );

      /* Espera pela conexão*/
      for (i = 0; WiFi.status() != WL_CONNECTED && i < 30; i++) {
        delay ( 500 );
        Serial.print ( "." );
      }

      if (WiFi.status() == WL_CONNECTED) {
        myIP = WiFi.localIP();

        Serial.println ( "" );
        Serial.print ( "Connected to '" );
        Serial.print ( ssid );
        Serial.println ( "'" );
        Serial.print ( "IP address: " );
        Serial.println ( myIP );

        client.setServer(mqtt_server, 1883);
        client.setCallback(mqttCallback);
      } else {
//        boardState.isWiFiAP = true;
      }
    }

    if (boardState.isWiFiAP && isFirstConnection) {
      boardState.mqtt.enable = false;
      boardState.isWiFiAP    = true;

      WiFi.mode(WIFI_AP); //aceita WIFI_AP / WIFI_AP_STA / WIFI_STA
      WiFi.softAP(ssid.c_str(), password.c_str());

      myIP = WiFi.softAPIP();

      Serial.println ( "" );
      Serial.print ( "Created '" );
      Serial.print ( ssid );
      Serial.println ( "'" );
      Serial.print ( "IP address: " );
      Serial.println ( myIP );
    }

    if (isFirstConnection) {
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
    }

    isFirstConnection = false;
  }
}

void loopMQTT() {
  static unsigned long timeout = 0;

  if (!client.connected()) {
    if (!timeout) {
      Serial.print("Attempting MQTT connection on " + boardState.mqtt.ip + "... ");
      /* Tentando se conectar*/
      if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
        Serial.println("connected");
        client.subscribe(board_commands_topic);
      } else {
        /*Espera 5s para reconectar*/
        timeout = millis() + mqtt_timeout;
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 60 seconds");
      }
    } else if (millis() > timeout) {
      timeout = 0;
    }
  }

  client.loop();
}

void loop ( void ) {
  bool updateTimer = false;

  loopNetwork();
  if (boardState.mqtt.enable) {
    loopMQTT();
  }

  unsigned long currms;
  static unsigned long lastms = 0;

  server.handleClient();

  boardState.lamp[0].parallel.bits.isP1ON = (digitalRead(botao1) != 0);   // turn the LED on (HIGH is the voltage level)
  boardState.lamp[0].parallel.bits.isP2ON = (digitalRead(botao2) != 0);   // turn the LED on (HIGH is the voltage level)
  boardState.lamp[0].parallel.bits.isP3ON = (digitalRead(botao3) != 0);   // turn the LED on (HIGH is the voltage level)

  /*Verifica se passou o inervalo de 1 segundo desde a ultima atualizacao do timer*/
  currms = millis();
  if (currms < lastms || (currms - lastms) >= 1000) {
    updateTimer = true;
    lastms = currms;
    if (boardState.mqtt.enable && client.connected()) {
      client.publish(board_topic, buildJSON().c_str(), true);
    }
  }

  /*Se passou 1 segundo os timers serao atualizados e, se houver atingido o valor programado, a saida sera configurada de acordo.*/
  lampUpdateOutput(boardState.lamp[0], updateTimer);

  digitalWrite(led, boardState.lamp[0].isON);    /*Turn the LED off by making the voltage LOW*/
}

