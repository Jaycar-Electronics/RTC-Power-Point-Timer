//convert incoming signal to hex code for emitting- usually 35 bits, 

  long t,u;
  int oldd=0;
  int newd;
  int d=0;
  int dl=0;

void setup() {
  Serial.begin(115200);
  Serial.println("Press a button on the remote");
  Serial.println("Adress is first five hex nybbles:");
  Serial.println("vvvvvvv");
}

void loop() {  
  t=micros();
  u=t;
  while(t-u<10000){
    newd=digitalRead(8);
    t=micros();
    if(newd!=oldd){
      dump();
      u=t;
    }    
    oldd=newd;
  }
}

void dump(){
  if(newd){
    if(t-u<500){
      dobit(1);
    }else if(t-u<900){
      dobit(0);
    }else{
      Serial.println(d<<(4-dl),HEX);d=0;dl=0;Serial.print("0x");
    }
  }
}

void dump2(){
  if(!newd){
    Serial.print(t-u);
    Serial.write(' ');  
  }
}

void dobit(byte n){
  d=d*2+n;
  dl++;
  if(dl>3){Serial.print(d,HEX);d=0;dl=0;}
}

