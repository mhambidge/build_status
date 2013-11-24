// 
// Arduino 1.0.1 apparently had some weird issues with strings that result in failure
// to properly read the incoming data from the Xbee. Updating to Arduino 1.0.4 resolved
// the issue.
// 
#include <Wire.h>
// BlinkMCommunicator available at http://blinkm-projects.googlecode.com/files/BlinkMCommunicator.zip
#include <BlinkM_funcs.h>
// SoftwareSerial is now included by default with Arduino
#include <SoftwareSerial.h>

// The appropriate protocol (http, https), hostname or ip, port if non-standard
// of the Jenkins server.
#define JENKINS_HOST "http://192.168.1.5:8080"

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
// TODO: MTH: prefix names
enum BallColor {
  UNKNOWN,
  RED, 
  RED_ANIME,
  YELLOW,
  YELLOW_ANIME,
  BLUE,
  BLUE_ANIME,
  GREY,
  GREY_ANIME
  // We don't need the following since they all resolve to "grey" in the json
  //DISABLED,
  //DISABLED_ANIME,
  //ABORTED,
  //ABORTED_ANIME,
  //NOTBUILT,
  //NOTBUILT_ANIME
};

// The default address of all BlinkMs
byte blinkm_addr = 0x09; 

// set this if you're plugging a BlinkM directly into an Arduino,
// into the standard position on analog in pins 2,3,4,5
// otherwise you can set it to false or just leave it alone
const boolean BLINKM_ARDUINO_POWERED = true;

// I chose to use SoftwareSerial to interface with
// the xbee so that I could use the hardware serial for 
// printing debug information. This is quite handy when
// running while attached via USB to your computer as you
// can view the debug information in the terminal interface.

// I'm using the Sparkfun xbee shield which has a switch that
// allows you to switch from using the UART to using digital
// pin 2 and 3 for RX/TX (TODO: check order). To use 
// SoftwareSerial you need to switch it to the digital pin
// option. If you are instead using a breadboard you can, of 
// course, use whatever digital pins you want and just need
// to change the 2, 3 below accordingly.
SoftwareSerial xbeeSerial(2, 3);

// We store off the last ball color so that we can 
// short circuit updates if the color  hasn't actually
// changed. We start in an UNKNOWN state.
int lastBallColor = UNKNOWN;

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
    return RED;
  }
  if(response->indexOf("yellow") >= 0) {
    return YELLOW;
  }
  if(response->indexOf("blue") >= 0) {
    return BLUE;
  }
  if(response->indexOf("grey") >= 0) {
    return GREY;
  }
  
  Serial.println("Failed to resolve static ball color...");
  return UNKNOWN;
}

int resolveBallColorAnime(String *response) {
  Serial.println("resolveBallColorAnime(): BEGIN");
  
  if(response->indexOf("red") >= 0) {
    return RED_ANIME;
  }
  if(response->indexOf("yellow") >= 0) {
    return YELLOW_ANIME;
  }
  if(response->indexOf("blue") >= 0) {
    return BLUE_ANIME;
  }
  if(response->indexOf("grey") >= 0) {
    return GREY_ANIME;
  }
  
  Serial.println("Failed to resolve anime ball color...");
  return UNKNOWN;
}

// From jenkins core/src/main/java/hudson/util/ColorPalette.java
void updateBlinkM(int ballColor) {
  
  BlinkM_off(blinkm_addr);
  
  switch(ballColor) {
    case RED:
      BlinkM_fadeToRGB( blinkm_addr, 0xff, 0x00, 0x00);
      break;
    case YELLOW:
      BlinkM_fadeToRGB( blinkm_addr, 0xff, 0xff, 0x00);
      break;
    case BLUE:
      BlinkM_fadeToRGB( blinkm_addr, 0x00, 0x00, 0xff);
      break;
    case GREY:
      BlinkM_fadeToRGB( blinkm_addr, 0x80, 0x80, 0x80);
      break;
    case RED_ANIME:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_red, 0, script_lines_blink_red);
      BlinkM_playScript( blinkm_addr, 0, 0, 0 );
      break;
    case YELLOW_ANIME:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_yellow, 0, script_lines_blink_yellow);
      BlinkM_playScript( blinkm_addr, 0, 0, 0 );
      break;
    case BLUE_ANIME:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_blue, 0, script_lines_blink_blue);
      BlinkM_playScript( blinkm_addr, 0, 0, 0 );
      break;
    case GREY_ANIME:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_grey, 0, script_lines_blink_grey);
      BlinkM_playScript( blinkm_addr, 0, 0, 0 );
      break;
    case UNKNOWN:
      BlinkM_writeScript( blinkm_addr, 0, script_len_blink_unknown, 0, script_lines_blink_unknown);
      BlinkM_playScript( blinkm_addr, 0, 0, 0 );
      break;
  }
}


void setup() {
  
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
  xbeeSerial.begin(9600);
  
  delay(3000);
  
  lastBallColor = UNKNOWN;
  updateBlinkM(UNKNOWN);
}

void loop() {

  Serial.println("loop(): BEGIN");
  
  // Clear out any garbage from the xbee before proceeding...
  xbeeSerial.flush();
 
  // Build the request that we will send to Jenkins to obtain the build status.
  // I originally used the ajson library to try to parse th full response from
  // the jenkins rest api for the Job page. Unfortunately, there wasn't enough
  // free memory to do both this and use the SoftwareSerial library. The
  // workaround was to have Jenkins filter the result for me by including the
  // tree=color query parameter. This results in jenkins returning a very 
  // manageable response of: {"color":"blue"} or similar.
  String request = JENKINS_HOST;
  if(!request.endsWith("/")) {
    request += "/";
  }
  request += "job/";
  request += JENKINS_JOB_NAME;
  request += "/api/json?tree=color";
  Serial.print("Request: ");
  Serial.println(request);
  
  // Send the request to Jenkins via the xbee and the ConnectPort X2
  xbeeSerial.print(request);
  xbeeSerial.print("\r");

  // Read the response. We wait for 3 seconds of silence before we assume
  // we've read everything there is to read. This handle both slow responses
  // and the case where we don't receive any response at all.
  String response; 
  unsigned int timeout = 3000;
  unsigned long lastReadTime = millis();
  while((millis() - lastReadTime) < timeout) {
    if(xbeeSerial.available()) {
      lastReadTime = millis();
      response += (char)xbeeSerial.read();
    }
  }
  
  // Process the resulting response 
  processResponse(&response);
  
  // Wait 10 seconds before rechecking the status. You can decrease this if
  // you want more responsiveness, but remember that each device running this
  // script will be pinging the jenkins server at this rate.
  delay(10000);
  
  Serial.println("loop(): END");
}
