#include "src/chess_engine.hpp"

String inbuf;

void setup(){
#ifdef ARDUINO_ENV
  Serial.begin(115200);
  while(!Serial){}
#endif
  initEngine();
}

void loop(){
#ifdef ARDUINO_ENV
  while(Serial.available()){
    char c=Serial.read(); if(c=='\r') continue;
    if(c=='\n'){
      String cmd=inbuf; inbuf=""; cmd.trim();
      if(cmd=="uci"){ Serial.println("id name PicoChess Bitboard"); Serial.println("id author Arnold"); Serial.println("uciok"); }
      else if(cmd=="isready"){ Serial.println("readyok"); }
      else if(cmd=="ucinewgame"){ setStartPos(); }
      else if(cmd.startsWith("position")){ parsePosition(cmd); }
      else if(cmd.startsWith("go")){ goCommand(cmd); }
    } else inbuf+=c;
  }
#endif
}
