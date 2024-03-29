//Minimal 433MHz Controller for MS6148
#define OUTPIN 3
#define SHORTPULSE 316
#define LONGPULSE 818
const byte rfcmds[]={0xF,0xE,0xD,0xC,0xB,0xA,0x7,0x6,0x4,0x8};      //nocmd, 1 on/off,2 on/off,3 on/off,4 on/off,all on/off
const unsigned long address=0x12340;

void setup() {
  pinMode(OUTPIN,OUTPUT);
}

void loop() {
  sendrf(packet(address,rfcmds[0]));      //1 on
  delay(3000);
  sendrf(packet(address,rfcmds[1]));      //1 off
  delay(3000); 
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

