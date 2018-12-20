// Based on the Arduino Clock project, date and time backed up by XC4536 RTC module
// Has timer schedule in eeprom (dayofweek/hour/minute/command) 4 byte sequence (command=0>do nothing)
// command is sent at scheduled time to wireless power point to initiate
// enter timer set by holding select while resetting, has manual control all commands
// during normal mode LEFT/UP/DOWN/RIGHT toggle 1,2,3,4
// set to 10 timers, can go up to 99 if eeprom space allows

// pushbutton code from https://arduino-info.wikispaces.com/LCD-Pushbuttons

#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal.h>
#include <EEPROM.h>

//pins
#define OUTPIN 3
#define KEYPIN A0
#define SHORTPULSE 316
#define LONGPULSE 818

//button constants
#define btnRIGHT  6
#define btnUP     5
#define btnDOWN   4
#define btnLEFT   3
#define btnSELECT 2
#define btnNONE   (-1)

#define TIMERCOUNT 10

RTC_DS1307 rtc;
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); //LiquidCrystal lcd(RS, E, b4, b5, b6, b7);

char monthsOfTheYear[12][4]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
char dOfTheWeek[10][4]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat","W/E","W/D","ALL"};
char setphase=0;                    //to know whether we are setting the clock or not
char setpos=0;                      //setting y/m/d/h/m/s
char cursorx[5]={2,5,7,12,15};      //where to place cursor in set mode

DateTime now;                       //set as global so we can update between loops

byte timers[TIMERCOUNT*4];
byte ostate[4]={'?','?','?','?'};   //output state at start is unknown (?)
unsigned long address=0x12340;      //address of remote, can be any 20 bit value with lowest 4 bits set to zero, ie HEX nnnn0
const byte rfcmds[]={0,0xF,0xE,0xD,0xC,0xB,0xA,0x7,0x6,0x4,0x8};      //nocmd, 1 on/off,2 on/off,3 on/off,4 on/off,all on/off
char cmdname[11][8]={"none   ","1 On   ","1 Off  ","2 On   ","2 Off  ","3 On   ","3 Off  ","4 On   ","4 Off  ","All On ","All Off"};

void setup () {
  byte charoff[8] = {B00000,B01110,B10001,B10001,B10001,B10001,B01110,B00000};  //custom characters for on/off icons, doesn't need to be global
  byte charon[8] = {B00000,B01110,B11111,B11111,B11111,B11111,B01110,B00000};  
  lcd.begin(16, 2);
  lcd.createChar(0, charoff);       //set up custom characters
  lcd.createChar(1, charon);
  lcd.clear();
  if (!rtc.begin()) {
    lcd.print("RTC fail.");  
    while (1);
  }
  for(int i=0;i<(TIMERCOUNT*4);i++){         //load timer schedule
    timers[i]=EEPROM.read(i);    
    if(timers[i]==255){timers[i]=0;}         //eeprom defaults to 0xFF
  }  
  if(read_LCD_buttons()==btnSELECT){         //go to timer setting mode-  save eeprom cycles, timers will not respond while setting!
    lcd.print("Timer Set Mode");  
    while(read_LCD_buttons()==btnSELECT){}   //wait til select released
    dotimerset();
    }
}

void loop () {
    if(!setphase){now = rtc.now();}                   //only update clock if we're not in setting mode
    dobuttons();    //respond to button presses
    lcd.setCursor(0, 0);
    if(setphase==0){
      lcd.print(dOfTheWeek[now.dayOfTheWeek()]);      //in normal mode, show day of week
    }else{
      lcd.write('`');                                 //in set mode, show year
      lcd.print(tens(now.year()), DEC);
      lcd.print(units(now.year()), DEC);
    }
    lcd.print(" ");
    lcd.print(tens(now.day()), DEC);                  //show day of month
    lcd.print(units(now.day()), DEC);
    lcd.print(' ');
    lcd.print(monthsOfTheYear[now.month()-1]);        //show month name
    lcd.print(' ');
    lcd.print(tens(now.hour()), DEC);                 //show hour
    lcd.print(units(now.hour()), DEC);
    lcd.print(':');
    lcd.print(tens(now.minute()), DEC);               //show minute
    lcd.print(units(now.minute()), DEC);
    dostates();                                       //display last output state
    checktimers();                                    //see if timers need to trigger
    lcd.setCursor(cursorx[setpos],0);                 //place flashing cursor in set mode
    delay(150);                                       //short enough to pick up brief keypresses
}

void dostates(){for(int i=0;i<4;i++){lcd.setCursor(i*4, 1);lcd.print(i+1);lcd.write(ostate[i]);}}

void dotimerset(){
  delay(100);                       //debounce
  lcd.clear();
  int n=1;
  lcd.setCursor(0, 0);              //manual control mode
  lcd.print("Manual:R to send");    //up/down to choose function, R to activate function
  lcd.setCursor(0, 1);
  lcd.print("Sel. U/D:");    
  while(read_LCD_buttons()!=btnSELECT){    //wait till select to continue
    lcd.setCursor(9, 1);
    lcd.print(cmdname[n]);
    if(read_LCD_buttons()==btnRIGHT){sendrf(packet(address,rfcmds[n]));}
    if(read_LCD_buttons()==btnDOWN){n=n-1;if(n<1){n=10;}}
    if(read_LCD_buttons()==btnUP){n=n+1;if(n>10){n=1;}}
    delay(150);
  }
  lcd.clear();
  delay(300);                           //debounce
  lcd.setCursor(0, 0);                  //timer setting mode
  lcd.print("Set TMR    of   ");
  int i=0;                              //index into timers
  int lmt,t;                            //upper limit of current value
  lcd.blink();                          //flashing cursor
  while(read_LCD_buttons()!=btnSELECT){ //wait till select to continue and save- use reset to abandon changes
    lmt=9;                              //days of week
    if((i&3)==1){lmt=23;}               //hours
    if((i&3)==2){lmt=59;}               //minutes
    if((i&3)==3){lmt=10;}               //modes
    lcd.setCursor(8, 0);
    lcd.print(tens(i/4+1),DEC);
    lcd.setCursor(9, 0);
    lcd.print(units(i/4+1),DEC);
    lcd.setCursor(14, 0);
    lcd.print(tens(TIMERCOUNT),DEC);
    lcd.setCursor(15, 0);
    lcd.print(units(TIMERCOUNT),DEC);
    lcd.setCursor(0, 1);
    lcd.print(dOfTheWeek[timers[i&0x7FFC]]);
    lcd.print(tens(timers[(i&0x7FFC)|1]),DEC);
    lcd.print(units(timers[(i&0x7FFC)|1]),DEC);
    lcd.print(":");
    lcd.print(tens(timers[(i&0x7FFC)|2]),DEC);
    lcd.print(units(timers[(i&0x7FFC)|2]),DEC);
    lcd.print(" ");
    lcd.print(cmdname[timers[(i&0x7FFC)|3]]);
    lcd.setCursor((i&3)*3,1);  //place flashing cursor over edit item
    if(read_LCD_buttons()==btnRIGHT){i=i+1;if(i>TIMERCOUNT*4-1){i=0;}}
    if(read_LCD_buttons()==btnLEFT){i=i-1;if(i<0){i=TIMERCOUNT*4-1;}}
    t=timers[i];    //use temporary int in case of loop around
    if(read_LCD_buttons()==btnUP){t=t+1;if(t>lmt){t=0;}}
    if(read_LCD_buttons()==btnDOWN){t=t-1;if(t<0){t=lmt;}}
    timers[i]=t;    
    delay(150);  
  }
  for(i=0;i<TIMERCOUNT*4;i++){EEPROM.write(i,timers[i]);}     //commit to eeprom
  lcd.clear();
  lcd.noBlink();    //flashing cursor off
  delay(300);   //debounce
}

void checktimers(){
  int flag;
  for(int i=0;i<TIMERCOUNT;i++){
    flag=0;                                                                               //assume no match
    if((timers[i*4]==7)&&((now.dayOfTheWeek()==0)||(now.dayOfTheWeek()==6))){flag=1;}    //weekend
    if((timers[i*4]==8)&&(now.dayOfTheWeek()>0)&&(now.dayOfTheWeek()<6)){flag=1;}        //weekday    
    if((timers[i*4]==9)){flag=1;}                                                        //all days
    if((now.dayOfTheWeek()==timers[i*4])){flag=1;}                                        //day of the week match
    if(now.second()>TIMERCOUNT){flag=0;}                                                  //limit time spent transmitting- should be enough unless heaps are the same time
    if((flag==1)&&(now.hour()==timers[i*4+1])&&(now.minute()==timers[i*4+2])){            //hour/minute match
      if(timers[i*4+3]){                                                                  //command is set
        lcd.setCursor(15, 1);lcd.write('T');                                              //show that transmission is occurring, and keys might not respond
        switch(timers[i*4+3]){
          case 1: sendrf(packet(address,0xF));ostate[0]=1;break;      //1 on
          case 2: sendrf(packet(address,0xE));ostate[0]=0;break;      //1 off
          case 3: sendrf(packet(address,0xD));ostate[1]=1;break;      //2 on
          case 4: sendrf(packet(address,0xC));ostate[1]=0;break;      //2 off
          case 5: sendrf(packet(address,0xB));ostate[2]=1;break;      //3 on
          case 6: sendrf(packet(address,0xA));ostate[2]=0;break;      //3 off
          case 7: sendrf(packet(address,0x7));ostate[3]=1;break;      //4 on
          case 8: sendrf(packet(address,0x6));ostate[3]=0;break;      //4 off
          case 9: sendrf(packet(address,0x4));ostate[0]=1;ostate[1]=1;ostate[2]=1;ostate[3]=1;break;      //all on
          case 10: sendrf(packet(address,0x8));ostate[0]=0;ostate[1]=0;ostate[2]=0;ostate[3]=0;break;      //all off
        }
        delay(1000);
      }
    }
  }
  lcd.setCursor(15, 1);lcd.write(' ');      //show that transmission ended
}

char tens(int n){return (n/10)%10;}    //to help show leading zeroes/digit positions

char units(int n){return n%10;}        //to help show leading zeros/digit positions

int read_LCD_buttons(){
  int adc_key_in    = 0;
  adc_key_in = analogRead(KEYPIN);      // read the value from the sensor 
  delay(5); //switch debounce delay. Increase this delay if incorrect switch selections are returned.
  int k = (analogRead(KEYPIN) - adc_key_in); //gives the button a slight range to allow for a little contact resistance noise
  if (5 < abs(k)) return btnNONE;  // double checks the keypress. If the two readings are not equal +/-k value after debounce delay, it tries again.
  // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
  // we add approx 50 to those values and check to see if we are close
  if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
  if (adc_key_in < 50)   return btnRIGHT;  
  if (adc_key_in < 195)  return btnUP; 
  if (adc_key_in < 380)  return btnDOWN; 
  if (adc_key_in < 555)  return btnLEFT; 
  if (adc_key_in < 790)  return btnSELECT;   
  return btnNONE;  // when all others fail, return this...
}

void dobuttons(){
  int key;
  key = read_LCD_buttons();
  if(key==btnSELECT){
    if(setphase==0){setphase=1;lcd.blink();setpos=3;}             //phase 1 is starting set mode, but select key still pressed, turn cursor on, default to changing hour
    if(setphase==2){setphase=3;lcd.noBlink();rtc.adjust(now);}    //phase 3 is ending set mode, but select key still pressed, turn cursor off, need to update RTC to edited time
  }
  if(key==btnNONE){
    if(setphase==1){setphase=2;}    //phase 2 is in set mode select key released
    if(setphase==3){setphase=0;}    //phase 0 is normal mode, select key released
  }
  if(setphase==0){          //respond to keys in normal mode (toggle outputs)
    if(read_LCD_buttons()==btnLEFT){ostate[0]=1-(ostate[0]&1);dostates();sendrf(packet(address,ostate[0]+0xE));delay(500);}
    if(read_LCD_buttons()==btnUP){ostate[1]=1-(ostate[1]&1);dostates();sendrf(packet(address,ostate[1]+0xC));delay(500);}
    if(read_LCD_buttons()==btnDOWN){ostate[2]=1-(ostate[2]&1);dostates();sendrf(packet(address,ostate[2]+0xA));delay(500);}
    if(read_LCD_buttons()==btnRIGHT){ostate[3]=1-(ostate[3]&1);dostates();sendrf(packet(address,ostate[3]+0x6));delay(500);}
  }
  if(setphase==2){          //respond to keys in setting mode
    if(key==btnLEFT){setpos=setpos-1;if(setpos<0){setpos=4;}}
    if(key==btnRIGHT){setpos=setpos+1;if(setpos>4){setpos=0;}}
    int syear,smon,sday,shour,smin,ssec;
    syear=now.year();
    smon=now.month();
    sday=now.day();
    shour=now.hour();
    smin=now.minute();
    ssec=now.second();
      if(key==btnUP){
        if(setpos==1){sday=sday+1;if(sday>31){sday=1;}}
        if(setpos==2){smon=smon+1;if(smon>12){smon=1;}}
        if(setpos==0){syear=syear+1;}
        if(setpos==3){shour=shour+1;if(shour>23){shour=0;}}
        if(setpos==4){smin=smin+1;if(smin>59){smin=0;}}
        rtc.adjust(DateTime(syear, smon, sday, shour, smin, ssec));    //update clock
        now = rtc.now();                                              //reload into now
    }
      if(key==btnDOWN){
        if(setpos==1){sday=sday-1;if(sday<1){sday=31;}}
        if(setpos==2){smon=smon-1;if(smon<1){smon=12;}}
        if(setpos==0){syear=syear-1;}
        if(setpos==3){shour=shour-1;if(shour<0){shour=23;}}
        if(setpos==4){smin=smin-1;if(smin<0){smin=59;}}
        rtc.adjust(DateTime(syear, smon, sday, shour, smin, ssec));    //update clock
        now = rtc.now();                                              //reload into now
    }
  }
}

unsigned long packet(unsigned long a,byte c){     //takes address, command, calculates crc and sends it
  unsigned long p;
  byte cx;
  p=((a&0xFFFFF)<<4)|(c&0xF);
  cx=rfcrc(p);
  p=(p<<8)|cx;
  return p;
}

byte rfcrc(unsigned long d){                      //calculate crc
  byte a,b,c;
  a=reverse(d>>16);
  b=reverse(d>>8);
  c=reverse(d);
  return reverse(a+b+c);
}

byte reverse(byte d){                             //reverse bit order in byte
  return ((d&0x80)>>7)|((d&0x40)>>5)|((d&0x20)>>3)|((d&0x10)>>1)|((d&0x08)<<1)|((d&0x04)<<3)|((d&0x02)<<5)|((d&0x01)<<7);
}

void sendrf(unsigned long int k){          //send a raw packet
  unsigned long int i;
  for(int r=0;r<20;r++){                   //do repeats-- do more if success rate is low
    for(i=0x80000000UL;i>0;i=i>>1){        //32 bits of sequence
      if(i&k){                             //hi bit
        digitalWrite(OUTPIN,HIGH);
        delayMicroseconds(LONGPULSE);
        digitalWrite(OUTPIN,LOW);
        delayMicroseconds(SHORTPULSE);
      }else{                               //lo bit
        digitalWrite(OUTPIN,HIGH);
        delayMicroseconds(SHORTPULSE);
        digitalWrite(OUTPIN,LOW);
        delayMicroseconds(LONGPULSE);      
      }
    }
    digitalWrite(OUTPIN,HIGH);        //3 more lo bits
    delayMicroseconds(SHORTPULSE);
    digitalWrite(OUTPIN,LOW);
    delayMicroseconds(LONGPULSE);      
    digitalWrite(OUTPIN,HIGH);
    delayMicroseconds(SHORTPULSE);
    digitalWrite(OUTPIN,LOW);
    delayMicroseconds(LONGPULSE);      
    digitalWrite(OUTPIN,HIGH);
    delayMicroseconds(SHORTPULSE);
    digitalWrite(OUTPIN,LOW);
    delayMicroseconds(LONGPULSE);      
    delayMicroseconds(8000);          //brief delay between repeats
  }
}

