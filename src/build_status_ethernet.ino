// 
// Arduino 1.0.1 apparently had some weird issues with strings that result in failure
// to properly read the incoming data from the Xbee. Updating to Arduino 1.0.4 resolved
// the issue.
// 
#include <Wire.h>
// BlinkMCommunicator available at http://blinkm-projects.googlecode.com/files/BlinkMCommunicator.zip
#include <BlinkM_funcs.h>
#include <SPI.h>
#include <Ethernet.h>

// TODO: MTH: Change this... is there one on the shield?
byte myMACAddress[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x01};

// Default IP address used when DHCP fails
IPAddress defaultLocalIPAddress(192,168,1,25);

// The hostname or ip of the Jenkins server
#define JENKINS_HOST "192.168.1.5"

// The port for the Jenkins server
#define JENKINS_PORT 8080

// The Jenkins Job to monitor
#define JENKINS_JOB_NAME "Test%20Job%201"

blinkm_script_line script_lines_blink_red[] = {
 {  1,  {'f', 10,00,00}},
 {  100, {'c', 0xff,0x00,0x00}},  // red
 {  100, {'c', 0x00,0x00,0x00}},  // black
};
int script_len_blink_red = 3;  // number of script lines above

blinkm_script_line script_lines_blink_yellow[] = {
 {  1,  {'f', 10,00,00}},
 {  100, {'c', 0xff,0xff,0x00}},  // yellow
 {  100, {'c', 0x00,0x00,0x00}},  // black
};
int script_len_blink_yellow = 3;  // number of script lines above

blinkm_script_line script_lines_blink_blue[] = {
 {  1,  {'f', 10,00,00}},
 {  100, {'c', 0x00,0x00,0xff}},  // blue
 {  100, {'c', 0x00,0x00,0x00}},  // black
};
int script_len_blink_blue = 3;  // number of script lines above

blinkm_script_line script_lines_blink_grey[] = {
 {  1,  {'f', 10,00,00}},
 {  100, {'c', 0x80,0x80,0x80}},  // grey
 {  100, {'c', 0x00,0x00,0x00}},  // black
};
int script_len_blink_grey = 3;  // number of script lines above

blinkm_script_line script_lines_blink_unknown[] = {
 {  1,  {'f', 10,00,00}},
 {  10, {'c', 0xff,0x00,0x00}},  // red
 {  10, {'c', 0xff,0xff,0x00}},  // yellow
 {  10, {'c', 0x00,0x00,0xff}},  // blue
 {  10, {'c', 0x00,0x00,0x00}},  // black
};
int script_len_blink_unknown = 5;  // number of script lines above

// From jenkins core/src/main/java/hudson/model/BallColor.java
enum BallColor {
  BALL_COLOR_UNKNOWN,
  BALL_COLOR_RED, 
  BALL_COLOR_RED_ANIME,
  BALL_COLOR_YELLOW,
  BALL_COLOR_YELLOW_ANIME,
  BALL_COLOR_BLUE,
  BALL_COLOR_BLUE_ANIME,
  BALL_COLOR_GREY,
  BALL_COLOR_GREY_ANIME
  // We don't need the following since they all resolve to "grey" in the json
  //BALL_COLOR_DISABLED,
  //BALL_COLOR_DISABLED_ANIME,
  //BALL_COLOR_ABORTED,
  //BALL_COLOR_ABORTED_ANIME,
  //BALL_COLOR_NOTBUILT,
  //BALL_COLOR_NOTBUILT_ANIME
};

// The default address of all BlinkMs
byte blinkm_addr = 0x09; 

// set this if you're plugging a BlinkM directly into an Arduino,
// into the standard position on analog in pins 2,3,4,5
// otherwise you can set it to false or just leave it alone
const boolean BLINKM_ARDUINO_POWERED = true;

// We store off the last ball color so that we can 
// short circuit updates if the color  hasn't actually
// changed. We start in an UNKNOWN state.
int lastBallColor = BALL_COLOR_UNKNOWN;

EthernetClient client;

String response;

// Blatantly stolen from the BlinkM examples. This function will
// attempt to discover the address for the BlinkM.
void lookForBlinkM() {
  int address = BlinkM_findFirstI2CDevice();
  if(address == -1) {
    Serial.println("No I2C devices found");
  } else { 
    Serial.print("Device found at addr ");
    Serial.println(address, DEC);
    blinkm_addr = address;
  }
}

void processResponse(String *response) {

  if(response->length() == 0) {
    Serial.println("No data received...");
    return;
  }

  Serial.print("Response: ");
  Serial.println(*response);

  // Check to see if we received a response we expect
  if(response->indexOf("{\"color\":") < 0) {
    Serial.println("Received unexpected response...");
    return;
  }

  int ballColor = resolveBallColor(response);
  Serial.print("Resolved ball color: ");
  Serial.println(ballColor);
  if(ballColor == lastBallColor) {
    Serial.println("Ball color has not changed...");
    return;
  }
  
  lastBallColor = ballColor;
  updateBlinkM(ballColor);
}

int resolveBallColor(String *response) {
  if(response->indexOf("_anime") < 0) {
    return resolveBallColorStatic(response);
  } else {
    return resolveBallColorAnime(response);
  }
}

int resolveBallColorStatic(String *response) {
  Serial.println("resolveBallColorStatic(): BEGIN");
  
  if(response->indexOf("red") >= 0) {
    return BALL_COLOR_RED;
  }
  if(response->indexOf("yellow") >= 0) {
    return BALL_COLOR_YELLOW;
  }
  if(response->indexOf("blue") >= 0) {
    return BALL_COLOR_BLUE;
  }
  if(response->indexOf("grey") >= 0) {
    return BALL_COLOR_GREY;
  }
  
  Serial.println("Failed to resolve static ball color...");
  return BALL_COLOR_UNKNOWN;
}

int resolveBallColorAnime(String *response) {
  Serial.println("resolveBallColorAnime(): BEGIN");
  
  if(response->indexOf("red") >= 0) {
    return BALL_COLOR_RED_ANIME;
  }
  if(response->indexOf("yellow") >= 0) {
    return BALL_COLOR_YELLOW_ANIME;
  }
  if(response->indexOf("blue") >= 0) {
    return BALL_COLOR_BLUE_ANIME;
  }
  if(response->indexOf("grey") >= 0) {
    return BALL_COLOR_GREY_ANIME;
  }
  
  Serial.println("Failed to resolve anime ball color...");
  return BALL_COLOR_UNKNOWN;
}

// From jenkins core/src/main/java/hudson/util/ColorPalette.java
void updateBlinkM(int ballColor) {
  
  BlinkM_off(blinkm_addr);
  
  switch(ballColor) {
    case BALL_COLOR_RED:
      BlinkM_fadeToRGB( blinkm_addr, 0xff, 0x00, 0x00);
      break;
    case BALL_COLOR_YELLOW:
      BlinkM_fadeToRGB( blinkm_addr, 0xff,0xff,0x00);
      break;
    case BALL_COLOR_BLUE:
      BlinkM_fadeToRGB( blinkm_addr, 0x00,0x00,0xff);
      break;
    case BALL_COLOR_GREY:
      BlinkM_fadeToRGB( blinkm_addr, 0x80,0x80,0x80);
      break;
    case BALL_COLOR_RED_ANIME:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_red, 0, script_lines_blink_red);
      BlinkM_playScript( blinkm_addr, 0,0,0 );
      break;
    case BALL_COLOR_YELLOW_ANIME:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_yellow, 0, script_lines_blink_yellow);
      BlinkM_playScript( blinkm_addr, 0,0,0 );
      break;
    case BALL_COLOR_BLUE_ANIME:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_blue, 0, script_lines_blink_blue);
      BlinkM_playScript( blinkm_addr, 0,0,0 );
      break;
    case BALL_COLOR_GREY_ANIME:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_grey, 0, script_lines_blink_grey);
      BlinkM_playScript( blinkm_addr, 0,0,0 );
      break;
    case BALL_COLOR_UNKNOWN:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_unknown, 0, script_lines_blink_unknown);
      BlinkM_playScript( blinkm_addr, 0,0,0 );
      break;
  }
}

boolean sendRequest() {
  
  Serial.println("Connecting to server...");
  if (client.connect(JENKINS_HOST, JENKINS_PORT)) {
   
    String request = "GET /job/";
    request += JENKINS_JOB_NAME;
    request += "/api/json?tree=color HTTP/1.1";
    
    String host = "HOST: ";
    host += JENKINS_HOST;
        
    Serial.print("Request: ");
    Serial.println(request);
    Serial.print("HOST: ");
    Serial.println(host);
        
    // make HTTP GET request:
    client.println(request);
    client.println(host);
    client.println();
    
    return true;
  } else {
    Serial.println("Failed to connect...");
    return false;
  }
}   

void setup() {
  
  // Give me time to get the serial monitor up...
  delay(5000);
  
  // Strings have a tendancy to hit memory issues. Reserve some space up front...
  response.reserve(256);
  
  if(BLINKM_ARDUINO_POWERED) {
    BlinkM_beginWithPower();
  } else {
    BlinkM_begin();
  }

  delay(100); // wait a bit for things to stabilize
  
  lookForBlinkM();
  
  byte result = BlinkM_checkAddress( blinkm_addr );
  if(result == -1) 
    Serial.println("No response from Blinkm");
  else if(result == 1) 
    Serial.println("Address mismatch for Blinkm");
    
  BlinkM_off(blinkm_addr);
   
  Serial.begin(115200);
  
  Serial.println("Obtaining an IP address using DHCP");
  if (!Ethernet.begin(myMACAddress)) {
    // if DHCP fails, start with a hard-coded address:
    Serial.println("Failed to obtain an IP address using DHCP, trying manually...");
    Ethernet.begin(myMACAddress, defaultLocalIPAddress);
  }
  
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());
  
  // Start the BlinkM in the "Unknown" state until we get the first update. 
  lastBallColor = BALL_COLOR_UNKNOWN;
  updateBlinkM(BALL_COLOR_UNKNOWN);

  delay(3000);
}

void loop() {

  Serial.println("loop(): BEGIN");
  
  // Clear out any garbage before proceeding...
  client.flush();
  
  response = "";
 
  if(sendRequest()) {
    
    // Read the response. We wait for 3 seconds of silence before we assume
    // we've read everything there is to read. This handles both slow responses
    // and the case where we don't receive any response at all.
    unsigned int timeout = 3000;
    unsigned long lastReadTime = millis();
    while((millis() - lastReadTime) < timeout) {
      if(client.available()) {
        lastReadTime = millis();
        response += (char)client.read();
        if(response.endsWith("\n") && response.indexOf("{\"color\":") < 0) {
          response = "";
        }
        //Serial.print((char)client.read());
      }
    }
    
    client.stop();
  
    // Process the resulting response 
    processResponse(&response);
  }

  // Wait 10 seconds before rechecking the status. You can decrease this if
  // you want more responsiveness, but remember that each device running this
  // script will be pinging the jenkins server at this rate.
  delay(10000);
  
  Serial.println("loop(): END");
}
