/**************** RELEASE NOTES ****************/
/*
2.0 | 2015-02-21 | Size = 22 374 octects
   - Rebuild the program : more simply
   - Reformat the URL for Motion & Alarm & Bell like this : [motion|alarm|bell]-admin-[off|on|position|status] 
   - Only two function for web page : renderWebValue[Binary | Decimal]();
   - Only three functions : getInfoAlarmAdmin() = Administrate alarm | getInfoBellAdmin() = Administrate the bell | getInfoEquipements() = Check all equipements
   - Use MCP 23017 Library for connect all equipements and have more pin ; default address = 0x20 (=0)
   - Add 3 static vars for Siren Outdoor
   - Add 2 static vars for Siren Indoor
   - Add 1 static var for Siren Box
   - 90% of Serial.print is disabled
   - Add function for manage the Siren : Box & Indoor & Outdoor
   - Add flag for Siren > Used or not ; By default = Siren Box ON | Siren Outdoor & Indoor OFF
  
1.5 | 2015-02-17 | Size = 21 766 octects
   /!\ Fix : All motion sensor are enabled when one motion are activated in function manageMotionPosition()
   /!\ Fix : Increase size of case 150 to 130 (at 100 > The web pase still not complete rendered)
   /!\ Issue : Temperature called two times ; Probably the buffer is too large...
   
1.4 | 2015-02-16 | Size = 21 728 octects
   - Allow to enable motion sensor on web page
   - Get Status of Motion on web page, add new produre : manageMotionPosition() ; by default all motion is OFF
   - By default all motion enable ; after init all motion put as OFF and can be managed from the web
   /!\ Issue : Temperature called two times ; Probably the buffer is too large...
   
1.3 | 2015-02-10 | Size = 21 358 octects
   /!\ Fix : the parameters was never saved > only tamper requested in function sendAlertJeedom and same value sended > 1
   /!\ Issue : Temperature called two times ; Probably the buffer is too large...
   
1.2 | 2015-02-09 | Size = 21 252 octects
   - All URL need to have "&" parameters at the end of the web string, upadte STR_WEB_END_PARAMETERS for new value
   - Parse the web URL to get the value parameters in : handleTemperature() 
   - Parse the web URL to get the value parameters in : sendAlertJeedom()
   - Convert char * to Integer used in atoi((char *)arrValue[0]) in : handleTemperature() & sendAlertJeedom()
   /!\ Issue : Temperature called two times ; Probably the buffer is too large...
  
1.1 | 2015-02-07 | Size = 21 342 octects
   - Add new URL for get status of Bell : ON | OFF
   - Reset status of the bell in function resetAllStatusOnSensor()
   - Add text to specify the type of Sensor requested in function sendAlertJeedom()
   - Add in the startup program the version and programme name
   /!\ Fix : At startup after test Alarm, reset variables
   /!\ Issue : Temperature called two times ; Probably the buffer is too large...
  
1.0 | 2015-02-07 | Size = 21 174 octects
   - Update all URL for Alarm
   - Add new URL for get status of alarm : ON | OFF
   - At startup alarm is turned OFF
*/
/**************** RELEASE NOTES ****************/
#include <EtherCard.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_MCP23017.h>

#define APP_NAME "ardui HomeSecurity"
#define APP_VERSION "v2.0"

#define INT_NB_SENSORS 3

/* 
 * -----------------------------------------
 * ---  Network
 * -----------------------------------------
 */
// Const
#define INT_PIN_NETWORK 10
#define INT_LAN_BUFFER 130

// Network Information
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 }; 
static byte myip[] = { 192,168,21,20 };
static byte mymask[] = { 255,255,255,0 };
static byte gwip[] = { 192,168,21,1 };
static byte dns[] = { 192,168,21,1 };
byte Ethernet::buffer[INT_LAN_BUFFER];
int intHostPort = 80;
word pos;

// Clear Cache
static void clearNetworkBuffer(void){
  // Clear buffer
  for(int i=0;i<INT_LAN_BUFFER;i++){
    Ethernet::buffer[i] = 0;
  }
}
  
// Called when a ping comes in (replies to it are automatic)
static void gotPinged (byte* ptr) {
  ether.printIp(">>> Ping received from: ", ptr);
}

// Initialise inferface
static void initNetwork(void) {
  if (ether.begin(sizeof Ethernet::buffer, mymac, INT_PIN_NETWORK) == 0) {
    Serial.println( "Failed to access Ethernet controller");
  }
  // Information
  ether.staticSetup(myip, gwip);
  ether.hisport = intHostPort;
  // Netmask
  ether.mymask[0] = mymask[0];
  ether.mymask[1] = mymask[1];
  ether.mymask[2] = mymask[2];
  ether.mymask[3] = mymask[3];
  // call this to report others pinging us
  ether.registerPingCallback(gotPinged);
}

/* 
 * -----------------------------------------
 * ---  Web Server
 * -----------------------------------------
 */
#define STR_WEB_END_PARAMETERS "&"
BufferFiller bfillWebPage;
char *arrParameter[1]; // Array for get the parameter
char *arrValue[1]; // Array for get the values of the parameter
// Send a value via web server
static void renderWebValueBinary(int intOutputWebPage) {
  bfillWebPage = ether.tcpOffset();
  bfillWebPage.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "$D"), intOutputWebPage);
  ether.httpServerReply(bfillWebPage.position());
}
// Send a value via web server
static void renderWebValueDecimal(char strOutputWebPage[10]) {
  bfillWebPage = ether.tcpOffset();
  bfillWebPage.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "$S"), strOutputWebPage);
  ether.httpServerReply(bfillWebPage.position());
}
/* 
 * -----------------------------------------
 * ---  Temperature
 * -----------------------------------------
 */
int arrBusTemperature[INT_NB_SENSORS] = {3, 4, 5}; // Connection Bus ID for Temperature
float intTemperature = 0.01;
char strTemperature[10];
int intIdGetTemp = 0;

// Get Temperature information from GET parameter
static void getTemperature(void){
  // Parse the Web URL
  char *token = strtok((char *)Ethernet::buffer + pos, STR_WEB_END_PARAMETERS);  // Get everything up to the parameter
  char *arrValue[1]; // Array for get the values of the parameter
  strtok_r(token, "=", arrValue); // Split the data all values after the parameter will be register in the variable
  // Convertion String to Integer with a cast in char *
  intIdGetTemp = atoi((char *)arrValue[0]);
  // Get The value on Dallas Temperature
  byte intAddress = arrBusTemperature[intIdGetTemp];
  OneWire oneWire(intAddress);
  DallasTemperature sensors(&oneWire);
  sensors.begin();
  sensors.requestTemperatures();
  intTemperature = sensors.getTempCByIndex(0);
  // Send value
  dtostrf(intTemperature,6,2,strTemperature);
  renderWebValueDecimal(strTemperature);
  //Serial.println("Request : " + (String)intIdGetTemp + " ; Value : " + (String)strTemperature + " \260C");
}

/* 
 * -----------------------------------------
 * ---  ALARM
 * -----------------------------------------
 */
// MCP
Adafruit_MCP23017 mcpEquipements;
  
// Varialbes
int arrAlarmStatusSensor[INT_NB_SENSORS] ={1, 1, 1}; // Statut du capteur 1 = OK ; 0 = KO
int arrAlarmStatusTamper[INT_NB_SENSORS] ={1, 1, 1}; // Statut du capteur 1 = OK ; 0 = KO
int arrSensorList[INT_NB_SENSORS] ={1, 1, 1}; // List Sensor Enabled
int arrBusSensor[INT_NB_SENSORS] = {0, 2, 4}; // Connection Bus ID for Sensor
int arrBusTamper[INT_NB_SENSORS] = {1, 3, 5}; // Connection Bus ID for Tamper
String arrSensorRoom[INT_NB_SENSORS] = {"Salon","Cuisine","Couloir"}; // List Sensor Name

// Bell OutDoor
#define INT_BELL_OUTDOOR_TAMPER 13
#define INT_BELL_OUTDOOR_LED 14
#define INT_BELL_OUTDOOR_SIREN 15
// Bell InDoor
#define INT_BELL_INDOOR_LED 10
#define INT_BELL_INDOOR_SIREN 11
// Bell InDoor
#define INT_BELL_BOX_SIREN 8

// For Loop
long previousMillis = 0;
long intIntervalMotion = 200;
// Put this value to 1 if you want to use the tamper
#define INT_ALARM_CHECK_TAMPER 0
// Variables
boolean blnAlarmActivated = false;
int blnBellBoxActivated = 1;
int blnBellOutdoorActivated = 1;
int blnBellIndoorActivated = 1;
// for Bell
int intFlagBellBox = 1;
int intFlagBellIndoor = 0;
int intFlagBellOutdoor = 0;
// For Alarm
int intIdGetAlarm = 1;
int intFlagAlarm = 1;
int intIdRequested = 0;
// Vars
int intPinSensorValue = 0;
int intPinTamperValue = 0;

// Enable box buzzer
static void activeBell(void) {
  // Log
  intFlagAlarm = 0;
  // BELL INDOOR
  if(intFlagBellIndoor == 1) {
    blnBellIndoorActivated = 0;
    mcpEquipements.digitalWrite(INT_BELL_OUTDOOR_LED, HIGH);
    mcpEquipements.digitalWrite(INT_BELL_OUTDOOR_SIREN, HIGH);
  }
  // BELL OUTDOOR
  if(intFlagBellOutdoor == 1) {
    blnBellOutdoorActivated = 0;
    mcpEquipements.digitalWrite(INT_BELL_INDOOR_LED, HIGH);
    mcpEquipements.digitalWrite(INT_BELL_INDOOR_SIREN, HIGH);
  }
  // BELL BOX : Flash alarm
  if(intFlagBellBox == 1) {
    blnBellBoxActivated = 0;
    for(int i=0;i<INT_NB_SENSORS;i++){  
      delay(100);
      mcpEquipements.digitalWrite(INT_BELL_BOX_SIREN, HIGH);
      delay(100);
      mcpEquipements.digitalWrite(INT_BELL_BOX_SIREN, LOW);
    }
  }
}

// This function allow to administrate the alarm : value = indoor | outdoor | box
static void getInfoBellAdmin(void) {
  // Initialize 
  int intStatusResult = 0;
  // Parse the Web URL
  char *ethernetValue = (char *)Ethernet::buffer + pos;
  char *token = strtok(ethernetValue, STR_WEB_END_PARAMETERS);  // Get everything up to the parameter
  char *arrValue[1]; // Array for get the values of the parameter
  strtok_r(token, "?", arrParameter); // Split the data all values after the parameter will be register in the variable
  char *arrGetParameter = strtok_r(arrParameter[0], "=", arrValue); // Split the data all values after the parameter will be register in the variable
  // Bell ON
  if(strstr((char *)Ethernet::buffer + pos, "GET /bell-admin-on") != 0) {
    intStatusResult = 1; //Serial.print("Bell Admin On ");
    if(strstr((char *)arrValue[0], "box") != 0) intFlagBellBox = intStatusResult;
    if(strstr((char *)arrValue[0], "indoor") != 0) intFlagBellIndoor = intStatusResult;
    if(strstr((char *)arrValue[0], "outdoor") != 0) intFlagBellOutdoor = intStatusResult;
  }
  // Bell OFF
  if(strstr((char *)Ethernet::buffer + pos, "GET /bell-admin-off") != 0) {
    intStatusResult = 0; //Serial.print("Bell Admin Off ");
    if(strstr((char *)arrValue[0], "box") != 0) intFlagBellBox = intStatusResult;
    if(strstr((char *)arrValue[0], "indoor") != 0) intFlagBellIndoor = intStatusResult;
    if(strstr((char *)arrValue[0], "outdoor") != 0) intFlagBellOutdoor = intStatusResult;
  }
  // Bell get status
  if(strstr((char *)Ethernet::buffer + pos, "GET /bell-admin-position") != 0) {
    //Serial.print("Bell Status ");
    if(strstr((char *)arrValue[0], "box") != 0) intStatusResult = intFlagBellBox;
    if(strstr((char *)arrValue[0], "indoor") != 0) intStatusResult = intFlagBellIndoor;
    if(strstr((char *)arrValue[0], "outdoor") != 0) intStatusResult = intFlagBellOutdoor;
  }
  // Bell get status
  if(strstr((char *)Ethernet::buffer + pos, "GET /bell-admin-status") != 0) {
    //Serial.print("Bell Position ");
    if(strstr((char *)arrValue[0], "box") != 0) intStatusResult = blnBellBoxActivated;
    if(strstr((char *)arrValue[0], "indoor") != 0) intStatusResult = blnBellOutdoorActivated;
    if(strstr((char *)arrValue[0], "outdoor") != 0) intStatusResult = blnBellIndoorActivated;
  }
  //Serial.println(" Request " + (String)intStatusResult);
  // Render Web Page
  renderWebValueBinary(intStatusResult);
}

// This function allow to administrate the alarm 
static void getInfoAlarmAdmin(void) {
  // Initialize 
  int intStatusResult = 0;
  char *ethernetValue = (char *)Ethernet::buffer + pos;
  // Alarm ON
  if(strstr(ethernetValue, "GET /alarm-admin-on?") != 0) {
    blnAlarmActivated = true;
    intStatusResult = 1;
    //Serial.print("Alarm Admin On ");
  }
  // Alarm OFF
  if(strstr(ethernetValue, "GET /alarm-admin-off?") != 0) {
    blnAlarmActivated = false; 
    intFlagAlarm = 1;
    intStatusResult = 1;
    //Serial.print("Alarm Admin Off ");
  }
  // Alarm All status reseted
  if(strstr(ethernetValue, "GET /alarm-admin-reset?") != 0) {
    // Reset all status
    for(int j=0;j<INT_NB_SENSORS;j++){
      arrAlarmStatusSensor[j] = 1;
      arrAlarmStatusTamper[j] = 1;
    }
    // Reset
    intIdGetAlarm = 1;
    intFlagAlarm = 1;
    blnBellBoxActivated = 1;
    intStatusResult = 1;
    //Serial.print("Alarm Admin Reset ");
  }
  // Alarm get status
  if(strstr(ethernetValue, "GET /alarm-admin-status?") != 0) {
    intStatusResult = 0; 
    if(blnAlarmActivated == true) intStatusResult = 1;
    //Serial.print("Alarm Admin Status ");
  }
  //Serial.println(" Request " + (String)intStatusResult);
  // Render Web Page
  renderWebValueBinary(intStatusResult);
}

// This function include all actions for motion
static void getInfoEquipements(void) {
  // Variable
  int intStatusMotion = 0;
  // Parse the Web URL
  char *ethernetValue = (char *)Ethernet::buffer + pos;
  char *token = strtok(ethernetValue, STR_WEB_END_PARAMETERS);  // Get everything up to the parameter
  char *arrValue[1]; // Array for get the values of the parameter
  strtok_r(token, "?", arrParameter); // Split the data all values after the parameter will be register in the variable
  char *arrGetParameter = strtok_r(arrParameter[0], "=", arrValue); // Split the data all values after the parameter will be register in the variable
  // Convertion String to Integer with a cast in char *
  intIdRequested = atoi((char *)arrValue[0]);
  // Put Sensor OFF
  if(strstr(ethernetValue, "GET /motion-admin-off") != 0) {
    intStatusMotion = 0;
    arrSensorList[(byte)intIdRequested] = intStatusMotion; 
    //Serial.print("Motion Admin Off ");
  }
  // Put Sensor ON
  if(strstr(ethernetValue, "GET /motion-admin-on") != 0) {
    intStatusMotion = 1;
    arrSensorList[(byte)intIdRequested] = intStatusMotion; 
    //Serial.print("Motion Admin On ");
  }
  // Get Sensor status
  if(strstr(ethernetValue, "GET /motion-admin-position") != 0) {
    intStatusMotion = arrSensorList[(byte)intIdRequested];
    //Serial.print("Motion Position ");
  }
  // Get Sensor status
  if(strstr(ethernetValue, "GET /motion-admin-status") != 0) {
    // If Motion 
    if(strstr(arrGetParameter, "M") != 0) {
      intStatusMotion = arrAlarmStatusSensor[(byte)intIdRequested];
    } else {
      intStatusMotion = arrAlarmStatusTamper[(byte)intIdRequested];
    }
    //Serial.print("Motion Status ");
  }
  //Serial.println("Requested : " + (String)intIdRequested + " ; Value : " + (String)intStatusMotion + " ; Parameters : " + (String)arrGetParameter);
  // Render Web Page
  renderWebValueBinary(intStatusMotion);
}

// Check All motion
static void handleAlarm(void) {
  // Iteration on each sensor enabled
  for(int intRowMotion=0;intRowMotion<INT_NB_SENSORS;intRowMotion++){
    // MOTION
    if(arrSensorList[intRowMotion] > 0) {
      // Get Value for each pin
      intPinSensorValue = mcpEquipements.digitalRead(arrBusSensor[intRowMotion]);
      intPinTamperValue = mcpEquipements.digitalRead(arrBusTamper[intRowMotion]);
      // Motion
      if(intPinSensorValue == 1) {
        //Serial.print(">>> Motion detected on ");Serial.println(arrSensorRoom[intRowMotion]);
        arrAlarmStatusSensor[intRowMotion] = 0; 
        activeBell();
      }
      // Box
      if(INT_ALARM_CHECK_TAMPER == 1) {
        if(intPinTamperValue == 0) {
          arrAlarmStatusTamper[intRowMotion] = 0;
          activeBell(); 
        }
      }
    }
  }
  
  // OUTDOOR : Tamper
  if(intFlagBellOutdoor == 1) {
    intPinSensorValue = mcpEquipements.digitalRead(INT_BELL_OUTDOOR_TAMPER);
    if(intPinSensorValue == 0) {
      //Serial.println(">>> Box Open on Siren Outdoor");
      blnBellOutdoorActivated = 0; 
      activeBell();
    }
  }
}

// Initialise Pin
static void initAlarm(void) {
  // BELL BOX
  mcpEquipements.pinMode(INT_BELL_BOX_SIREN, OUTPUT);
  mcpEquipements.digitalWrite(INT_BELL_BOX_SIREN, LOW);
  // BELL INDOOR
  mcpEquipements.pinMode(INT_BELL_INDOOR_SIREN, OUTPUT);
  mcpEquipements.digitalWrite(INT_BELL_INDOOR_SIREN, LOW);
  // BELL OUTDOOR
  mcpEquipements.pinMode(INT_BELL_OUTDOOR_LED, OUTPUT);
  mcpEquipements.digitalWrite(INT_BELL_OUTDOOR_LED, LOW);
  mcpEquipements.pinMode(INT_BELL_OUTDOOR_SIREN, OUTPUT);
  mcpEquipements.digitalWrite(INT_BELL_OUTDOOR_SIREN, LOW);
  mcpEquipements.pinMode(INT_BELL_OUTDOOR_TAMPER, INPUT);
  mcpEquipements.pullUp(INT_BELL_OUTDOOR_TAMPER, HIGH); 
  // MOTION
  for(int intRow=0;intRow<INT_NB_SENSORS;intRow++){
    // PIR & TAMPER = INPUT
    mcpEquipements.pinMode(arrBusSensor[intRow], INPUT);  
    mcpEquipements.pinMode(arrBusTamper[intRow], INPUT);
    // PIR & TAMPER = PULL-UP RESISTOR ENABLE
    mcpEquipements.pullUp(arrBusSensor[intRow], HIGH);  
    mcpEquipements.pullUp(arrBusTamper[intRow], HIGH);
    // Put the value to OFF
    arrSensorList[(byte)intRow] = 0;
  }
  // Test
  activeBell();
  // Reset status after 
  intFlagAlarm = 1;
  blnBellBoxActivated = 1;
  blnBellIndoorActivated = 1;
  blnBellOutdoorActivated = 1;
}

/* 
 * -----------------------------------------
 * ---  MAIN
 * -----------------------------------------
 */
void setup(void) {
  // start serial port
  Serial.begin(57600);
  Serial.println("- - - - - - - - - - - - - - -");
  Serial.println("  " + (String)APP_NAME + " - " + (String)APP_VERSION);
  Serial.println("- - - - - - - - - - - - - - -");
  // Initialize Network Interface
  initNetwork();
  // Initialise
  mcpEquipements.begin();
  // Initialise pin for Alarm
  initAlarm();
}
 
void loop (void) {
  /*
   * NETWORK
   */
  pos = ether.packetLoop(ether.packetReceive());
  
  /*
   * ALARM
   */ 
  // save value millis()
  unsigned long currentMillis = millis(); 
  if(currentMillis - previousMillis > intIntervalMotion) {
    // save value millis() 
    previousMillis = currentMillis;
    // If alarm activated
    if(blnAlarmActivated == true) handleAlarm();
  }
  // Web Request...Get Status = On or Off | Reset all value for motion | Manage Position : On or Off
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-admin-") != 0) { 
    getInfoAlarmAdmin();
    clearNetworkBuffer();  
  }
  // Web Request...Get Status = On or Off | Reset all value for motion | Manage Position : On or Off
  if(strstr((char *)Ethernet::buffer + pos, "GET /bell-admin-") != 0) { 
    getInfoBellAdmin();
    clearNetworkBuffer();  
  }
  // Web Request...Get Status = Intrusion or not | Get Position = Active or Disabled | Manage Position : On or Off
  if(strstr((char *)Ethernet::buffer + pos, "GET /motion-admin-") != 0) { 
    getInfoEquipements();
    clearNetworkBuffer();
    // For stop the Bell
    if(intFlagAlarm == 0) {
      intFlagAlarm = 1;
    }   
  }
  /*
   * TEMPERATURE
   */
  if(strstr((char *)Ethernet::buffer + pos, "GET /sonde-dallas?") != 0) { 
    getTemperature();
    clearNetworkBuffer();
  }
}
