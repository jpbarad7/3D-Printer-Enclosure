#define BLYNK_TEMPLATE_ID "TMPLgMbZODbI"
#define BLYNK_TEMPLATE_NAME "Taz Pro Heater"
#define BLYNK_AUTH_TOKEN "eXXc7Yc30ETML1gzk7sFkpcvgQ6RlfIY"

#define filament_heater_SSR           33
#define enclosure_heater_SSR          27
#define rp_heater_output_signal       12
#define filament_heater_status_LED    A1
#define filament_heater_coil_ON_LED   A0
#define enclosure_heater_status_LED   14
#define enclosure_heater_coil_ON_LED  A5

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_AM2315.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

int fc_old_temp;
int fc_old_hum;
int pe_old_temp;
int pe_old_hum;
int FC_Temp_set_point = 20;
int FC_TSP = 20;
int PE_Temp_set_point = 40;
int PE_TSP = 40;
int Filament_Heater;
int Filament_bake_time = 6;
int Enclosure_Heater;
int FC_hoursPassed;
int Printer_status = 1;               // Status is 'OFF' on startup - see comment below
int ON = 0;                           // So named to avoid confusion when Raspberry Pi output pin is LOW when 'ON' and HIGH when 'OFF'
int OFF = 1;                          // due to amplification of the signal through a N channel MOSFET
int Override;
int Alternate;

bool FC_timerActive = false;
bool displayTimerActive = false;      // To check if the wait timer is active
bool displayTimer2Active = false;     // To check if the wait timer is active
unsigned long displayTimerStart = 0;  // To record when the timer was started
unsigned long displayTimer2Start = 0; // To record when the timer was started
const long displayDelay = 5000;       // Delay of 5 seconds

float temperature, humidity;          // AM2315 sensor in filament chamber
float Temperature, Humidity;          // BM280 sensor in enclosure

BlynkTimer timer;
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
Adafruit_AM2315 am2315;
Adafruit_BME280 bme;

char auth[] = BLYNK_AUTH_TOKEN;

char ssid[] = "BR Guest";
char pass[] = "Pamma355!";

BLYNK_CONNECTED()
{
  display.setCursor(41,13); display.print("WiFi");
  display.setCursor(8,35); display.print("Connected");
  display.display();
  delay(2000);
  display.clearDisplay();
  display.display();
 
  Blynk.syncAll();
}

BLYNK_WRITE(V3)
{
  int FC_Temp_set_point = param.asInt();
  FC_TSP = FC_Temp_set_point;
  Blynk.virtualWrite(V2 ,FC_Temp_set_point);
}

BLYNK_WRITE(V8)
{
  int PE_Temp_set_point = param.asInt();
  PE_TSP = PE_Temp_set_point;
  Blynk.virtualWrite(V7 ,PE_Temp_set_point);
}

BLYNK_WRITE(V10) {
  int pinValue = param.asInt();
  if (pinValue == 1) {
    Filament_Heater = 1;
    FC_timerActive = true;
    FC_hoursPassed = 0;  // Reset timer
    Blynk.virtualWrite(V12, FC_hoursPassed);
    digitalWrite(filament_heater_status_LED, HIGH);
  } else {
    Filament_Heater = 0;
    FC_timerActive = false;
    digitalWrite(filament_heater_status_LED, LOW);
  }
}

BLYNK_WRITE(V13) {
  int pinValue = param.asInt();
  if (pinValue == 1) {
    Override = 1;
    digitalWrite(enclosure_heater_status_LED, HIGH);
  } else {
    Override = 0;
    digitalWrite(enclosure_heater_status_LED, LOW);
  }
}

WidgetLED led1(V4);
WidgetLED led2(V9);

void setup() 
{
  pinMode (rp_heater_output_signal, INPUT_PULLUP);   
  pinMode (filament_heater_SSR, OUTPUT);   
  pinMode (enclosure_heater_SSR, OUTPUT);
  pinMode (filament_heater_status_LED, OUTPUT);
  pinMode (enclosure_heater_status_LED, OUTPUT);
  pinMode (filament_heater_coil_ON_LED, OUTPUT);
  pinMode (enclosure_heater_coil_ON_LED, OUTPUT);

  unsigned status;
    
  am2315.begin();
  status = bme.begin();    
  display.begin(0x3C, true);

  display.setTextSize(2);
  display.setRotation(1);
  display.setTextColor(SH110X_WHITE);
 
  display.clearDisplay();
  display.display();
 
  timer.setInterval(100L, myTimerEvent);
  timer.setInterval(3600000L, FC_timerUpdate);  // Set timer for 1 hour

  Blynk.begin(auth, ssid, pass);

  Blynk.virtualWrite(V2, FC_Temp_set_point);
  Blynk.virtualWrite(V3, FC_TSP);
  Blynk.virtualWrite(V7, PE_Temp_set_point); 
  Blynk.virtualWrite(V8, PE_TSP);  
  Blynk.virtualWrite(V10, 0);
  Blynk.virtualWrite(V11, 0);
  Blynk.virtualWrite(V13, 0);

  Printer_status = digitalRead(rp_heater_output_signal);

  if (Printer_status == ON) {Enclosure_Heater = 1;}
}

void myTimerEvent()
{
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V5, Temperature);
  Blynk.virtualWrite(V6, Humidity);
}

void FilamentChamberInfo ()
{
  if (Filament_Heater) {
     if ((temperature != fc_old_temp) || (humidity != fc_old_hum)) {
        display.clearDisplay();
        display.display();

        display.setTextSize(2);
        display.setCursor(16,10);
        display.print("Filament");

        display.setTextSize(1);
        display.setCursor(8,35);
        display.print("Temperature: ");
        display.setCursor(8,45);
        display.print("Humidity: ");

        display.setCursor(88,35);
        display.print(temperature);
        display.setCursor(88,45);
        display.print(humidity);
        display.display();

        // Activate display timer and set start time
        displayTimerActive = true;
        displayTimerStart = millis();           
     }

    fc_old_temp = temperature;
    fc_old_hum = humidity;
  }
}  

void PrinterEnclosureInfo() 
{
  if (Enclosure_Heater) {
     if ((Temperature != pe_old_temp) || (Humidity != pe_old_hum)) {
        display.clearDisplay();
        display.display();

        display.setTextSize(2);
        display.setCursor(9,10);
        display.print("Enclosure");

        display.setTextSize(1);
        display.setCursor(8,35);
        display.print("Temperature: ");
        display.setCursor(8,45);
        display.print("Humidity: ");

        display.setCursor(88,35);
        display.print(Temperature);
        display.setCursor(88,45);
        display.print(Humidity);
        display.display();

        // Activate display timer and set start time
        displayTimer2Active = true;
        displayTimer2Start = millis();
     }

  pe_old_temp = Temperature;
  pe_old_hum = Humidity;
  }
} 

void FC_timerUpdate() {
  if (FC_timerActive && ((FC_hoursPassed <= Filament_bake_time) || (Printer_status == ON))) {
    FC_hoursPassed++;
    Blynk.virtualWrite(12, FC_hoursPassed); 
  }
  else {
    Filament_Heater = 0;
    FC_timerActive = false;
    Blynk.virtualWrite(V10, 0);
    digitalWrite(filament_heater_status_LED, LOW);  
  }
}

void loop()
{
  Blynk.run();
  timer.run();
  Serial.println("here");

  if (displayTimerActive && (millis() - displayTimerStart >= displayDelay)) {
          displayTimerActive = false;
          Alternate = 1;
  }
          
  if (displayTimer2Active && (millis() - displayTimer2Start >= displayDelay)) {
          displayTimer2Active = false;
          Alternate = 0;
  }
                
  am2315.readTemperatureAndHumidity(&temperature, &humidity);  
  Temperature = bme.readTemperature();
  Humidity = bme.readHumidity(); 

  Printer_status = digitalRead(rp_heater_output_signal);

  if (Printer_status) {
    if (Override) {
      Enclosure_Heater = 0;
      Blynk.virtualWrite(V11, LOW);
      digitalWrite(enclosure_heater_status_LED, LOW);  
    }
    else {
      Enclosure_Heater = 1;
      Blynk.virtualWrite(V11, HIGH);
      digitalWrite(enclosure_heater_status_LED, HIGH);
    }
  }
  else {
    Enclosure_Heater = 0;
    Blynk.virtualWrite(V11, LOW);
    Blynk.virtualWrite(V13, LOW);
    digitalWrite(enclosure_heater_status_LED, LOW);
    digitalWrite(enclosure_heater_coil_ON_LED, LOW);
    digitalWrite(enclosure_heater_SSR, LOW);
    display.clearDisplay();
    display.display();
  }

  // Filament heater temp control
  if (Filament_Heater && (temperature <= FC_TSP)) { 
      digitalWrite(filament_heater_SSR, HIGH); 
      digitalWrite(filament_heater_coil_ON_LED, HIGH);
      led1.on();  }
  else {
      digitalWrite(filament_heater_SSR, LOW);
      digitalWrite(filament_heater_coil_ON_LED, LOW);
      led1.off(); }

  // Printer enclosure temp control
  if (Enclosure_Heater && !Override  && (Temperature <= PE_TSP)) {
    digitalWrite(enclosure_heater_SSR, HIGH);
    digitalWrite(enclosure_heater_coil_ON_LED, HIGH);
    led2.on(); }
  else { 
    digitalWrite(enclosure_heater_SSR, LOW);
    digitalWrite(enclosure_heater_coil_ON_LED, LOW); 
    led2.off(); }  


  // Display Temperature and Humidity info
  if (Filament_Heater && !Enclosure_Heater && (displayTimerActive == false)) {FilamentChamberInfo();}
  
  if (Enclosure_Heater && !Filament_Heater && (displayTimer2Active == false)) {PrinterEnclosureInfo();}

  if (Filament_Heater && Enclosure_Heater && (displayTimerActive == false) && (displayTimer2Active == false)) {
    if (!Alternate) {FilamentChamberInfo();}
    if (Alternate) {PrinterEnclosureInfo();}
  }

  if (!Filament_Heater && !Enclosure_Heater && Override) {
    display.clearDisplay();
    display.display();} 

}
