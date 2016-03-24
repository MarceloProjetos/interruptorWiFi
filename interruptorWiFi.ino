/*const int pin = 2; <--led azul
 * 
 * <form method='post' action='x'>
 */

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

void handleRoot() {
	char temp[800], estado[][10] = { "Ligado", "Desligado" };
	int sec = millis() / 1000;
	int min = sec / 60;
	int hr = min / 60;

// Primeiro atualizamos o estado da placa, conforme parametros recebidos
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if(server.argName(i) == "led") {
      boardState.isWebSwitchON = (atoi(server.arg(i).c_str()) != 0);
    } else if(server.argName(i) == "p2") {
      boardState.parallel.bits.isP2ON = (atoi(server.arg(i).c_str()) != 0);
    } else if(server.argName(i) == "p3") {
      boardState.parallel.bits.isP3ON = (atoi(server.arg(i).c_str()) != 0);
    }
  }

// Agora exibe a pagina
	snprintf ( temp, 800,

"<html>\
  <head>\
    <title>Testando o WIFI</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Bem vindo!</h1>\
    <p>Ligado a: %02d:%02d:%02d</p>\
    <img src=\"/test.svg\" />\
    <br/><a href='/?led=%d'>Interruptor Web</a>\
    <br/>Paralelo 1: %s\
    <br/>Paralelo 2: %s\
    <br/>Paralelo 3: %s\
  </body>\
</html>",

		hr, min % 60, sec % 60, !boardState.isWebSwitchON, estado[boardState.parallel.bits.isP1ON == 0], estado[boardState.parallel.bits.isP2ON == 0], estado[boardState.parallel.bits.isP3ON == 0]
	);

	server.send ( 200, "text/html", temp );
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
	server.on ( "/test.svg", file_test_svg );
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

  client.setServer(mqtt_server, 1883);
}

void file_test_svg() {
  String out = "";
  char temp[8];

  int r = 0, g = 0;
  if(boardState.isLedON) {
    g = 255;
  } else {
    r = 255;
  }
  sprintf(temp, "%d,%d", r, g);

  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" height=\"64\" width=\"64\">";
  out += "<defs>";
  out += "<radialGradient id=\"grad1\" cx=\"30%\" cy=\"30%\" r=\"10%\" fx=\"50%\" fy=\"50%\">";
  out += "<stop offset=\"0%\" style=\"stop-color:rgb(255,255,255);";
  out += " stop-opacity:0\" />";
  out += " <stop offset=\"100%\" style=\"stop-color:rgb(";
  out += temp;
  out += ",0);stop-opacity:1\" />";
  out += " </radialGradient>";
  out += "</defs>";
  out += " <ellipse cx=\"32\" cy=\"32\" rx=\"32\" ry=\"32\" fill=\"url(#grad1)\" />";
  out += "</svg>"; 

  server.send ( 200, "image/svg+xml", out);
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

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  boardState.parallel.bits.isP1ON = (digitalRead(botao1) != 0);   // turn the LED on (HIGH is the voltage level)
  boardState.parallel.bits.isP2ON = (digitalRead(botao2) != 0);   // turn the LED on (HIGH is the voltage level)
  boardState.parallel.bits.isP3ON = (digitalRead(botao3) != 0);   // turn the LED on (HIGH is the voltage level)

  // Nao calcula a saida no primeiro loop
  if(initOK) {
    // Se houve alteracao na chave Web, o estado dela representa o estado do led
    // Se houve alteracao no estado do paralelo, inverte o estado do led. Mas se a chave Web estiver ativa, mantem o led ativo.
    if(boardState.isWebSwitchON != lastBoardState.isWebSwitchON) {
      boardState.isLedON = boardState.isWebSwitchON;
    } else if(boardState.parallel.value != lastBoardState.parallel.value) {
      boardState.isLedON = !boardState.isLedON || boardState.isWebSwitchON;
    }
  } else {
    initOK = 1;
  }

  digitalWrite(led, boardState.isLedON);    // turn the LED off by making the voltage LOW

  if( (boardState.isWebSwitchON != lastBoardState.isWebSwitchON) || (boardState.parallel.value != lastBoardState.parallel.value) ) {
    client.publish(board_topic, "teste", true);
  }

  lastBoardState = boardState;
}

