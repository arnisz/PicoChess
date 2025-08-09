#include <stdint.h>

// --- move/history müssen VOR den von Arduino generierten Prototypen stehen! ---
struct Move {
  uint8_t from, to, flags;   // 0=quiet, 1=promoQ, 2=castle, 3=EP
  int     score;
};

struct Hist {
  int8_t cap, pcs;
  int    oldCastle, oldEp, oldHalf, oldKingW, oldKingB;
};

#include <Arduino.h>

/* ======== UCI-Engine für RP2040 / Arduino ========
   - 0x88 board
   - legal move gen (incl. castling, EP, promo->Q)
   - alpha-beta + quiescence
   - FEN/startpos + moves
   Arnold-ready :)
=================================================== */

#define MAX_PLY 64
#define INF 32000

// Pieces (white > 0, black < 0)
enum { EMPTY=0, WP=1, WN=2, WB=3, WR=4, WQ=5, WK=6,
              BP=-1, BN=-2, BB=-3, BR=-4, BQ=-5, BK=-6 };

// Castling bits
enum { WKC=1, WQC=2, BKC=4, BQC=8 };

// Board: 0..127  (0x88)
int8_t board[128];
int side = 1;                // 1 = white, -1 = black
int castle = 0;              // castling rights mask
int ep_sq = -1;              // en-passant target (0x88) or -1
int halfmove = 0, fullmove = 1;
int histPly = 0;

int kingSq[2] = { -1, -1 };  // [white=0, black=1]

// PST (coarse)
const int pstPawn[128] PROGMEM = {
  0,0,0,0,0,0,0,0,  5,5,5,5,5,5,5,5,  1,1,2,3,3,2,1,1,  0,0,0,2,2,0,0,0,
  0,0,0,2,2,0,0,0,  -1,-1,-1,0,0,-1,-1,-1,  -2,-2,-2,-2,-2,-2,-2,-2,  0,0,0,0,0,0,0,0
};
const int pstKnight[8]  = { -5,-2,0,1,1,0,-2,-5 }; // file weights (rank mirrored)
const int pstBishopDiag[8] = { -2,-1,0,1,1,0,-1,-2 };
const int pstRookFile[8]   = { 1,1,1,2,2,1,1,1 };
const int valP=100, valN=320, valB=330, valR=500, valQ=900, valK=0;

inline bool off(int sq){ return (sq & 0x88); }
inline int  fileOf(int sq){ return sq & 7; }
inline int  rankOf(int sq){ return sq >> 4; }
inline int  idx(int f,int r){ return (r<<4)|f; }

String inbuf;


Move movelist[256];
int  movecount;

int16_t pvTable[MAX_PLY][MAX_PLY];
int    pvLen[MAX_PLY];

int cmpMoves(const void*a,const void*b){ return ((Move*)b)->score - ((Move*)a)->score; }

// --------------------------------------------------
// Utilities
// --------------------------------------------------
void uciSend(const char*s){ Serial.println(s); }
void printMove(const Move&m, char out[8]){
  int f1=fileOf(m.from), r1=rankOf(m.from), f2=fileOf(m.to), r2=rankOf(m.to);
  out[0]='a'+f1; out[1]='1'+r1; out[2]='a'+f2; out[3]='1'+r2;
  if(m.flags==1) { out[4]='q'; out[5]=0; } else { out[4]=0; }
}
bool isWhite(int p){ return p>0; }
bool isBlack(int p){ return p<0; }

// --------------------------------------------------
// FEN
// --------------------------------------------------
bool loadFEN(const String& fen){
  for(int i=0;i<128;i++) if(!off(i)) board[i]=EMPTY;
  castle=0; ep_sq=-1; halfmove=0; fullmove=1;
  kingSq[0]=kingSq[1]=-1;
  histPly = 0; // reset move history when loading a new position

  int i=0, sq=idx(0,7);
  while(i<fen.length()){
    char c = fen[i++];
    if(c==' ') break;
    if(c=='/'){ sq = idx(0, rankOf(sq)-1); continue; }
    if(c>='1'&&c<='8'){ sq += (c-'0'); continue; }
    int p=0;
    switch(c){
      case 'P': p=WP; break; case 'N': p=WN; break; case 'B': p=WB; break;
      case 'R': p=WR; break; case 'Q': p=WQ; break; case 'K': p=WK; kingSq[0]=sq; break;
      case 'p': p=BP; break; case 'n': p=BN; break; case 'b': p=BB; break;
      case 'r': p=BR; break; case 'q': p=BQ; break; case 'k': p=BK; kingSq[1]=sq; break;
    }
    if(p){ board[sq++]=p; }
  }
  // side
  if(i>=fen.length()) return false;
  if(fen[i]=='w') { side=1; } else side=-1;
  while(i<fen.length() && fen[i]!=' ') i++; if(i<fen.length()) i++;
  // castling
  if(i<fen.length() && fen[i]!='-'){
    while(i<fen.length() && fen[i]!=' '){
      char c=fen[i++];
      if(c=='K') castle|=WKC; else if(c=='Q') castle|=WQC;
      else if(c=='k') castle|=BKC; else if(c=='q') castle|=BQC;
    }
  } else { i++; }
  if(i<fen.length()) i++;
  // ep
  if(i<fen.length() && fen[i]!='-'){
    int f=fen[i]-'a'; int r=fen[i+1]-'1'; ep_sq=idx(f,r); i+=2;
  } else { ep_sq=-1; i++; }
  // half/full
  while(i<fen.length() && fen[i]==' ') i++;
  int n=0; while(i<fen.length() && isDigit(fen[i])){ n=n*10 + (fen[i++]-'0'); } halfmove=n;
  while(i<fen.length() && fen[i]==' ') i++;
  n=0; while(i<fen.length() && isDigit(fen[i])){ n=n*10 + (fen[i++]-'0'); } if(n) fullmove=n;

  return true;
}

bool isDigit(char c){ return c>='0' && c<='9'; }

// --------------------------------------------------
// Attack detection
// --------------------------------------------------
const int knightD[8]={31,33,14,-14,18,-18,-31,-33};
const int kingD[8]={1,-1,16,-16,17,-17,15,-15};

bool sqAttacked(int sq, int bySide){
  // pawns
  if(bySide==1){ // white attacks up
    int a1=sq-15, a2=sq-17;
    if(!off(a1) && board[a1]==WP) return true;
    if(!off(a2) && board[a2]==WP) return true;
  }else{
    int a1=sq+15, a2=sq+17;
    if(!off(a1) && board[a1]==BP) return true;
    if(!off(a2) && board[a2]==BP) return true;
  }
  // knights
  for(int i=0;i<8;i++){ int t=sq+knightD[i]; if(!off(t)){
    int p=board[t]; if(p==0) continue;
    if(bySide==1 && p==WN) return true;
    if(bySide==-1 && p==BN) return true;
  }}
  // sliders
  const int dirB[4]={17,15,-15,-17};
  const int dirR[4]={1,-1,16,-16};
  // bishops/queen
  for(int d=0;d<4;d++){ int t=sq; while(true){ t+=dirB[d]; if(off(t)) break; int p=board[t];
      if(p){ if(bySide==1 && (p==WB||p==WQ)) return true;
             if(bySide==-1 && (p==BB||p==BQ)) return true; break; } }
  }
  // rooks/queen
  for(int d=0;d<4;d++){ int t=sq; while(true){ t+=dirR[d]; if(off(t)) break; int p=board[t];
      if(p){ if(bySide==1 && (p==WR||p==WQ)) return true;
             if(bySide==-1 && (p==BR||p==BQ)) return true; break; } }
  }
  // king
  for(int i=0;i<8;i++){ int t=sq+kingD[i]; if(!off(t)){
    int p=board[t];
    if(bySide==1 && p==WK) return true;
    if(bySide==-1 && p==BK) return true;
  }}
  return false;
}

// --------------------------------------------------
// Make / Unmake
// --------------------------------------------------
Hist hist[MAX_PLY];

bool makeMove(const Move&m){
  // validate source and target squares before making any changes
  int moving = board[m.from];
  int target = board[m.to];

  // source square must contain a piece of the side to move
  if(moving == EMPTY) return false;
  if((side == 1 && moving < 0) || (side == -1 && moving > 0)) return false;

  // destination must be empty or contain an enemy piece
  if(target != EMPTY && ((target > 0) == (moving > 0))) return false;

  Hist &h = hist[histPly++];
  h.cap = target;
  h.pcs = moving;
  h.oldCastle=castle; h.oldEp=ep_sq; h.oldHalf=halfmove; h.oldKingW=kingSq[0]; h.oldKingB=kingSq[1];

  ep_sq = -1;
  // EP capture
  if(m.flags==3){
    if(side==1){ // white moves, capturing black pawn behind
      int capSq = m.to+16;
      h.cap = board[capSq];
      board[capSq]=EMPTY;
    }else{
      int capSq = m.to-16;
      h.cap = board[capSq];
      board[capSq]=EMPTY;
    }
  }

  // move piece
  board[m.to]   = board[m.from];
  board[m.from] = EMPTY;

  // update king square
  if(board[m.to]==WK) kingSq[0]=m.to;
  if(board[m.to]==BK) kingSq[1]=m.to;

  // castling move
  if(m.flags==2){
    if(board[m.to]==WK){ // white
      if(m.to==idx(6,0)){ // O-O
        board[idx(5,0)]=WR; board[idx(7,0)]=EMPTY;
      }else if(m.to==idx(2,0)){ // O-O-O
        board[idx(3,0)]=WR; board[idx(0,0)]=EMPTY;
      }
    }else{ // black
      if(m.to==idx(6,7)){
        board[idx(5,7)]=BR; board[idx(7,7)]=EMPTY;
      }else if(m.to==idx(2,7)){
        board[idx(3,7)]=BR; board[idx(0,7)]=EMPTY;
      }
    }
  }

  // promotions (always to queen)
  if(m.flags==1){
    if(side==1 && rankOf(m.to)==7) board[m.to]=WQ;
    if(side==-1 && rankOf(m.to)==0) board[m.to]=BQ;
  }

  // set EP square if double pawn push
  if(h.pcs==WP && rankOf(m.from)==1 && rankOf(m.to)==3) ep_sq = m.from+16;
  if(h.pcs==BP && rankOf(m.from)==6 && rankOf(m.to)==4) ep_sq = m.from-16;

  // update castling rights on rook/king moves/captures
  if(h.pcs==WK){ castle&=~(WKC|WQC); }
  if(h.pcs==BK){ castle&=~(BKC|BQC); }
  if(m.from==idx(0,0)||m.to==idx(0,0)) castle&=~WQC;
  if(m.from==idx(7,0)||m.to==idx(7,0)) castle&=~WKC;
  if(m.from==idx(0,7)||m.to==idx(0,7)) castle&=~BQC;
  if(m.from==idx(7,7)||m.to==idx(7,7)) castle&=~BKC;

  int us = side;          // side that just moved
  side = -side;           // switch side to move to the opponent
  int kingSqUs = (us==1 ? kingSq[0] : kingSq[1]);

  // legality: own king must not be in check
  if(sqAttacked(kingSqUs, side)) {
    // unmake the move because it leaves our king in check
    side = us; // restore side
    board[m.from]=h.pcs; board[m.to]=h.cap;
    if(h.pcs==WK) kingSq[0]=h.oldKingW;
    if(h.pcs==BK) kingSq[1]=h.oldKingB;
    // undo special pieces
    if(m.flags==2){
      if(h.pcs==WK){
        if(m.to==idx(6,0)){ board[idx(7,0)]=WR; board[idx(5,0)]=EMPTY; }
        else if(m.to==idx(2,0)){ board[idx(0,0)]=WR; board[idx(3,0)]=EMPTY; }
      }else{
        if(m.to==idx(6,7)){ board[idx(7,7)]=BR; board[idx(5,7)]=EMPTY; }
        else if(m.to==idx(2,7)){ board[idx(0,7)]=BR; board[idx(3,7)]=EMPTY; }
      }
    }
    if(m.flags==3){
      if(us==1){
        board[m.to+16]=BP;
      }else{
        board[m.to-16]=WP;
      }
    }
    castle=h.oldCastle; ep_sq=h.oldEp; halfmove=h.oldHalf;
    histPly--; // undo history pointer on illegal move
    return false;
  }
  return true;
}

void unmakeMove(const Move&m){
  side = -side;
  // Hist &h = hist[halfmove % MAX_PLY];
  Hist &h = hist[--histPly];
  // undo special
  if(m.flags==2){
    if(h.pcs==WK){
      if(m.to==idx(6,0)){ board[idx(7,0)]=WR; board[idx(5,0)]=EMPTY; }
      else if(m.to==idx(2,0)){ board[idx(0,0)]=WR; board[idx(3,0)]=EMPTY; }
    }else if(h.pcs==BK){
      if(m.to==idx(6,7)){ board[idx(7,7)]=BR; board[idx(5,7)]=EMPTY; }
      else if(m.to==idx(2,7)){ board[idx(0,7)]=BR; board[idx(3,7)]=EMPTY; }
    }
  }
  if(m.flags==3){
    if(side==1){ board[m.to+16]=BP; }
    else       { board[m.to-16]=WP; }
  }
  board[m.from]=h.pcs;
  board[m.to]=h.cap;
  castle=h.oldCastle; ep_sq=h.oldEp; halfmove=h.oldHalf;
  kingSq[0]=h.oldKingW; kingSq[1]=h.oldKingB;
}

// --------------------------------------------------
// Move generation (pseudo, then legality by makeMove)
// --------------------------------------------------
void addMove(uint8_t from,uint8_t to,uint8_t flags,int score=0){
  Move &m = movelist[movecount++];
  m.from=from; m.to=to; m.flags=flags; m.score=score;
}

void genMoves(){
  movecount=0;
  const int dirB[4]={17,15,-15,-17};
  const int dirR[4]={1,-1,16,-16};
  for(int sq=0; sq<128; sq++){
    if (off(sq)) { sq += 7; continue; }
    int p = board[sq]; if(!p) continue;
    if(side==1 && p<0) continue;
    if(side==-1 && p>0) continue;

    int sgn = (p>0? 1:-1);
    int absp = sgn*p;

    if(absp==1){ // pawn
      int fwd = (sgn==1? sq+16 : sq-16);
      int startRank = (sgn==1? 1:6);
      int promoRank = (sgn==1? 6:1);
      if(!off(fwd) && board[fwd]==EMPTY){
        if(rankOf(sq)==promoRank) addMove(sq,fwd,1); // promo to Q
        else addMove(sq,fwd,0);
        int fwd2 = (sgn==1? sq+32 : sq-32);
        if(rankOf(sq)==startRank && board[fwd2]==EMPTY) addMove(sq,fwd2,0);
      }
      int cap1 = (sgn==1? sq+17 : sq-15);
      int cap2 = (sgn==1? sq+15 : sq-17);
      if(!off(cap1) && board[cap1]!=EMPTY && (board[cap1]*sgn)<0){
        if(rankOf(sq)==promoRank) addMove(sq,cap1,1);
        else addMove(sq,cap1,0,1000);
      }
      if(!off(cap2) && board[cap2]!=EMPTY && (board[cap2]*sgn)<0){
        if(rankOf(sq)==promoRank) addMove(sq,cap2,1);
        else addMove(sq,cap2,0,1000);
      }
      // EP
      if(ep_sq!=-1){
        if(cap1==ep_sq) addMove(sq,cap1,3,1200);
        if(cap2==ep_sq) addMove(sq,cap2,3,1200);
      }
    }
    else if(absp==2){ // knight
      const int d[8]={31,33,14,-14,18,-18,-31,-33};
      for(int i=0;i<8;i++){ int t=sq+d[i]; if(off(t)) continue; int q=board[t];
        if(q==EMPTY || (q*sgn)<0) addMove(sq,t,0,(q?1500:0));
      }
    }
    else if(absp==3||absp==4||absp==5){ // sliders
      const int* arr; int cnt;
      if(absp==3){ arr=dirB; cnt=4; }
      else if(absp==4){ arr=dirR; cnt=4; }
      else { static int qd[8]={1,-1,16,-16,17,15,-15,-17}; arr=qd; cnt=8; }
      for(int i=0;i<cnt;i++){
        int t=sq;
        while(true){
          t+=arr[i]; if(off(t)) break;
          int q=board[t];
          if(q==EMPTY){ addMove(sq,t,0); }
          else { if((q*sgn)<0) addMove(sq,t,0,1400); break; }
        }
      }
    }
    else if(absp==6){ // king + castling
      for(int i=0;i<8;i++){ int t=sq+kingD[i]; if(off(t)) continue; int q=board[t];
        if(q==EMPTY || (q*sgn)<0) addMove(sq,t,0,(q?2000:0));
      }
      // castling (check-free squares)
      if(sgn==1){
        if((castle&WKC) && board[idx(5,0)]==EMPTY && board[idx(6,0)]==EMPTY &&
           !sqAttacked(idx(4,0),-1) && !sqAttacked(idx(5,0),-1) && !sqAttacked(idx(6,0),-1))
          addMove(idx(4,0),idx(6,0),2);
        if((castle&WQC) && board[idx(3,0)]==EMPTY && board[idx(2,0)]==EMPTY && board[idx(1,0)]==EMPTY &&
           !sqAttacked(idx(4,0),-1) && !sqAttacked(idx(3,0),-1) && !sqAttacked(idx(2,0),-1))
          addMove(idx(4,0),idx(2,0),2);
      } else {
        if((castle&BKC) && board[idx(5,7)]==EMPTY && board[idx(6,7)]==EMPTY &&
           !sqAttacked(idx(4,7),1) && !sqAttacked(idx(5,7),1) && !sqAttacked(idx(6,7),1))
          addMove(idx(4,7),idx(6,7),2);
        if((castle&BQC) && board[idx(3,7)]==EMPTY && board[idx(2,7)]==EMPTY && board[idx(1,7)]==EMPTY &&
           !sqAttacked(idx(4,7),1) && !sqAttacked(idx(3,7),1) && !sqAttacked(idx(2,7),1))
          addMove(idx(4,7),idx(2,7),2);
      }
    }
  }
}

// --------------------------------------------------
// Evaluation
// --------------------------------------------------
int pstKnightVal(int sq, int sgn){
  int f=fileOf(sq), r=rankOf(sq); if(sgn==-1) r=7-r;
  return pstKnight[f] + pstKnight[r];
}
int pstBishopVal(int sq, int sgn){
  int f=fileOf(sq), r=rankOf(sq); if(sgn==-1) r=7-r;
  return pstBishopDiag[f] + pstBishopDiag[r];
}
int pstRookVal(int sq, int sgn){
  int f=fileOf(sq); return pstRookFile[f];
}

int evaluate(){
  int s=0;
  for(int sq=0; sq<128; sq++){
    if (off(sq)) { sq += 7; continue; }
    int p=board[sq]; if(!p) continue;
    int sgn=(p>0?1:-1), absp=sgn*p;
    int v=0;
    switch(absp){
      case 1: v=valP + pgm_read_word(&pstPawn[sq]); break;
      case 2: v=valN + pstKnightVal(sq,sgn); break;
      case 3: v=valB + pstBishopVal(sq,sgn); break;
      case 4: v=valR + pstRookVal(sq,sgn); break;
      case 5: v=valQ; break;
      case 6: v=valK; break;
    }
    s += sgn*v;
  }
  return (side==1? s : -s); // from side-to-move perspective
}

// --------------------------------------------------
// Search
// --------------------------------------------------
int nodes;

int quiesce(int alpha, int beta, int ply){
  int stand = evaluate();
  if(stand>=beta) return beta;
  if(stand>alpha) alpha=stand;

  genMoves();
  // only captures
  for(int i=0;i<movecount;i++){
    if(movelist[i].score<=0) continue; // capture heur
    Move m=movelist[i];
    if(!makeMove(m)) continue;
    nodes++;
    int score = -quiesce(-beta, -alpha, ply+1);
    unmakeMove(m);
    if(score>=beta) return beta;
    if(score>alpha) alpha=score;
  }
  return alpha;
}

int search(int depth, int alpha, int beta, int ply){
  if(depth==0) return quiesce(alpha, beta, ply);
  genMoves();
  if(movecount==0){
    // checkmate or stalemate
    int inCheck = sqAttacked((side==1? kingSq[0]:kingSq[1]), -side);
    return inCheck ? -INF+ply : 0;
  }
  // simple MVVLVA-style ordering already in score; also PV move later
  qsort(movelist, movecount, sizeof(Move), cmpMoves);
  int best = -INF;
  for(int i=0;i<movecount;i++){
    Move m=movelist[i];
    if(!makeMove(m)) continue;
    nodes++;
    int sc = -search(depth-1, -beta, -alpha, ply+1);
    unmakeMove(m);
    if(sc>best){ best=sc; pvTable[ply][ply]= (m.from<<8)|(m.to); pvLen[ply]=ply+1; }
    if(sc>alpha){ alpha=sc; }
    if(alpha>=beta) break;
  }
  return best;
}

Move think(int depth){
  nodes=0;
  Move best={0,0,0,0};
  int bestScore=-INF;
  for(int d=1; d<=depth; d++){
    genMoves();
    qsort(movelist, movecount, sizeof(Move), cmpMoves);
    int alpha=-INF, beta=INF;
    for(int i=0;i<movecount;i++){
      Move m=movelist[i];
      if(!makeMove(m)) continue;
      int sc = -search(d-1, -beta, -alpha, 1);
      unmakeMove(m);
      if(sc>bestScore){ bestScore=sc; best=m; }
      if(sc>alpha){ alpha=sc; }
    }
  }
  if(best.from==0 && best.to==0){
    // Fallback: pick the first legal move if search failed to find one
    genMoves();
    for(int i=0;i<movecount;i++){
      Move m=movelist[i];
      if(makeMove(m)){
        unmakeMove(m);
        best=m;
        break;
      }
    }
  }
  return best;
}

// --------------------------------------------------
// UCI plumbing
// --------------------------------------------------
void setStartPos(){
  loadFEN("rn1qkbnr/pppb1ppp/4p3/3p4/3P4/5N2/PPPNPPPP/R1BQKB1R w KQkq - 0 4"); // placeholder? we want normal start
  loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void parsePosition(const String& s){
  // "position startpos moves ..." or "position fen <..> moves ..."
  if(s.indexOf("startpos")>=0){
    loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  } else {
    int p = s.indexOf("fen ");
    if(p>=0){
      String fen = s.substring(p+4);
      int mpos = fen.indexOf(" moves ");
      if(mpos>=0) fen = fen.substring(0,mpos);
      fen.trim(); loadFEN(fen);
    }
  }
  int m = s.indexOf(" moves ");
  if(m>=0){
    String rest = s.substring(m+7); rest.trim();
    int i=0;
    while(i<rest.length()){
      if(rest[i]==' ') { i++; continue; }
      char f1=rest[i++]; char r1=rest[i++]; char f2=rest[i++]; char r2=rest[i++];
      int from = idx(f1-'a', r1-'1');
      int to   = idx(f2-'a', r2-'1');
      uint8_t flags=0;
      if(i<rest.length() && (rest[i]=='q'||rest[i]=='Q')){ flags=1; i++; }
      Move m; m.from=from; m.to=to; m.flags=flags; m.score=0;
      if(!makeMove(m)){ /* ignore ill-formed */ }
    }
  }
}

int fixedDepth = 3;

void goCommand(const String& s){
  int p = s.indexOf("depth ");
  int d = fixedDepth;
  if(p>=0){
    d = s.substring(p+6).toInt();
    if(d<=0) d=fixedDepth;
  }
  Move bm = think(d);
  char buf[8]; printMove(bm, buf);
  Serial.print("bestmove "); Serial.println(buf);
}

// --------------------------------------------------

void setup(){
  Serial.begin(115200);
  while(!Serial) {}
  setStartPos();
}

void loop(){
  while(Serial.available()){
    char c=Serial.read();
    if(c=='\r') continue;
    if(c=='\n'){
      String cmd=inbuf; inbuf="";
      cmd.trim();
      if(cmd=="uci"){
        uciSend("id name PicoChess");
        uciSend("id author Arnold");
        uciSend("uciok");
      } else if(cmd=="isready"){
        uciSend("readyok");
      } else if(cmd=="ucinewgame"){
        setStartPos();
      } else if(cmd.startsWith("position")){
        parsePosition(cmd);
      } else if(cmd.startsWith("go")){
        goCommand(cmd);
      } else if(cmd=="quit"){
        // do nothing
      }
    } else inbuf+=c;
  }
}
