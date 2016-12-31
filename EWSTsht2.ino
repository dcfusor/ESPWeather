/*
 * Copyright (c) 2015, Majenko Technologies
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * * Neither the name of Majenko Technologies nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /* well, the above wrote the wifi stack, I wrote the rest and it's GPL-V2, C Doug Coulter 2016 */

 // test change 12/15 retest - very strange, think I found a bug.  Remebers path to "save as" last time, but when trying to get it back..not correct.

 //@@@ for later use:  ESP.restart() is software reset, but doesn't work on serial jig - only later on.

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

// pin defines as this might get ported to another platform
#define SDA 4
#define SCL 5
#define DEBUG_PIN 13

Adafruit_SHT31 sht31 = Adafruit_SHT31();

// scheduling stuff
bool debug; // if pin is high, we're not running on the dev box
bool did_something; // we did something this loop, so hold off till next one

bool do_tellem;
bool update_th;
bool update_plot_data;

unsigned long loop_millis;
#define TENSEC_MS 10000
unsigned long tensec_z;
#define MINUTE_MS 60000
unsigned long minute_z; // milliseconds since last tell
#define TWOMINUTE_MS 120000
unsigned long twominute_z;

// tellem stuff
WiFiUDP port;
char packetBuffer[255];
unsigned int localPort = 53831; // landns UDP port
IPAddress myip; // my ip as assigned by dhcp
IPAddress bcip; // broadcast addr for this network
unsigned char mac[6]; // my mac, I hope
char tellem_string[40]; // What we'll broadcast
unsigned long tellems;
unsigned long errors;


// WAP stuff - uh-oh hardcoded creds @@@
const char *ssid = "machswap";
const char *password = "fusordoug";

ESP8266WebServer server ( 80 );

char g_t_txt[7]; // global avg temp as text
char g_h_txt[7]; // humidity avg as text

#define LPI 0.1
#define LPA (1.0 - LPI)
// the above must sum to 1.0 regardless of any FP representation errors

bool sht_first_time; // start without filter
float t_avg; // average temp
float h_avg; // average h

#define PLOT_SIZE 720
byte t_plot[PLOT_SIZE]; // plot data for 720 points 
byte h_plot[PLOT_SIZE]; // done at 2 min intervals for 24 hours
word plot_index;  // where we are in the roundy round

////////////////////////////////////
void init_ram()
{ // make ram clean
  word i;
  debug = did_something = do_tellem = update_th = update_plot_data = false;
  sht_first_time = true;
  tellems = errors = 0;
  twominute_z = minute_z = millis();
  plot_index = 0;
  for (i=0;i<PLOT_SIZE;i++)
  {
   t_plot[i] = h_plot[i] = 0;
  }
}

////////////////////////////////////
void build_tellem_string ()
{
  char i;
  String mystring;

//@@@ class String can't 0 pad hex...
// but sprintf and strncpy are lots more lines of code (less rom though)
  mystring = myip.toString();
  mystring += " ESP_";
  for (i=3;i<6;i++) 
  {
    if (16 > mac[i]) mystring += "0"; // yes, it is that dumb    
    mystring += String(mac[i],HEX);
   }
  mystring.toUpperCase(); // because damn, that's what it is on the net for some retarded opsys
  mystring.toCharArray(tellem_string, 39);
  
  Serial.println(tellem_string); // debug
}
////////////////////////////////////
void tellem()
{
  port.beginPacket(bcip,localPort);
  port.write(tellem_string);
  port.endPacket(); 
  tellems++; // for grins, count them
  do_tellem = false; // we did it
}
////////////////////////////////////
void ditch_incoming()
{ // ignore other broadcasts, maybe stop possible memory leak.
  int packetSize = port.parsePacket();
  if (packetSize) {
    int len = port.read(packetBuffer, 254);
    if (debug)
    { // but hey, make sure it works if we are in debug mode!
     if (len > 0) packetBuffer[len] = 0;
     Serial.println(packetBuffer);
    }  
  }
}
////////////////////////////////////
void read_sht()
{
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (! isnan(t)) {  // check if 'is not a number'
    t *= 1.8;
    t += 32;
    if (!sht_first_time) t_avg = t * LPI + t_avg * LPA;
    else t_avg = t; // just cram it the first time
    
    dtostrf(t_avg,5,2,g_t_txt);

    if (debug) 
    {
      Serial.print("F*= "); 
      Serial.println(t);
    }
  } else { 
    if (debug) Serial.println("Failed to read temperature");
    errors++;
  }
  
  if (! isnan(h)) {  // check if 'is not a number'
    if (debug) 
    {
      Serial.print("H% = "); 
      Serial.println(h);
    }
    if (!sht_first_time) h_avg = h * LPI + h_avg * LPA; // lowpass filter
    else h_avg = h; // first time just jam it in
    dtostrf(h_avg,5,2,g_h_txt);
    
  } else { 
    if (debug) Serial.println("Failed to read humidity");
    errors++;
  }
  if (debug) Serial.println();
  update_th = false; // did it
  sht_first_time = false;

}
////////////////////////////////////



////////////////////////////////////
void handleRoot() {
  #define PAGE_SIZE 500
	char temp[PAGE_SIZE];

  did_something = true; // we did something that takes time
	snprintf ( temp, PAGE_SIZE,

"<html>\
  <head>\
    <meta http-equiv='refresh' content='60'/>\
    <title>ESP demo2</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>ESP weather</h1>\
    <p>Temp: %s F RH: %s%%\t Bcasts:%d Sensor Errors:%d</p>\
    <p><h1>Temperature 24 Hour</h1></p>\
    <img src=\"/tp.svg\" />\
    <p><h1>Relative Humidity 24 Hour</h1></p>\
    <img src=\"/th.svg\" />\
  </body>\
</html>"
 ,g_t_txt,g_h_txt,tellems,errors);
	server.send ( 200, "text/html", temp );
}
/////////////////////////////////////////////////
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
  did_something = true;
}
// *****************************************************************************/
void setup ( void ) {
//  Wire.pins(SDA,SCL);
   init_ram();
// decide on debug mode here
   pinMode (DEBUG_PIN,INPUT);
   debug = !digitalRead(DEBUG_PIN);

	Serial.begin ( 115200 );
	WiFi.begin ( ssid, password );
	Serial.println ( "" );

	// Wait for connection
	while ( WiFi.status() != WL_CONNECTED ) {
		delay ( 500 );
		Serial.print ( "." );
   /// tellem stuff
    WiFi.macAddress(mac); // get my MAC addr, as it's part of hostname
    myip = WiFi.localIP(); // class and union of dword and 4 bytes
    bcip = myip; // copy net address but
    bcip[3] = 255; // make it broadcast address

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
//	server.on ( "/test.svg", drawGraph );
  server.on ( "/tp.svg", drawTplot ); // add real one
  server.on ( "/th.svg", drawHplot ); // add real one
	server.on ( "/inline", []() {
		server.send ( 200, "text/plain", "this works as well" );
	} );
	server.onNotFound ( handleNotFound );
	server.begin();
	Serial.println ( "HTTP server started" );
  // tellem stuff
  build_tellem_string();
  port.begin(localPort); // for tellem
  sht31.begin(0x44);  // init sht sensor

}
///////////////////////////////////////////////////////////////////
void update_plot_arrays()
{
// make sliding inserts in arrays to plot
    t_plot[plot_index] = byte (t_avg + 0.5); // assuming we need rounding
    h_plot[plot_index] = byte (h_avg + 0.5); 
    plot_index++;
    if (plot_index >= PLOT_SIZE) plot_index = 0;
    update_plot_data = false; // we did it
}
///////////////////////////////////////////////////////////////////
void loop ( void ) {
  did_something = false; // because we haven't yet on this round
	server.handleClient(); // if any hits should set did_something to true
  loop_millis = millis(); // just do this once
  debug = !digitalRead(DEBUG_PIN); // semi-realtime update of debug

// set scheduling flags for various intervals
// add setting various bools to these to get something to run (eventually)
  if (loop_millis - twominute_z >= TWOMINUTE_MS)
  {
    twominute_z = loop_millis; // update for next time
    update_plot_data = true; // schedule 1/2minute stuff
  }
  if (loop_millis - minute_z >= MINUTE_MS)
  {
    minute_z = loop_millis; // update for next time
    do_tellem = true; // schedule 1/minute stuff
  }
  if (loop_millis - tensec_z >= TENSEC_MS)
  {
   tensec_z = loop_millis; 
   update_th = true; // every ten seconds    
  }

  
// Do stuff.  Not 1::1 with how often requested on purpose - important stuff gets priority regardless.
// (because we check/do that first)  
// If we've already "done something" this loop, wait for next roundy round.

  if (update_plot_data && !did_something)
  {
    update_plot_arrays();
    did_something = true;
  }

  if (do_tellem  && !did_something) // only if it is time AND we didn't do something else first 
  {
   tellem();
   did_something = true; // skip other things this loop
  } // every minute or so

   if (update_th && !did_something)
   {
    read_sht();
    did_something = true;
   }
 
   if (!did_something) ditch_incoming(); // in case it matters?
// fancy opsys, eh?
}
//********************************************************************************************
///@@@ NOTE!!! Class String in arduino tops out at 64k?....so I did this as two plots
// but it could also be just not that much mem available...
// if polyline and more cleverness used, it'd (~?) fit in one, and be faster at the expense of more LOC
///////////////////////////////////////////////////////////////////
void drawTplot() { //chunk size limited to 1460 in webserver.h ?  seems to work with bigger anyway
  String out = "";
  char temp[100];
  int i,j,k,y2;
  
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"720\" height=\"360\">\n";
  out += "<rect width=\"720\" height=\"360\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";

  for (i=72,j=20;i<360;i+=72,j+=20)
  { // draw grid lines and labels
   sprintf(temp,"<line x1=\"20\" y1=\"%d\" x2=\"719\" y2=\"%d\" stroke-dasharray=\"5,5\" stroke=\"black\" />\n",360-i,360-i);
   out += temp; 
   sprintf(temp,"<text x=\"5\" y=\"%d\">%d</text>\n",360-i,j);
   out += temp;   
  }
   out += "<g transform=\"translate(0,360) scale(1,-3.6)\">\n"; // let browser do the flipping and scaling
// draw temperature array in red
  out += "<g stroke=\"red\">\n";

  j = plot_index; // where we are in circular buffer
  k = t_plot[j];
  for (i=0;i<PLOT_SIZE-1;i++) // go around the circle
  {
   j++;
   if (j >= PLOT_SIZE) j = 0; // wrap around at end    
   y2 = t_plot[j];
   sprintf(temp,"<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\"  />\n",i,k,i+1,y2);
   out += temp;
   k=y2;
  }
  
  out += "</g>\n</g>\n</svg>\n"; // end the groups
  server.send ( 200, "image/svg+xml", out);
  did_something = true; // we did something that takes time
}

////////////////////////////////////////////////////////////////////////
void drawHplot() { //chunk size limited to 1460 in webserver.h ?  seems to work with bigger anyway
  String out = "";
  char temp[100];
  int i,j,k,y2;
  
// start svg create
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"720\" height=\"360\">\n";
  out += "<rect width=\"720\" height=\"360\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
// draw dashed lines on plot
  for (i=72,j=20;i<360;i+=72,j+=20)
  { // draw grid lines and labels
   sprintf(temp,"<line x1=\"20\" y1=\"%d\" x2=\"719\" y2=\"%d\" stroke-dasharray=\"5,5\" stroke=\"black\" />\n",360-i,360-i);
   out += temp; 
   sprintf(temp,"<text x=\"5\" y=\"%d\">%d</text>\n",360-i,j);
   out += temp;   
  }
// attempt scale/flip global group
  out += "<g transform=\"translate(0,360) scale(1,-3.6)\">\n";
// draw humid array in blue - could not make work inside transform group
  out += "<g stroke=\"blue\">\n";
  j = plot_index; // where we are in circular buffer
   k= int(h_plot[j]);
  for (i=0;i<PLOT_SIZE-1;i++) // go around the circle
  {
   j++;
   if (j >= PLOT_SIZE) j = 0; // wrap around at end    
   y2= int(h_plot[j]); 
   sprintf(temp,"<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\"  />\n",i,k,i+1,y2);
   out += temp;
   k=y2;
  }
  out += "</g>\n</g>\n</svg>\n"; // end the groups and plot
  server.send ( 200, "image/svg+xml", out);
  did_something = true; // we did something that takes time
}


/*
 server.send as used above sends a header first, then String class content
 server.sendContent(const String&) just sends a string class
 So you should be able to break these up into chunks to keep the string size down
 for any single chunk.
*/
