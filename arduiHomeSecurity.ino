/**************** RELEASE NOTES ****************/
/*
1.4 | 2015-02-17 | Size = 21 728 octects
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

#define APP_NAME "ardui HomeSecurity"
#define APP_VERSION "v1.4"
long previousMillis = 0;
long intIntervalMotion = 500; 


/* 
 * -----------------------------------------
 * ---  Network
 * -----------------------------------------
 */
// Const
#define INT_PIN_NETWORK 10
#define INT_LAN_BUFFER 150
// Network Information
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 }; 
static byte myip[] = { 192,168,21,20 };
static byte mymask[] = { 255,255,255,0 };
static byte gwip[] = { 192,168,21,1 };
static byte dns[] = { 192,168,21,1 };
byte Ethernet::buffer[INT_LAN_BUFFER];
int intHostPort = 80;
word pos;
/*
 * Clear cache
 */
static void clearNetworkBuffer(void){
  // Clear buffer
  for(int i=0;i<INT_LAN_BUFFER;i++){
    Ethernet::buffer[i] = 0;
  }
}
  
/*
 * NETWORK : Called when a ping comes in (replies to it are automatic)
 */
static void gotPinged (byte* ptr) {
  ether.printIp(">>> Ping received from: ", ptr);
}

/*
 * NETWORK : Initialise inferface
 */
static void initNetwork(void) {
  // Log
  Serial.println(">>> Network configuration ongoing...");
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
  // DNS
  ether.dnsip[0] = dns[0];
  ether.dnsip[1] = dns[1];
  ether.dnsip[2] = dns[2];
  ether.dnsip[3] = dns[3];
  // Log
  ether.printIp(">>> Network IP : ", ether.myip);
  ether.printIp(">>> Network Netmask : ", ether.mymask);
  ether.printIp(">>> Network Gateway : ", ether.gwip);
  ether.printIp(">>> Network DNS : ", ether.dnsip);
  // call this to report others pinging us
  ether.registerPingCallback(gotPinged);
  // Log
  Serial.println(">>> Network complete...");
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

/*
 * Function for render the Web page
 */
static void renderWebPageAlarm(int intOutputWebPage) {
  bfillWebPage = ether.tcpOffset();
  bfillWebPage.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "$D"), intOutputWebPage);
  ether.httpServerReply(bfillWebPage.position());
}
/*
 * Function for render the Web page
 */
static void renderWebPageTemperature(char strOutputWebPage[10]) {
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
// Const
#define INT_NB_SENSORS 4
#define INT_PIN_LED A0
#define INT_PIN_BUZZER A1
// Variables :: Array
int arrAlarmStatusSensor[INT_NB_SENSORS] ={1, 1, 1, 1}; // Statut du capteur 1 = OK ; 0 = KO
int arrAlarmStatusTamper[INT_NB_SENSORS] ={1, 1, 1, 1}; // Statut du capteur 1 = OK ; 0 = KO
int arrSensorList[INT_NB_SENSORS] ={1, 1, 1, 1}; // List Sensor Enabled
int arrBusSensor[INT_NB_SENSORS] = {2, 4, 6, 8}; // Connection Bus ID for Sensor
int arrBusTamper[INT_NB_SENSORS] = {3, 5, 7, 9}; // Connection Bus ID for Tamper
int arrNumberMaxMotion[INT_NB_SENSORS] = {4, 4, 4, 4}; // Max Motion autorized for each sensor
String arrSensorRoom[INT_NB_SENSORS] = {"Salon", "Cuisine", "Couloir", "N/A"}; // List Sensor Name
int arrBusTemperature[INT_NB_SENSORS] = {A2, A3, A4, A5}; // Connection Bus ID for Temperature
int arrValueTemperature[INT_NB_SENSORS] = {0, 0, 0, 0}; // Connection Bus ID for Temperature
// Variables
int intPinSensorValue = 0;
int intPinTamperValue = 0;
String blnPortStatus = "";
boolean blnTemp = false;
float intTemperature = 0.01;
char strTemperature[10];
int intIdGetTemp = 0;

/*
 * Get Temperature for one Dallas Sonde
 */
static void getTemperatureById(int idAddress) {
  // Variale
  byte intAddress = arrBusTemperature[idAddress];
  // Start up the library
  OneWire oneWire(intAddress);
  DallasTemperature sensors(&oneWire);
  sensors.begin();
  sensors.requestTemperatures();
  // Log
  intTemperature = sensors.getTempCByIndex(0);
  Serial.print(">>> Pin : ");
  Serial.print(intAddress);
  Serial.print(" | Temperature is : ");
  Serial.print(intTemperature);
  Serial.println(" \260C");
}
/*
 * Get Temperature information from GET parameter
 */
static void handleTemperature(void){
  // Parse the Web URL
  char *token = strtok((char *)Ethernet::buffer + pos, STR_WEB_END_PARAMETERS);  // Get everything up to the parameter
  char *arrValue[1]; // Array for get the values of the parameter
  strtok_r(token, "=", arrValue); // Split the data all values after the parameter will be register in the variable
  // Convertion String to Integer with a cast in char *
  intIdGetTemp = atoi((char *)arrValue[0]);
  // Temperature : Sonde 0
  if(blnTemp == true) { 
    blnTemp = false;
    Serial.println("- - - - - START WEB REQUEST ");
    getTemperatureById(intIdGetTemp);
    // Format value
    dtostrf(intTemperature,6,2,strTemperature);
    // Render Web Page
    renderWebPageTemperature(strTemperature);
    Serial.println("- - - - - END WEB REQUEST");
  }

}

/* 
 * -----------------------------------------
 * ---  ALARM
 * -----------------------------------------
 */
boolean blnAlarmActivated = false;
int blnAlarmBellActivated = 1;
int intIdGetAlarm = 1;
int intFlagAlarm = 1;
int intIdRequested = 0;
/*
 * Activate Buzzer & Led
 */
static void enableAlarm(void) {
  // Log
  intFlagAlarm = 0;
  blnAlarmBellActivated = 0;
  Serial.println("!!! ALARM : Led & Buzzer Activated");
  uint8_t flashes=3;
  // Create a flash alarm
  for(int i=0;i<flashes;i++){  
    delay(100);
    digitalWrite(INT_PIN_BUZZER, HIGH);
    digitalWrite(INT_PIN_LED, HIGH);
    delay(100);
    digitalWrite(INT_PIN_BUZZER, LOW);
    digitalWrite(INT_PIN_LED, LOW);
  }
}

/*
 * Clear alert saved
 */
static void resetAllStatusOnSensor(void){
  // Clear buffer
  for(int j=0;j<INT_NB_SENSORS;j++){
    arrAlarmStatusSensor[j] = 1;
    arrAlarmStatusTamper[j] = 1;
  }
  intIdGetAlarm = 1;
  intFlagAlarm = 1;
  blnAlarmBellActivated = 1;
  Serial.println(">>> All status for Sensor motion reseted");
}
/*
 * get Action from  Jeedom : on / off / reset
 */
static void manageAlarmPosition(void) {
  // Log
  Serial.println("- - - - - START WEB REQUEST ");
  int intStatusAlarm = 0;
  // Alarm ON
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned-on?") != 0) {
    blnAlarmActivated = true;
    intStatusAlarm = 1;
    Serial.println(">>> The alarm is turned ON  ");
  }
  // Alarm OFF
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned-off?") != 0) {
    blnAlarmActivated = false;
    intFlagAlarm = 1;
    intStatusAlarm = 1;
    Serial.println(">>> The alarm is turned OFF  ");
  }
  // Alarm All status reseted
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned-reset?") != 0) {
    intStatusAlarm = 1;
    resetAllStatusOnSensor();
  }
  // Alarm get status
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned-status?") != 0) {
    intStatusAlarm = 0;
    if(blnAlarmActivated == true) intStatusAlarm = 1;
    Serial.println(">>> The alarm status sended");
  }
  // Alarm get status for bell
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned-bell?") != 0) {
    intStatusAlarm = blnAlarmBellActivated;
    Serial.println(">>> The bell status sended");
  }
  // Render Web Page
  renderWebPageAlarm(intStatusAlarm);
  Serial.println("- - - - - END WEB REQUEST");
}

/*
 * get Action from  Jeedom : on / off / status
 */
static void manageMotionPosition(void) {
  // Variable
  int intStatusMotion = 0;
  // Put Sensor OFF
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-motion-off?") != 0) {
    intStatusMotion = 0;
  }
  // Put Sensor ON
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-motion-on?") != 0) {
    intStatusMotion = 1;
  }
  // Get Sensor status
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-motion-status?") != 0) {
    intStatusMotion = arrSensorList[intIdRequested];
  }
  // Parse the Web URL
  char *token = strtok((char *)Ethernet::buffer + pos, STR_WEB_END_PARAMETERS);  // Get everything up to the parameter
  char *arrValue[1]; // Array for get the values of the parameter
  strtok_r(token, "=", arrValue); // Split the data all values after the parameter will be register in the variable
  // Convertion String to Integer with a cast in char *
  intIdRequested = atoi((char *)arrValue[0]);
  arrSensorList[intIdRequested] = intStatusMotion;
  // Log
  Serial.println("- - - - - START WEB REQUEST ");
  Serial.print(">>> Motion ID : ");
  Serial.print(intIdRequested);
  Serial.print(" | The status is : ");
  Serial.println(intStatusMotion);
  // Render Web Page
  renderWebPageAlarm(intStatusMotion);
  Serial.println("- - - - - END WEB REQUEST");
}

/*
 * Send Alert
 */
static void sendAlertJeedom(void) {
  // Parse the Web URL
  char *token = strtok((char *)Ethernet::buffer + pos, STR_WEB_END_PARAMETERS);  // Get everything up to the parameter
  strtok_r(token, "?", arrParameter); // Split the data all values after the parameter will be register in the variable
  char *arrGetParameter = strtok_r(arrParameter[0], "=", arrValue); // Split the data all values after the parameter will be register in the variable
  // Convertion String to Integer with a cast in char *
  intIdRequested = atoi((char *)arrValue[0]);
  // Log
  Serial.println("- - - - - START WEB REQUEST ");
  Serial.print(">>> ");
  // If Motion 
  if(strstr(arrGetParameter, "M") != 0) {
    intIdGetAlarm = arrAlarmStatusSensor[(byte)intIdRequested];
    Serial.print("Sensor Motion : ");
  } else {
    intIdGetAlarm = arrAlarmStatusTamper[(byte)intIdRequested];
    Serial.print("Sensor Tamper : ");
  }
  // Log
  Serial.print(intIdRequested);
  Serial.print(" | Status is : ");
  Serial.println(intIdGetAlarm);
  // Render Web Page
  renderWebPageAlarm(intIdGetAlarm);
  Serial.println("- - - - - END WEB REQUEST");
}

/*
 * Check status for each motion sensor
 */
static void handleMotion(void) {
  // Iteration on each sensor enabled
  for(int intRowMotion=0;intRowMotion<INT_NB_SENSORS;intRowMotion++){
    if(arrSensorList[intRowMotion] > 0) {
      // Get Value for each pin
      intPinSensorValue = digitalRead(arrBusSensor[intRowMotion]);
      intPinTamperValue = digitalRead(arrBusTamper[intRowMotion]);
      // Motion
      if(intPinSensorValue == 1) {
        Serial.print("!!! ALARM : Motion detected on : ");
        Serial.println(arrSensorRoom[intRowMotion]);
        arrAlarmStatusSensor[intRowMotion] = 0; 
        enableAlarm();
      }
      // Box
      if(intPinTamperValue == 1) {
        Serial.print("!!! ALARM : Box open on : ");
        Serial.println(arrSensorRoom[intRowMotion]);
        arrAlarmStatusTamper[intRowMotion] = 0;
        enableAlarm(); 
      }
    }
  }
}

/*
 * Initialise Led & Buzzer & Sensors
 */
static void initAlarm(void) {
  // Initialize Configuration LED = Output & OFF | BUZZER = Output & OFF
  Serial.println(">>> Led & Buzzer registered as [OUTPUT / OFF]");
  pinMode(INT_PIN_LED, OUTPUT);
  pinMode(INT_PIN_BUZZER, OUTPUT);
  digitalWrite(INT_PIN_BUZZER, LOW); 
  digitalWrite(INT_PIN_LED, LOW);
  // Test on startup
  enableAlarm();
  // Reset status after 
  intFlagAlarm = 1;
  blnAlarmBellActivated = 1;
  // For each sensor
  for(int intRow=0; intRow < INT_NB_SENSORS; intRow++){
    // If it's enabled
    if(arrSensorList[intRow] > 0) {
      // Log Console
      Serial.print(">>> Alarm activated on Room : ");
      Serial.println(arrSensorRoom[intRow]);
      Serial.println(">>> Register Motion Sensor & Tamper as [INPUT] & [PULL-UP RESISTOR]");  
      // PIR & TAMPER = INPUT
      pinMode(arrBusSensor[intRow], INPUT);  
      pinMode(arrBusTamper[intRow], INPUT);
      // PIR & TAMPER = PULL-UP RESISTOR ENABLE
      digitalWrite(arrBusSensor[intRow], HIGH);  
      digitalWrite(arrBusTamper[intRow], HIGH);
      // Put the value to OFF
      arrSensorList[intRow] = 0;
    } 
  }
}


/* 
 * -----------------------------------------
 * ---  MAIN
 * -----------------------------------------
 */
// Configuration
void setup(void) {
  // start serial port
  Serial.begin(57600);
  Serial.println("");
  Serial.println("- - - - - - - - - - - - - - -");
  Serial.print("  ");
  Serial.print(APP_NAME);
  Serial.print(" - ");
  Serial.println(APP_VERSION);
  Serial.println("- - - - - - - - - - - - - - -");
  Serial.println("");
  Serial.println(">>> Start setup....");
  Serial.println("");
  // STEP 01 : Initialise Alarm
  initAlarm();
  // STEP 02 : Initialize Network
  initNetwork();
  // Log
  Serial.println(">>> ALARM IS OFF  ");
  Serial.println("");
  Serial.println(">>> Ended setup....");
  Serial.println("");
}

// Iteration
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
    if(blnAlarmActivated == true) handleMotion();
  }
  // Web Request...for get status on motion
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-status?") != 0) { 
    sendAlertJeedom();
    clearNetworkBuffer();
    // For stop the Bell
    if(intFlagAlarm == 0) {
      intFlagAlarm = 1;
    }   
  }
  // Web Request...Manage position of Motion Sensor
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-motion") != 0) { 
    manageMotionPosition();
    clearNetworkBuffer();    
  }
  // Web Request...for turned on
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned") != 0) { 
    manageAlarmPosition();
    clearNetworkBuffer();  
  }
  /*
   * TEMPERATURE
   */
  if(strstr((char *)Ethernet::buffer + pos, "GET /sonde?") != 0) { 
    blnTemp = true;
    handleTemperature();
    clearNetworkBuffer();
  }
}
