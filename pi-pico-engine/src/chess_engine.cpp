#include "chess_engine.hpp"

History history[128];
int histPly=0;

bool makeMove(const Move &m){
  History &h = history[histPly];
  h.m = m; h.castle = castle; h.ep = enpassant; h.half = halfmove;

  popBit(bitboards[m.piece], m.from);
  if(m.flags & 4){
    int capSq = (side==WHITE ? m.to-8 : m.to+8);
    int capPiece = side==WHITE ? BP : WP;
    popBit(bitboards[capPiece], capSq);
  }
  if(m.capture!=NO_PIECE && !(m.flags & 4)) popBit(bitboards[m.capture], m.to);

  int piece = m.piece;
  int to = m.to;
  if(m.flags & 16){
    piece = (side==WHITE ? WQ : BQ);
  }
  setBit(bitboards[piece], to);

  if(m.flags & 8){
    if(m.to==6){ popBit(bitboards[WR],7); setBit(bitboards[WR],5); }
    else if(m.to==2){ popBit(bitboards[WR],0); setBit(bitboards[WR],3); }
    else if(m.to==62){ popBit(bitboards[BR],63); setBit(bitboards[BR],61); }
    else if(m.to==58){ popBit(bitboards[BR],56); setBit(bitboards[BR],59); }
  }

  if(m.piece==WK) castle &= ~(WKC|WQC);
  if(m.piece==BK) castle &= ~(BKC|BQC);
  if(m.from==0 || m.to==0) castle &= ~WQC;
  if(m.from==7 || m.to==7) castle &= ~WKC;
  if(m.from==56 || m.to==56) castle &= ~BQC;
  if(m.from==63 || m.to==63) castle &= ~BKC;

  enpassant = -1;
  if(m.flags & 2) enpassant = (side==WHITE ? m.from+8 : m.from-8);

  updateOccupancies();

  int kingSq = lsb(bitboards[ side==WHITE ? WK : BK ]);
  if(squareAttacked(kingSq, side^1)){
    unmakeMove();
    return false;
  }

  side ^= 1;
  histPly++;
  return true;
}

void unmakeMove(){
  histPly--;
  side ^= 1;
  History &h = history[histPly];
  const Move &m = h.m;
  castle = h.castle; enpassant = h.ep; halfmove = h.half;

  popBit(bitboards[m.flags & 16 ? (side==WHITE?WQ:BQ):m.piece], m.to);
  setBit(bitboards[m.piece], m.from);

  if(m.flags & 4){
    int capSq = (side==WHITE ? m.to-8 : m.to+8);
    int capPiece = side==WHITE ? BP : WP;
    setBit(bitboards[capPiece], capSq);
  }
  if(m.capture!=NO_PIECE && !(m.flags & 4))
    setBit(bitboards[m.capture], m.to);

  if(m.flags & 8){
    if(m.to==6){ popBit(bitboards[WR],5); setBit(bitboards[WR],7); }
    else if(m.to==2){ popBit(bitboards[WR],3); setBit(bitboards[WR],0); }
    else if(m.to==62){ popBit(bitboards[BR],61); setBit(bitboards[BR],63); }
    else if(m.to==58){ popBit(bitboards[BR],59); setBit(bitboards[BR],56); }
  }

  updateOccupancies();
}

int quiesce(int alpha,int beta){
  int stand = evaluate();
  if(stand >= beta) return beta;
  if(stand > alpha) alpha = stand;

  MoveList list; generateMoves(list);
  for(int i=0;i<list.count;i++){
    if(!(list.moves[i].flags & 1)) continue;
    if(!makeMove(list.moves[i])) continue;
    int score = -quiesce(-beta, -alpha);
    unmakeMove();
    if(score >= beta) return beta;
    if(score > alpha) alpha = score;
  }
  return alpha;
}

int search(int depth,int alpha,int beta){
  if(depth==0) return quiesce(alpha,beta);
  MoveList list; generateMoves(list);
  if(list.count==0){
    if(squareAttacked(lsb(bitboards[ side==WHITE?WK:BK ]), side^1)) return -32000 + depth;
    return 0;
  }
  for(int i=0;i<list.count;i++){
    Move m = list.moves[i];
    if(!makeMove(m)) continue;
    int score = -search(depth-1, -beta, -alpha);
    unmakeMove();
    if(score >= beta) return beta;
    if(score > alpha) alpha = score;
  }
  return alpha;
}

Move think(int depth){
  Move best={0};
  int bestScore=-32000;
  MoveList list; generateMoves(list);
  for(int d=1; d<=depth; d++){
    for(int i=0;i<list.count;i++){
      Move m = list.moves[i];
      if(!makeMove(m)) continue;
      int sc = -search(d-1, -32000, 32000);
      unmakeMove();
      if(sc>bestScore){ bestScore=sc; best=m; }
    }
  }
  return best;
}

void parsePosition(const String& s){
  if(s.indexOf("startpos")>=0) setStartPos();
  else {
    int p=s.indexOf("fen ");
    if(p>=0){ String fen=s.substring(p+4); int mpos=fen.indexOf(" moves "); if(mpos>=0) fen=fen.substring(0,mpos); const_cast<String&>(fen).trim(); loadFEN(fen); }
  }
  int m=s.indexOf(" moves ");
  if(m>=0){
    String rest=s.substring(m+7); const_cast<String&>(rest).trim(); int i=0;
    while(i<rest.length()){
      char f1=rest[i++]; char r1=rest[i++]; char f2=rest[i++]; char r2=rest[i++];
      int from=(r1-'1')*8+(f1-'a'); int to=(r2-'1')*8+(f2-'a');
      MoveList list; generateMoves(list);
      for(int j=0;j<list.count;j++){ Move mv=list.moves[j]; if(mv.from==from && mv.to==to){ makeMove(mv); break; } }
      if(i<rest.length() && rest[i]=='q') i++;
      while(i<rest.length() && rest[i]==' ') i++;
    }
  }
}

void goCommand(const String& s){
  int p=s.indexOf("depth "); int d=3; if(p>=0){ d=s.substring(p+6).toInt(); if(d<=0) d=3; }
  Move bm=think(d); char buf[6];
  int f1=bm.from%8, r1=bm.from/8, f2=bm.to%8, r2=bm.to/8;
  buf[0]='a'+f1; buf[1]='1'+r1; buf[2]='a'+f2; buf[3]='1'+r2;
  if(bm.flags & 16) { buf[4]='q'; buf[5]=0; } else buf[4]=0;
  Serial.print("bestmove "); Serial.println(buf);
}

void initEngine(){
  initLeapers();
  setStartPos();
}
