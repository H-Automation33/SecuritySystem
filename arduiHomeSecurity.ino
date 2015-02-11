#include <EtherCard.h>
#include <OneWire.h>
#include <DallasTemperature.h>

long previousMillis = 0;
long intIntervalMotion = 500; 

/* 
 * -----------------------------------------
 * ---  Network
 * -----------------------------------------
 */
// Const
#define INT_PIN_NETWORK 10
#define INT_LAN_BUFFER 200
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
BufferFiller bfillWebPage;
/*
 * Function for render the Web page
 */
static void renderWebPageAlarm(char strOutputWebPage[10]) {
  bfillWebPage = ether.tcpOffset();
  bfillWebPage.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "$D"), strOutputWebPage);
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
int arrSensorList[INT_NB_SENSORS] ={0, 0, 0, 1}; // List Sensor Enabled
int arrBusSensor[INT_NB_SENSORS] = {2, 4, 6, 8}; // Connection Bus ID for Sensor
int arrBusTamper[INT_NB_SENSORS] = {3, 5, 7, 9}; // Connection Bus ID for Tamper
int arrNumberMaxMotion[INT_NB_SENSORS] = {4, 4, 4, 4}; // Max Motion autorized for each sensor
String arrSensorRoom[INT_NB_SENSORS] = {"Salon", "Cuisine", "Entree", "Bureau"}; // List Sensor Name
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
int intIdGetAlarm = 1;
int intFlagAlarm = 1;
int intIdRequested = 0;
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
  // Temperature : Sonde 0
  if(strstr((char *)Ethernet::buffer + pos, "T=0") != 0) { 
    blnTemp = true;
    intIdGetTemp = 0;
  }
  // Temperature : Sonde 1
  if(strstr((char *)Ethernet::buffer + pos, "T=1") != 0) { 
    blnTemp = true;
    intIdGetTemp = 1;
  }
  // Temperature : Sonde 2
  if(strstr((char *)Ethernet::buffer + pos, "T=2") != 0) { 
    blnTemp = true;
    intIdGetTemp = 2;
  }
  // Temperature : Sonde 3
  if(strstr((char *)Ethernet::buffer + pos, "T=3") != 0) { 
    blnTemp = true;
    intIdGetTemp = 3;
  }
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
  delay(2000);
}

/* 
 * -----------------------------------------
 * ---  ALARM
 * -----------------------------------------
 */
boolean blnAlarmActivated = false;

/*
 * Activate Buzzer & Led
 */
static void enableAlarm(void) {
  // Log
  intFlagAlarm = 0;
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
  Serial.println(">>> All status for Sensor motion reseted");
}
/*
 * get Action from  Jeedom : on / off / reset
 */
static void manageAlarmPosition(void) {
  // Log
  Serial.println("- - - - - START WEB REQUEST ");
  int intStatusAlarm = 0;
  // Check the URL
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned-on?") != 0) {
    blnAlarmActivated = true;
    intStatusAlarm = 1;
    Serial.println(">>> The alarm is turned ON  ");
  }
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned-off?") != 0) {
    blnAlarmActivated = false;
    intStatusAlarm = 1;
    Serial.println(">>> The alarm is turned OFF  ");
  }
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned-reset?") != 0) {
    intStatusAlarm = 1;
    resetAllStatusOnSensor();
  }
  if(strstr((char *)Ethernet::buffer + pos, "GET /alarm-turned-status?") != 0) {
    intStatusAlarm = 0;
    if(blnAlarmActivated == true) intStatusAlarm = 1;
  }
  // Render Web Page
  renderWebPageAlarm((char *)intStatusAlarm);
  Serial.println("- - - - - END WEB REQUEST");
}
/*
 * Send Alert
 */
static void sendAlertJeedom(void) {
  // Alarm : Sonde 0
  if(strstr((char *)Ethernet::buffer + pos, "M=0") != 0) {intIdRequested = 0; intIdGetAlarm = arrAlarmStatusSensor[0];}
  if(strstr((char *)Ethernet::buffer + pos, "T=0") != 0) {intIdRequested = 0; intIdGetAlarm = arrAlarmStatusTamper[0];}
  // Alarm : Sonde 1
  if(strstr((char *)Ethernet::buffer + pos, "M=1") != 0) {intIdRequested = 1; intIdGetAlarm = arrAlarmStatusSensor[1];}
  if(strstr((char *)Ethernet::buffer + pos, "T=1") != 0) {intIdRequested = 1; intIdGetAlarm = arrAlarmStatusTamper[1];}
  // Alarm : Sonde 2
  if(strstr((char *)Ethernet::buffer + pos, "M=2") != 0) {intIdRequested = 2; intIdGetAlarm = arrAlarmStatusSensor[2];}
  if(strstr((char *)Ethernet::buffer + pos, "T=2") != 0) {intIdRequested = 2; intIdGetAlarm = arrAlarmStatusTamper[2];}
  // Alarm : Sonde 3
  if(strstr((char *)Ethernet::buffer + pos, "M=3") != 0) {intIdRequested = 3; intIdGetAlarm = arrAlarmStatusSensor[3];}
  if(strstr((char *)Ethernet::buffer + pos, "T=3") != 0) {intIdRequested = 3; intIdGetAlarm = arrAlarmStatusTamper[3];}
  // Log
  Serial.println("- - - - - START WEB REQUEST ");
  Serial.print(">>> Sensor : ");
  Serial.print(intIdRequested);
  Serial.print(" | Status is : ");
  Serial.println(intIdGetAlarm);
  // Render Web Page
  renderWebPageAlarm((char *)intIdGetAlarm);
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
  Serial.println(">>> Start setup....");
  Serial.println("- - - - - - - - - - - - - - -");
  Serial.println("");
  // STEP 01 : Initialise Alarm
  initAlarm();
  // STEP 02 : Initialize Network
  initNetwork();
  // Log
  Serial.println("");
  Serial.println(">>> ALARM IS OFF  ");
  Serial.println("");
  Serial.println("- - - - - - - - - - - - - - -");
  Serial.println(">>> Ended setup....");
  Serial.println("- - - - - - - - - - - - - - -");
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
    if(intFlagAlarm == 0) {
      intFlagAlarm = 1;
    }   
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
    handleTemperature();
    clearNetworkBuffer();
  }
}
