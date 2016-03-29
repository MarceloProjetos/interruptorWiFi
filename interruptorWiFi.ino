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
  const int html_size = 1500;
	char temp[html_size], estado[][10] = { "Ligado", "Desligado" };
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
	snprintf ( temp, html_size,

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
    <style type=\"text/css\" media=\"screen\">\
      .pai { position=relative; width: 400px; height: 400px; } .filho_switch { width: 100px; position: absolute; left: 5px; } .filho_p1 { width: 100px; position: absolute; left: 105px; }\
      .filho_p2 { width: 100px; position: absolute; left: 205px; } .filho_p3 { width: 100px; position: absolute; left: 305px; }\
    </style>\
    <p></p>\
    <div class=\"pai\"><div class=\"filho_switch\"><p><a href='/?led=%d'><img src=\"/switch.svg\" /></a><br/>Web</p></div>\
    <div class=\"filho_p1\"><p><img src=\"/p1.svg\" /><br/>P1</p></div>\
    <div class=\"filho_p2\"><p><img src=\"/p2.svg\" /><br/>P2</p></div>\
    <div class=\"filho_p3\"><p><img src=\"/p3.svg\" /><br/>P3</p></div></div>\
  </body>\
</html>",

		hr, min % 60, sec % 60, !boardState.isWebSwitchON
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
  server.on ( "/switch.svg", file_switch_svg );
  server.on ( "/p1.svg", file_p1_svg );
  server.on ( "/p2.svg", file_p2_svg );
  server.on ( "/p3.svg", file_p3_svg );
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

void file_switch_on_svg(void)
{
  String out = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\
<svg id=\"svg3336\" xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" xmlns=\"http://www.w3.org/2000/svg\" height=\"100.55\" width=\"42.274\" version=\"1.1\" xmlns:cc=\"http://creativecommons.org/ns#\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\
 <metadata id=\"metadata3410\">\
  <rdf:RDF>\
   <cc:Work rdf:about=\"\">\
    <dc:format>image/svg+xml</dc:format>\
    <dc:type rdf:resource=\"http://purl.org/dc/dcmitype/StillImage\"/>\
    <dc:title/>\
   </cc:Work>\
  </rdf:RDF>\
 </metadata>\
 <defs id=\"defs4\">\
  <filter id=\"filter4899\">\
   <feGaussianBlur id=\"feGaussianBlur4901\" stdDeviation=\"0.1086468\"/>\
  </filter>\
  <filter id=\"filter4873\" y=\"-.53149\" width=\"2.063\" x=\"-.53149\" height=\"2.063\">\
   <feGaussianBlur id=\"feGaussianBlur4875\" stdDeviation=\"1.5292102\"/>\
  </filter>\
  <radialGradient id=\"radialGradient4486\" gradientUnits=\"userSpaceOnUse\" cy=\"978.64\" cx=\"62.349\" gradientTransform=\"matrix(1.0003,7.2095e-6,-2.6801e-5,3.3026,44.956,-2258.2)\" r=\"22.395\">\
   <stop id=\"stop4230\" stop-color=\"#fff\" offset=\"0\"/>\
   <stop id=\"stop4232\" stop-color=\"#121212\" offset=\"1\"/>\
  </radialGradient>\
  <radialGradient id=\"radialGradient4484\" gradientUnits=\"userSpaceOnUse\" cy=\"989.5\" cx=\"62.349\" gradientTransform=\"matrix(1.0003,8.4487e-6,-1.3663e-5,1.4399,44.943,-450.89)\" r=\"22.395\">\
   <stop id=\"stop4348\" stop-color=\"#413e3e\" offset=\"0\"/>\
   <stop id=\"stop4350\" stop-color=\"#373434\" offset=\"1\"/>\
  </radialGradient>\
  <radialGradient id=\"radialGradient4478\" gradientUnits=\"userSpaceOnUse\" cy=\"1046.3\" cx=\"78.314\" gradientTransform=\"matrix(1.8287,-1.3059,1.6925,2.1095,-1778.2,-1062.6)\" r=\"19.742\">\
   <stop id=\"stop4340\" stop-color=\"#8b8b8b\" offset=\"0\"/>\
   <stop id=\"stop4342\" stop-color=\"#484848\" offset=\"1\"/>\
  </radialGradient>\
  <radialGradient id=\"radialGradient3424\" gradientUnits=\"userSpaceOnUse\" cy=\"943.65\" cx=\"62.386\" gradientTransform=\"matrix(1.0044 .0013122 -.00059088 .39999 -5.222 571.22)\" r=\"22.468\">\
   <stop id=\"stop4256\" stop-color=\"#5a5a5a\" offset=\"0\"/>\
   <stop id=\"stop4250\" stop-color=\"#353232\" offset=\"1\"/>\
  </radialGradient>\
  <linearGradient id=\"linearGradient3426\" y2=\"942.33\" gradientUnits=\"userSpaceOnUse\" x2=\"87.009\" gradientTransform=\"matrix(1.0346 0 0 .81312 -7.9133 178.17)\" y1=\"942.33\" x1=\"40.179\">\
   <stop id=\"stop4270\" offset=\"0\"/>\
   <stop id=\"stop4282\" stop-opacity=\"0\" offset=\".080423\"/>\
   <stop id=\"stop4284\" stop-opacity=\"0\" offset=\".91051\"/>\
   <stop id=\"stop4272\" offset=\"1\"/>\
  </linearGradient>\
  <linearGradient id=\"linearGradient3428\" y2=\"74.239\" gradientUnits=\"userSpaceOnUse\" x2=\"27.999\" y1=\"82.6\" x1=\"36.359\">\
   <stop id=\"stop4841\" stop-color=\"#404040\" offset=\"0\"/>\
   <stop id=\"stop4843\" stop-opacity=\".977\" offset=\"1\"/>\
  </linearGradient>\
 </defs>\
 <g id=\"g5184\" transform=\"translate(-12.344 -941.88)\">\
  <g id=\"g5247\">\
   <path id=\"rect4200\" d=\"m92.575 969.37h30.18c2.4329 0 4.3916 1.2473 4.3916 2.7967v50.607c0 1.5494-1.9586 2.7967-4.3916 2.7967h-30.18c-2.4329 0-4.3916-1.2473-4.3916-2.7967v-50.607c0-1.5494 1.9586-2.7967 4.3916-2.7967z\" transform=\"matrix(.94345 0 0 1 -67.76 16.6)\" stroke=\"#1d1d1d\" stroke-width=\"0.521\" fill=\"url(#radialGradient4478)\"/>\
   <g id=\"g4318\" transform=\"matrix(.94046 0 0 1 -19.987 2.1471)\">\
    <path id=\"rect4214\" d=\"m39.223 939.96 36.068 0.009c1.6704-0.0345 2.6086 0.007 3.1627 0.52959 0.66223 0.6243 0.39491 1.1029 0.52134 2.3947l0.1183 4.8911-44.481 0.7752 0.0278-5.7495c0-1.0358 0.39495-1.8168 1.0839-2.1481 0.73962-0.47384 2.2204-0.68632 3.4995-0.70209z\" stroke-opacity=\"0.383\" stroke=\"#0b0b0b\" stroke-width=\".458\" fill=\"url(#radialGradient3424)\"/>\
    <path id=\"path4260\" fill=\"url(#linearGradient3426)\" d=\"m35.616 940.6c0.53956-0.35393 1.8461-0.79464 1.8461-0.79464h39.093s1.1624 0.1507 1.8664 0.41759c0.5213 0.19762 0.816 1.2638 0.816 1.2638l0.0022 6.6775-44.538 1.3019-0.04-7.4091s0.25594-0.99911 0.95403-1.457z\"/>\
   </g>\
   <path id=\"rect4210\" d=\"m85.617 933.19c0.46802-0.45382 1.399-0.37566 1.399-0.37566l40.918 0.0639 1.5115 0.002-2.2524 40.733h-39.055l-2.9826-39.374s-0.01252-0.58925 0.46226-1.0496z\" transform=\"matrix(.94345 0 0 1 -67.76 16.6)\" stroke=\"url(#radialGradient4486)\" stroke-width=\".5\" fill=\"url(#radialGradient4484)\"/>\
   <g id=\"g3381\" stroke-width=\"1.455\" fill=\"#84fa4d\">\
    <path id=\"path4847\" opacity=\"0.846\" d=\"m35.632 78.42c0 1.9069-1.5458 3.4527-3.4527 3.4527s-3.4527-1.5458-3.4527-3.4527 1.5458-3.4527 3.4527-3.4527 3.4527 1.5458 3.4527 3.4527z\" transform=\"matrix(.68718 0 0 .68718 11.903 954.27)\" filter=\"url(#filter4873)\" stroke=\"#0fff0f\"/>\
    <path id=\"path4813\" d=\"m35.632 78.42c0 1.9069-1.5458 3.4527-3.4527 3.4527s-3.4527-1.5458-3.4527-3.4527 1.5458-3.4527 3.4527-3.4527 3.4527 1.5458 3.4527 3.4527z\" transform=\"matrix(.68718 0 0 .68718 11.903 954.27)\" stroke=\"url(#linearGradient3428)\"/>\
   </g>\
   <g id=\"g3385\" font-size=\"40px\" font-weight=\"bold\" font-family=\"Sans\">\
    <text id=\"text4883\" xml:space=\"preserve\" filter=\"url(#filter4899)\" y=\"1024.5792\" x=\"27.593599\" fill=\"#616161\"><tspan id=\"tspan4885\" font-size=\"10px\" y=\"1024.5792\" x=\"27.593599\" font-family=\"Umpush\">ON</tspan></text>\
    <text id=\"text4879\" xml:space=\"preserve\" y=\"1024.2513\" x=\"27.317385\" fill=\"#0d0d0d\"><tspan id=\"tspan4881\" font-size=\"10px\" y=\"1024.2513\" x=\"27.317385\" font-family=\"Umpush\">ON</tspan></text>\
   </g>\
  </g>\
 </g>\
</svg>\
";

  server.send ( 200, "image/svg+xml", out);
}

void file_switch_off_svg(void)
{
  String out = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\
<svg id=\"svg3336\" xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" xmlns=\"http://www.w3.org/2000/svg\" height=\"100.58\" width=\"42.285\" version=\"1.1\" xmlns:cc=\"http://creativecommons.org/ns#\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\
 <metadata id=\"metadata3410\">\
  <rdf:RDF>\
   <cc:Work rdf:about=\"\">\
    <dc:format>image/svg+xml</dc:format>\
    <dc:type rdf:resource=\"http://purl.org/dc/dcmitype/StillImage\"/>\
    <dc:title/>\
   </cc:Work>\
  </rdf:RDF>\
 </metadata>\
 <defs id=\"defs4\">\
  <filter id=\"filter4899\">\
   <feGaussianBlur id=\"feGaussianBlur4901\" stdDeviation=\"0.1086468\"/>\
  </filter>\
  <filter id=\"filter4873\" y=\"-.53149\" width=\"2.063\" x=\"-.53149\" height=\"2.063\">\
   <feGaussianBlur id=\"feGaussianBlur4875\" stdDeviation=\"1.5292102\"/>\
  </filter>\
  <linearGradient id=\"linearGradient5177\" x1=\"40.179\" gradientUnits=\"userSpaceOnUse\" y1=\"942.33\" gradientTransform=\"matrix(1.0346 0 0 .81312 -7.9133 178.17)\" x2=\"87.009\" y2=\"942.33\">\
   <stop id=\"stop4270\" offset=\"0\"/>\
   <stop id=\"stop4282\" stop-opacity=\"0\" offset=\".080423\"/>\
   <stop id=\"stop4284\" stop-opacity=\"0\" offset=\".91051\"/>\
   <stop id=\"stop4272\" offset=\"1\"/>\
  </linearGradient>\
  <linearGradient id=\"linearGradient5352\" x1=\"36.359\" gradientUnits=\"userSpaceOnUse\" x2=\"27.999\" y1=\"82.6\" y2=\"74.239\">\
   <stop id=\"stop4841\" stop-color=\"#404040\" offset=\"0\"/>\
   <stop id=\"stop4843\" stop-opacity=\".977\" offset=\"1\"/>\
  </linearGradient>\
  <radialGradient id=\"radialGradient5348\" gradientUnits=\"userSpaceOnUse\" cy=\"989.5\" cx=\"62.349\" gradientTransform=\"matrix(0.94376,-8.4487e-6,-1.289e-5,-1.4399,-25.359,2418.6)\" r=\"22.395\">\
   <stop id=\"stop4348\" stop-color=\"#413e3e\" offset=\"0\"/>\
   <stop id=\"stop4350\" stop-color=\"#373434\" offset=\"1\"/>\
  </radialGradient>\
  <radialGradient id=\"radialGradient5350\" gradientUnits=\"userSpaceOnUse\" cy=\"978.64\" cx=\"62.349\" gradientTransform=\"matrix(0.94376,-7.2095e-6,-2.5286e-5,-3.3026,-25.346,4225.9)\" r=\"22.395\">\
   <stop id=\"stop4230\" stop-color=\"#fff\" offset=\"0\"/>\
   <stop id=\"stop4232\" stop-color=\"#121212\" offset=\"1\"/>\
  </radialGradient>\
  <radialGradient id=\"radialGradient5344\" gradientUnits=\"userSpaceOnUse\" cy=\"943.65\" cx=\"62.386\" gradientTransform=\"matrix(1.0044 .0013122 -.00059088 .39999 -5.222 571.22)\" r=\"22.468\">\
   <stop id=\"stop4256\" stop-color=\"#5a5a5a\" offset=\"0\"/>\
   <stop id=\"stop4250\" stop-color=\"#353232\" offset=\"1\"/>\
  </radialGradient>\
  <radialGradient id=\"radialGradient5342\" gradientUnits=\"userSpaceOnUse\" cy=\"1046.3\" cx=\"78.314\" gradientTransform=\"matrix(1.7252,1.3059,1.5968,-2.1095,-1745.4,3030.3)\" r=\"19.742\">\
   <stop id=\"stop4340\" stop-color=\"#8b8b8b\" offset=\"0\"/>\
   <stop id=\"stop4342\" stop-color=\"#484848\" offset=\"1\"/>\
  </radialGradient>\
 </defs>\
 <g id=\"g5326\" transform=\"translate(-72.336 -941.89)\">\
  <g id=\"g5260\" transform=\"translate(60)\">\
   <g id=\"g5262\">\
    <path id=\"path5264\" d=\"m19.58 998.34h28.474c2.2953 0 4.1432-1.2473 4.1432-2.7967v-50.607c0-1.5494-1.8479-2.7967-4.1432-2.7967h-28.474c-2.2953 0-4.1432 1.2473-4.1432 2.7967v50.607c0 1.5494 1.8479 2.7967 4.1432 2.7967z\" stroke=\"#1d1d1d\" stroke-width=\"0.506\" fill=\"url(#radialGradient5342)\"/>\
    <g id=\"g5266\" transform=\"matrix(.94046 0 0 -1 -19.987 1982.2)\">\
     <path id=\"path5268\" d=\"m39.223 939.96 36.068 0.009c1.6704-0.0345 2.6086 0.007 3.1627 0.52959 0.66223 0.6243 0.39491 1.1029 0.52134 2.3947l0.1183 4.8911-44.481 0.7752 0.0278-5.7495c0-1.0358 0.39495-1.8168 1.0839-2.1481 0.73962-0.47384 2.2204-0.68632 3.4995-0.70209z\" stroke-opacity=\"0.383\" stroke=\"#0b0b0b\" stroke-width=\".458\" fill=\"url(#radialGradient5344)\"/>\
     <path id=\"path5270\" fill=\"url(#linearGradient5177)\" d=\"m35.616 940.6c0.53956-0.35393 1.8461-0.79464 1.8461-0.79464h39.093s1.1624 0.1507 1.8664 0.41759c0.5213 0.19762 0.816 1.2638 0.816 1.2638l0.0022 6.6775-44.538 1.3019-0.04-7.4091s0.25594-0.99911 0.95403-1.457z\"/>\
    </g>\
    <path id=\"path5272\" d=\"m13.015 1034.5c0.44155 0.4538 1.3199 0.3757 1.3199 0.3757l38.604-0.064h1.426l-2.125-40.734h-36.847l-2.8139 39.374s-0.01181 0.5892 0.43612 1.0496z\" stroke=\"url(#radialGradient5350)\" stroke-width=\".486\" fill=\"url(#radialGradient5348)\"/>\
   </g>\
  </g>\
  <g id=\"g4930\" transform=\"translate(3.0248,4.28)\">\
   <path id=\"path4903\" opacity=\"0.846\" d=\"m35.632 78.42c0 1.9069-1.5458 3.4527-3.4527 3.4527s-3.4527-1.5458-3.4527-3.4527 1.5458-3.4527 3.4527-3.4527 3.4527 1.5458 3.4527 3.4527z\" transform=\"matrix(.68718 0 0 .68718 67.609 901.49)\" filter=\"url(#filter4873)\" fill=\"none\"/>\
   <path id=\"path4905\" d=\"m35.632 78.42c0 1.9069-1.5458 3.4527-3.4527 3.4527s-3.4527-1.5458-3.4527-3.4527 1.5458-3.4527 3.4527-3.4527 3.4527 1.5458 3.4527 3.4527z\" transform=\"matrix(.68718 0 0 .68718 69.023 901.49)\" stroke=\"url(#linearGradient5352)\" stroke-width=\"1.455\" fill=\"#b10505\"/>\
   <g id=\"g3402\" font-size=\"40px\" font-weight=\"bold\" font-family=\"Sans\">\
    <text id=\"text4907\" xml:space=\"preserve\" filter=\"url(#filter4899)\" y=\"971.79749\" x=\"81.299301\" fill=\"#616161\"><tspan id=\"tspan4909\" font-size=\"10px\" y=\"971.79749\" x=\"81.299301\" font-family=\"Umpush\">OFF</tspan></text>\
    <text id=\"text4911\" xml:space=\"preserve\" y=\"971.4696\" x=\"81.023087\" fill=\"#0d0d0d\"><tspan id=\"tspan4913\" font-size=\"10px\" y=\"971.4696\" x=\"81.023087\" font-family=\"Umpush\">OFF</tspan></text>\
   </g>\
  </g>\
 </g>\
</svg>\
";

  server.send ( 200, "image/svg+xml", out);
}

void file_switch_svg(void){
  if(boardState.isWebSwitchON) {
    file_switch_on_svg();
  } else {
    file_switch_off_svg();
  }
}

void file_p1_svg(void){
  if(boardState.parallel.bits.isP1ON) {
    file_switch_on_svg();
  } else {
    file_switch_off_svg();
  }
}

void file_p2_svg(void){
  if(boardState.parallel.bits.isP2ON) {
    file_switch_on_svg();
  } else {
    file_switch_off_svg();
  }
}

void file_p3_svg(void){
  if(boardState.parallel.bits.isP3ON) {
    file_switch_on_svg();
  } else {
    file_switch_off_svg();
  }
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

