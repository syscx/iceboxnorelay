#include <OneWire.h>
#include <Time.h>
#include <stdio.h>
#include <SPI.h>
#include <Wire.h>

#include <DS1302RTC.h>
#include <Time.h>
#include <avr/wdt.h>


// Init the DS1302
// Set pins:  CE, IO,CLK
DS1302RTC RTC(27, 29, 31);

// Optional connection for RTC module
#define DS1302_GND_PIN 33
#define DS1302_VCC_PIN 35

#define  LED_OFF  0
#define  LED_ON  1

/*-----( Declare objects )-----*/  

#define ONE_WIRE_BUS 6

OneWire ds(ONE_WIRE_BUS);

int previousmode = 0;//Exceed Temperature Mode 1 Lower Temperature Mode 0
int relay = 7;//Icebox Temperature Control

// initialize the library with the numbers of the interface pins
byte addr[8];

int temperatureall[24] = {
//0 , 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23  
  11,11,10,10,10,11,12,13,15,17,19,21,22,22,22,21,20,19,17,15,14,12,12,12};


void setup() 
{
  Serial.begin(9600);
  wdt_enable (WDTO_8S);

  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);
  // Activate RTC module
  digitalWrite(DS1302_GND_PIN, LOW);
  pinMode(DS1302_GND_PIN, OUTPUT);

  digitalWrite(DS1302_VCC_PIN, HIGH);
  pinMode(DS1302_VCC_PIN, OUTPUT);

  Serial.println("RTC activated");

  delay(500);

  // Check clock oscillation  
  if (RTC.haltRTC())
    Serial.println("Clock stopped!");
  else
    Serial.println("Clock working.");

  // Check write-protection
  if (RTC.writeEN())
    Serial.println("Write allowed.");
  else
    Serial.println("Write protected.");

  delay ( 2000 );

  // Setup Time library  
  Serial.print("RTC Sync");
  setSyncProvider(RTC.get); // the function to get the time from the RTC
  if(timeStatus() == timeSet)
    Serial.println(" Ok!");
  else
    Serial.println(" FAIL!");

}

int icebox_count = 0;
int notemp_count = 0;
int error_count = 0;

void loop() 
{
  // Warning!
  if(timeStatus() != timeSet) 
  {
    Serial.println(F("RTC ERROR: SYNC!"));
  }
  Serial.println(F("looping!"));
  if(icebox_count>100)
    icebox_count=100;
  if(notemp_count>100)
    notemp_count=100;
	
  int retrysearch = 0;
  int found = 0;
	
  while (retrysearch < 3 && found==0)
  {
    found = searchsensor();
    retrysearch++;
  }
	
  if(found==0)
  {
    notemp_count++;
  }
  else
  {
    notemp_count=0;
  }
  if(notemp_count==0)
  {
    //part 1 temperature capture
    byte present = 0;
    byte data[12];
    gettemperature(data);
    char str[150];
    snprintf(str,sizeof(str),"Try to adjust hour [%d] temperature to %d",hour(),temperatureall[hour()]);
    Serial.println(str);
    int ret = controlRelay(temperatureall[hour()],data);

  }
  else
  {
    Serial.println("no temperature detected");
    if(notemp_count>10)
    {
      Serial.println("Turn on icebox");
      digitalWrite(relay, LOW); 
    }
  }
  wdt_reset();
}

int searchsensor()
{
    if(!ds.search(addr))
    {
      char bufx[30];
      snprintf(bufx,sizeof(bufx),"add Error %d",error_count);
      if(error_count%20==0)
      {
        if(error_count>1000) error_count = 0;
      }
      error_count++;
      ds.reset_search();
      delay(350);
      return 0;
    }
    return 1;
}
void gettemperature(byte data[])
{
    byte i;
    if ( OneWire::crc8( addr, 7) != addr[7]) 
    {
      return;
    }
    ds.reset();
    ds.select(addr);
    ds.write(0x44,1);         // start conversion, with parasite power on at the end

    delay(750);     // maybe 750ms is enough, maybe not
    // we might do a ds.depower() here, but the reset will take care of it.

    ds.reset();
    ds.select(addr);    
    ds.write(0xBE);         // Read Scratchpad

    for ( i = 0; i < 9; i++) 
    {           // we need 9 bytes
      data[i] = ds.read();
    }
}
int controlRelay(int degreetrigger,  byte* data)
{
	int HighByte, LowByte, TReading, SignBit, Tc_100, Whole,Fract;
	LowByte = data[0];
	HighByte = data[1];
	TReading = (HighByte << 8) + LowByte;
	SignBit = TReading & 0x8000;  // test most sig bit
	if (SignBit) // negative
	{
		TReading = (TReading ^ 0xffff) + 1; // 2's comp
	}
	Tc_100 = (6 * TReading) + TReading / 4;    // multiply by (100 * 0.0625) or 6.25

	Whole = Tc_100 / 100;  // separate off the whole and fractional portions
        Fract = Tc_100 % 100;
        
        char str[80];
        snprintf(str,sizeof(str),"Temperature is [%d], exact one is [%d.%d] Time %d:%d:%d",Whole,Whole,Fract,hour(),minute(),second());
        Serial.println(str);

	if(Whole>degreetrigger)
	{
		if(previousmode==1)
			icebox_count++;
		else
			icebox_count=0;  		
		previousmode=1;
                char str[30];
                snprintf(str,sizeof(str),"Try to turn on box %d",icebox_count);
                Serial.println(str);
		if(icebox_count>80)
		{
			//trigger on
    		        Serial.println("Move relay to low");
			digitalWrite(relay, LOW);
    		        Serial.println("Turn Relay low");
			return 1;
		}
		else
                {
    		        Serial.println("[l]Wait counter reached 80");
      			return 0;
                }
	}
	else
	{
		if(previousmode==0)
		{
			icebox_count++;
		}
		else
			icebox_count=0;  		
		previousmode=0;
                char str[30];
                snprintf(str,sizeof(str),"Try to turn off box %d",icebox_count);
                Serial.println(str);
		if(icebox_count>80)
		{
			//trigger off
    		        Serial.println("Try to Relay high");
			digitalWrite(relay, HIGH); 
    		        Serial.println("Relay high");
			return 0;
		}
		else
                {
    		        Serial.println("[h]Wait counter reached 80");
      			return 1;
                }
	}	
}
