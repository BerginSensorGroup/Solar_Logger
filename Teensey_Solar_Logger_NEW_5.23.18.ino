//Import libaries
#include <Wire.h> //Also RTC library
#include <SD.h> //SD Card Library
#include <SPI.h>
#include <DS3231.h> //RTC library

// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;
const int chipSelect = 10;    
File logfile;

// how many milliseconds between grabbing data and logging it. 1000 ms is once a second
#define LOG_INTERVAL  1000 // mills between entries (reduce to take more/faster data)

#define SYNC_INTERVAL 10000 // mills between calls to flush() - to write data to the card
uint32_t syncTime = 0; // time of last sync()


// the digital pins that connect to the LEDs (Solder pads L1 (green) and L2 (red) on the datalogger shield)
#define redLEDpin 3
#define greenLEDpin 2

RTClib RTC;

//Analog input pins
#define solarpanel1 A6
#define solarpanel2 A7
#define thermocouple1 A3
#define thermocouple2 A2

float sp1;
float sp2;
float tc1;
float tc2;

float R1Panel1=56.0;
float R2Panel1=12.0;
float R1Panel2=56.0;
float R2Panel2=12.0;

float Calibration_Value = 1; // To Calibrate Solar Panel 2 with Solar Panel 1
int time_step = 300;

int years;  
int months; 
int days; 
int hours; 
int minutes;
int seconds;

int prev_hours = 0;
int day_count = 0;
float prev_ratio;

int iteration = 0;
unsigned long commencement;

void setup()
{
  
  Serial.begin(9600);
  Wire.begin();
  /*
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  */
  //Serial.print("Nothing");
  
  // initialize the SD card
  //Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  //Serial.println("Checking");

  pinMode(redLEDpin, OUTPUT);
  pinMode(greenLEDpin, OUTPUT);

  digitalWrite(redLEDpin, HIGH);
  
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    //Keep the red LED on if SD Card failed or is not present
    error("Card failed, or not present");
    return;
  }

  //Flash the LEDs at the start of the program
  for(int i=1;i<10;i++){
     digitalWrite(redLEDpin, HIGH);
     delay(500);
     digitalWrite(redLEDpin, LOW);
     delay(500);
  }
  
  // create a new file
  char filename[] = "00000000.CSV";
  if(!SD.exists(filename)){
    //Save the date and time into the following variables
    DateTime now = RTC.now();
    years = now.year();
    months = now.month();
    days = now.day();
    hours = now.hour();
    minutes = now.minute();
    seconds = now.second(); // get seconds

    

    //Write the date and time into a string for the file name
    filename[0] = months/10 + '0';
    filename[1] = months%10 + '0';
    filename[2] = days/10 + '0';
    filename[3] = days%10 + '0';
    filename[4] = hours/10 + '0';
    filename[5] = hours%10 + '0';
    filename[6] = minutes/10 + '0';
    filename[7] = minutes%10 + '0';

    //Serial.println(filename);
    logfile = SD.open(filename,FILE_WRITE);
  }  
  
  if (! logfile) {
    //Serial.println("couldnt create file");
    //Serial.println("printing all files and directories...");
    File root = SD.open("/");
   
    printDirectory(root, 0);
    
  }

  //Heading for all the columns in the Excel File
  logfile.println("millis, datetime, voltage1, voltage2, current1, current2, power1, power2, temp1, temp2, energy1, energy2, sum_energy1, sum_energy2, ratio, rate");    
}
 
void loop()
{
  unsigned long runs = 0;
  
  float voltage1 = 0.000;
  float voltage2 = 0.000;
  float current1 = 0.000;
  float current2 = 0.000;
  float power1 = 0.000;
  float power2 = 0.000;
  float temp1=0.000;
  float temp2=0.000;
  float energy1 = 0.000;
  float energy2 = 0.000;
  float sum_energy1 = 0.000;
  float sum_energy2 = 0.000;
  float ratio = 0.000;
  float rate = 0.000;

  DateTime now = RTC.now();
  //If its the first iteration, set the initial time to the current time
  if(iteration == 0){
      commencement = now.unixtime();
      digitalWrite(greenLEDpin, HIGH);
  }

  //Until time as indicated by time_step has passed, keep reading the voltage measurements
  while(now.unixtime()-commencement < time_step){                  // change this number to determine how often if averages and logs data. in millisecs
    ReadVoltage(voltage1, voltage2, current1, current2, power1, power2, temp1, temp2, energy1, energy2);
    runs++;
    now = RTC.now();
  }
  commencement = now.unixtime();

 //If it is not the first iteration, perform the following calculations, need to wait 1 iteration because of the time step being used in the energy calculation
 if(iteration != 0){ 
  voltage1 = voltage1/runs;
  voltage2 = Calibration_Value * (voltage2/runs);
  current1 = voltage1/(R1Panel1+R2Panel1);
  current2 = voltage2/(R1Panel2+R2Panel2);
  power1 = sq(voltage1)/(R1Panel1+R2Panel1);
  power2 = sq(voltage2)/(R1Panel2+R2Panel2);
  temp1 = temp1/runs;
  temp2 = temp2/runs;
  energy1 = power1*time_step;
  energy2 = power2*time_step;

  //Daily Energy Total for each panel
  sum_energy1+=energy1;
  sum_energy2+=energy2;
   
  // delay for the amount of time we want between readings
  delay((LOG_INTERVAL -1) - (millis() % LOG_INTERVAL));
  
  // log milliseconds since starting
  uint32_t m = millis();
  logfile.print(m);           // milliseconds since start
  logfile.print(", ");    

  DateTime now = RTC.now();
  years = now.year();
  months = now.month();
  days = now.day();
  hours = now.hour();
  minutes = now.minute();
  seconds = now.second(); // get seconds
    
  // logfile.print();
  logfile.print(months, DEC);  
  logfile.print("/");
  logfile.print(days, DEC);
  logfile.print("/");
  logfile.print(years, DEC);
  logfile.print(" ");
  logfile.print(hours, DEC);
  logfile.print(":");
  logfile.print(minutes, DEC);
  logfile.print(":");
  logfile.print(seconds, DEC);
  // logfile.print('"');      
    
  logfile.print(", ");    
  logfile.print(voltage1);
  logfile.print(", ");
  logfile.print(voltage2);
  logfile.print(", ");
  logfile.print(current1);
  logfile.print(", ");
  logfile.print(current2);
  logfile.print(", ");
  logfile.print(power1);
  logfile.print(", ");
  logfile.print(power2);
  logfile.print(", ");
  logfile.print(temp1);
  logfile.print(", ");
  logfile.print(temp2);  
  logfile.print(", ");
  logfile.print(energy1);
  logfile.print(", ");

  //Method to only calculate the soiling ratio when the hour changes from 22 to 23
  if(hours == 23 && prev_hours == 22){
    logfile.print(energy2);  
    logfile.print(", ");
    logfile.print(sum_energy1);
    logfile.print(", ");
    logfile.print(sum_energy2);  
    logfile.print(", ");
  
    ratio = sum_energy1/sum_energy2;
    day_count += 1;
    sum_energy1 = 0.000;
    sum_energy2 = 0.000;

    //Method to only calculate the rate if the hour changes from 22 to 23 and 2 days have passed
    if(hours == 23 && prev_hours == 22 && day_count == 2){
      rate = ratio - prev_ratio;
      logfile.print(ratio);
      logfile.print(", ");
      logfile.println(rate); 
      day_count = 0; //reset day count to 0
    }
    else{
      logfile.println(ratio);
    }
    prev_ratio = ratio;
  }
  else{
      logfile.println(energy2);  
  }
  
  prev_hours = hours;
  
 }
 iteration ++;
 
 //At the end of data logging the green LED is turned off
 //digitalWrite(greenLEDpin, LOW);
 
  // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
  // which uses a bunch of power. and takes time
  if ((millis() - syncTime) >= SYNC_INTERVAL){
    syncTime = millis();
    
    // blink LED to show we are syncing data to the card & updating FAT!
    digitalWrite(redLEDpin, HIGH);
    logfile.flush();
    digitalWrite(redLEDpin, LOW);
  }
  if(false) {
    } 
}

void ReadVoltage(float &voltage1, float &voltage2, float &current1, float &current2, float &power1, float &power2, float &temp1, float &temp2, float &energy1, float &energy2){
  float sample1 = 0.0;
  float sample2 = 0.0;
  float tempVariable1 = 0.0;
  float tempVariable2 = 0.0;
  
 // taking 150 samples from voltage divider with a interval of 2 millisec and then average the samples data collected 
 for(int i=0;i<10;i++)
  {
    sample1=sample1+analogRead(solarpanel1); //read the voltage from the divider circuit - 1
    tempVariable1=tempVariable1+analogRead(thermocouple1);  // read the temperature reading for Panel 1
    delay(100);
    sample2=sample2+analogRead(solarpanel2);  //read the voltage from the divider circuit - 2
    tempVariable2=tempVariable2+analogRead(thermocouple2);  // read the temperature reading for Panel 2
    delay(100);
  }
  voltage1+=((R1Panel1+R2Panel1)/R2Panel1)*(sample1/10 * 3.3)/1023.0; //splitter 1 has 18.3 and 47.8 ohms. 
  voltage2+=((R1Panel2+R2Panel2)/R2Panel2)*(sample2/10 * 3.3)/1023.0; //splitter 2 has 18.2 and 47.9 ohms. 
  temp1+=((((tempVariable1/10) * 3.3)/1023.0)-1.25)/0.005;
  temp2+=((((tempVariable2/10) * 3.3)/1023.0)-1.25)/0.005;
}

void error(char *str)
{  
 digitalWrite(redLEDpin, HIGH);
 //Serial.println("Card failed, or not present");
 while(1);
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
    }
    if (entry.isDirectory()) {
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
    }
    entry.close();
  }
}

