#include <Arduino.h>
#include <stdint.h>

// ================================================================
//    PicoChess Bitboard Engine
//    - Bitboard representation for fast move generation
//    - Legal move generation including castling, EP and promotions
//    - Alpha/Beta with quiescence search
//    - FEN and UCI support
// ================================================================

// ---- basic types ------------------------------------------------
using U64 = uint64_t;

enum { WHITE, BLACK, BOTH };
enum Piece {
  WP, WN, WB, WR, WQ, WK,
  BP, BN, BB, BR, BQ, BK, NO_PIECE
};

enum CastleRights { WKC=1, WQC=2, BKC=4, BQC=8 };

// ---- board state ------------------------------------------------
U64 bitboards[12];            // one bitboard per piece type
U64 occupancies[3];           // white, black, both
int side = WHITE;             // side to move
int castle = 0;               // castling rights mask
int enpassant = -1;           // en-passant target square or -1
int halfmove = 0, fullmove = 1;

// ---- utility bit helpers ---------------------------------------
inline void setBit(U64 &bb, int sq){ bb |= 1ULL << sq; }
inline void popBit(U64 &bb, int sq){ bb &= ~(1ULL << sq); }
inline bool getBit(U64 bb, int sq){ return bb & (1ULL << sq); }
inline int lsb(U64 bb){ return __builtin_ctzll(bb); }

int popLSB(U64 &bb){ int sq = lsb(bb); bb &= bb-1; return sq; }
int countBits(U64 bb){ return __builtin_popcountll(bb); }

// ---- attack tables ----------------------------------------------
U64 pawnAttacks[2][64];
U64 knightAttacks[64];
U64 kingAttacks[64];

// compute attacks for leaper pieces (pawn/knight/king)
void initLeapers(){
  for(int sq=0; sq<64; sq++){
    int r = sq / 8, f = sq % 8;

    // white pawn
    if(r < 7){
      if(f > 0) pawnAttacks[WHITE][sq] |= 1ULL << (sq + 7);
      if(f < 7) pawnAttacks[WHITE][sq] |= 1ULL << (sq + 9);
    }
    // black pawn
    if(r > 0){
      if(f > 0) pawnAttacks[BLACK][sq] |= 1ULL << (sq - 9);
      if(f < 7) pawnAttacks[BLACK][sq] |= 1ULL << (sq - 7);
    }

    // knights
    if(r + 2 <= 7 && f + 1 <= 7) knightAttacks[sq] |= 1ULL << (sq + 17);
    if(r + 1 <= 7 && f + 2 <= 7) knightAttacks[sq] |= 1ULL << (sq + 10);
    if(r + 2 <= 7 && f - 1 >= 0) knightAttacks[sq] |= 1ULL << (sq + 15);
    if(r + 1 <= 7 && f - 2 >= 0) knightAttacks[sq] |= 1ULL << (sq + 6);
    if(r - 2 >= 0 && f + 1 <= 7) knightAttacks[sq] |= 1ULL << (sq - 15);
    if(r - 1 >= 0 && f + 2 <= 7) knightAttacks[sq] |= 1ULL << (sq - 6);
    if(r - 2 >= 0 && f - 1 >= 0) knightAttacks[sq] |= 1ULL << (sq - 17);
    if(r - 1 >= 0 && f - 2 >= 0) knightAttacks[sq] |= 1ULL << (sq - 10);

    // king
    if(r + 1 <= 7) kingAttacks[sq] |= 1ULL << (sq + 8);
    if(r - 1 >= 0) kingAttacks[sq] |= 1ULL << (sq - 8);
    if(f + 1 <= 7) kingAttacks[sq] |= 1ULL << (sq + 1);
    if(f - 1 >= 0) kingAttacks[sq] |= 1ULL << (sq - 1);
    if(r + 1 <= 7 && f + 1 <= 7) kingAttacks[sq] |= 1ULL << (sq + 9);
    if(r + 1 <= 7 && f - 1 >= 0) kingAttacks[sq] |= 1ULL << (sq + 7);
    if(r - 1 >= 0 && f + 1 <= 7) kingAttacks[sq] |= 1ULL << (sq - 7);
    if(r - 1 >= 0 && f - 1 >= 0) kingAttacks[sq] |= 1ULL << (sq - 9);
  }
}

// sliding attacks computed on the fly
U64 maskRookAttacks(int sq, U64 block){
  U64 attacks=0ULL; int r=sq/8, f=sq%8;
  for(int tr=r+1; tr<=7; tr++){ int s=tr*8+f; attacks|=1ULL<<s; if(getBit(block,s)) break; }
  for(int tr=r-1; tr>=0; tr--){ int s=tr*8+f; attacks|=1ULL<<s; if(getBit(block,s)) break; }
  for(int tf=f+1; tf<=7; tf++){ int s=r*8+tf; attacks|=1ULL<<s; if(getBit(block,s)) break; }
  for(int tf=f-1; tf>=0; tf--){ int s=r*8+tf; attacks|=1ULL<<s; if(getBit(block,s)) break; }
  return attacks;
}

U64 maskBishopAttacks(int sq, U64 block){
  U64 attacks=0ULL; int r=sq/8, f=sq%8;
  for(int tr=r+1,tf=f+1; tr<=7 && tf<=7; tr++,tf++){ int s=tr*8+tf; attacks|=1ULL<<s; if(getBit(block,s)) break; }
  for(int tr=r+1,tf=f-1; tr<=7 && tf>=0; tr++,tf--){ int s=tr*8+tf; attacks|=1ULL<<s; if(getBit(block,s)) break; }
  for(int tr=r-1,tf=f+1; tr>=0 && tf<=7; tr--,tf++){ int s=tr*8+tf; attacks|=1ULL<<s; if(getBit(block,s)) break; }
  for(int tr=r-1,tf=f-1; tr>=0 && tf>=0; tr--,tf--){ int s=tr*8+tf; attacks|=1ULL<<s; if(getBit(block,s)) break; }
  return attacks;
}

void updateOccupancies(){
  occupancies[WHITE]=occupancies[BLACK]=0ULL;
  for(int p=WP; p<=WK; p++) occupancies[WHITE] |= bitboards[p];
  for(int p=BP; p<=BK; p++) occupancies[BLACK] |= bitboards[p];
  occupancies[BOTH]=occupancies[WHITE]|occupancies[BLACK];
}

// ---- piece helpers ----------------------------------------------
int pieceAt(int sq){
  for(int p=WP; p<=BK; p++) if(getBit(bitboards[p], sq)) return p;
  return NO_PIECE;
}

// ---- attack detection -------------------------------------------
bool squareAttacked(int sq, int bySide){
  if(bySide==WHITE){
    if(pawnAttacks[BLACK][sq] & bitboards[WP]) return true;
    if(knightAttacks[sq] & bitboards[WN]) return true;
    if(maskBishopAttacks(sq, occupancies[BOTH]) & (bitboards[WB]|bitboards[WQ])) return true;
    if(maskRookAttacks(sq, occupancies[BOTH]) & (bitboards[WR]|bitboards[WQ])) return true;
    if(kingAttacks[sq] & bitboards[WK]) return true;
  }else{
    if(pawnAttacks[WHITE][sq] & bitboards[BP]) return true;
    if(knightAttacks[sq] & bitboards[BN]) return true;
    if(maskBishopAttacks(sq, occupancies[BOTH]) & (bitboards[BB]|bitboards[BQ])) return true;
    if(maskRookAttacks(sq, occupancies[BOTH]) & (bitboards[BR]|bitboards[BQ])) return true;
    if(kingAttacks[sq] & bitboards[BK]) return true;
  }
  return false;
}

// ---- Move representation ---------------------------------------
struct Move {
  uint8_t from, to;
  uint8_t piece;       // moved piece
  uint8_t capture;     // captured piece or NO_PIECE
  uint8_t promo;       // promotion piece or NO_PIECE
  uint8_t flags;       // bits: 1=capture,2=double,4=EP,8=castle,16=promo
  int score;
};

struct History {
  Move m;
  int castle, ep, half;
};

History history[128];
int histPly=0;

// ---- move list --------------------------------------------------
struct MoveList { Move moves[256]; int count; };

void addMove(MoveList &list, Move m){ list.moves[list.count++] = m; }

// ---- Move generation -------------------------------------------
void generateMoves(MoveList &list){
  list.count=0;
  U64 bb, attacks;

  if(side==WHITE){
    // pawns
    bb = bitboards[WP];
    while(bb){
      int from = popLSB(bb);
      int r = from/8;
      int to = from + 8;
      if(!(occupancies[BOTH] & (1ULL<<to))){
        if(r==6){
          addMove(list,{(uint8_t)from,(uint8_t)to,WP,NO_PIECE,WQ,16,0});
        }else{
          addMove(list,{(uint8_t)from,(uint8_t)to,WP,NO_PIECE,NO_PIECE,0,0});
          if(r==1 && !(occupancies[BOTH] & (1ULL<<(to+8))))
            addMove(list,{(uint8_t)from,(uint8_t)(to+8),WP,NO_PIECE,NO_PIECE,2,0});
        }
      }
      attacks = pawnAttacks[WHITE][from] & occupancies[BLACK];
      while(attacks){
        int t = popLSB(attacks); int cap = pieceAt(t);
        if(r==6) addMove(list,{(uint8_t)from,(uint8_t)t,WP,(uint8_t)cap,WQ,17,0});
        else addMove(list,{(uint8_t)from,(uint8_t)t,WP,(uint8_t)cap,NO_PIECE,1,0});
      }
      if(enpassant!=-1 && (pawnAttacks[WHITE][from] & (1ULL<<enpassant)))
        addMove(list,{(uint8_t)from,(uint8_t)enpassant,WP,BP,NO_PIECE,5,0});
    }

    // knights
    bb = bitboards[WN];
    while(bb){
      int from = popLSB(bb);
      attacks = knightAttacks[from] & ~occupancies[WHITE];
      while(attacks){
        int t = popLSB(attacks); int cap = pieceAt(t);
        uint8_t flags = cap!=NO_PIECE?1:0;
        addMove(list,{(uint8_t)from,(uint8_t)t,WN,(uint8_t)cap,NO_PIECE,flags,0});
      }
    }

    // bishops
    bb = bitboards[WB];
    while(bb){
      int from = popLSB(bb);
      attacks = maskBishopAttacks(from, occupancies[BOTH]) & ~occupancies[WHITE];
      while(attacks){
        int t = popLSB(attacks); int cap = pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0;
        addMove(list,{(uint8_t)from,(uint8_t)t,WB,(uint8_t)cap,NO_PIECE,fl,0});
      }
    }

    // rooks
    bb = bitboards[WR];
    while(bb){
      int from = popLSB(bb);
      attacks = maskRookAttacks(from, occupancies[BOTH]) & ~occupancies[WHITE];
      while(attacks){
        int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0;
        addMove(list,{(uint8_t)from,(uint8_t)t,WR,(uint8_t)cap,NO_PIECE,fl,0});
      }
    }

    // queens
    bb = bitboards[WQ];
    while(bb){
      int from = popLSB(bb);
      attacks = (maskBishopAttacks(from, occupancies[BOTH])|
                 maskRookAttacks(from, occupancies[BOTH])) & ~occupancies[WHITE];
      while(attacks){
        int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0;
        addMove(list,{(uint8_t)from,(uint8_t)t,WQ,(uint8_t)cap,NO_PIECE,fl,0});
      }
    }

    // king
    int from = lsb(bitboards[WK]);
    attacks = kingAttacks[from] & ~occupancies[WHITE];
    while(attacks){
      int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0;
      addMove(list,{(uint8_t)from,(uint8_t)t,WK,(uint8_t)cap,NO_PIECE,fl,0});
    }

    // castling
    if(castle & WKC){
      if(!(occupancies[BOTH] & ((1ULL<<5)|(1ULL<<6))) &&
         !squareAttacked(4,BLACK) && !squareAttacked(5,BLACK) && !squareAttacked(6,BLACK))
        addMove(list,{4,6,WK,NO_PIECE,NO_PIECE,8,0});
    }
    if(castle & WQC){
      if(!(occupancies[BOTH] & ((1ULL<<1)|(1ULL<<2)|(1ULL<<3))) &&
         !squareAttacked(4,BLACK) && !squareAttacked(3,BLACK) && !squareAttacked(2,BLACK))
        addMove(list,{4,2,WK,NO_PIECE,NO_PIECE,8,0});
    }
  } else {
    // black side mirrors white code
    bb = bitboards[BP];
    while(bb){
      int from = popLSB(bb);
      int r=from/8; int to=from-8;
      if(!(occupancies[BOTH] & (1ULL<<to))){
        if(r==1){
          addMove(list,{(uint8_t)from,(uint8_t)to,BP,NO_PIECE,BQ,16,0});
        }else{
          addMove(list,{(uint8_t)from,(uint8_t)to,BP,NO_PIECE,NO_PIECE,0,0});
          if(r==6 && !(occupancies[BOTH] & (1ULL<<(to-8))))
            addMove(list,{(uint8_t)from,(uint8_t)(to-8),BP,NO_PIECE,NO_PIECE,2,0});
        }
      }
      attacks = pawnAttacks[BLACK][from] & occupancies[WHITE];
      while(attacks){
        int t = popLSB(attacks); int cap=pieceAt(t);
        if(r==1) addMove(list,{(uint8_t)from,(uint8_t)t,BP,(uint8_t)cap,BQ,17,0});
        else addMove(list,{(uint8_t)from,(uint8_t)t,BP,(uint8_t)cap,NO_PIECE,1,0});
      }
      if(enpassant!=-1 && (pawnAttacks[BLACK][from] & (1ULL<<enpassant)))
        addMove(list,{(uint8_t)from,(uint8_t)enpassant,BP,WP,NO_PIECE,5,0});
    }

    bb = bitboards[BN];
    while(bb){
      int from=popLSB(bb);
      attacks = knightAttacks[from] & ~occupancies[BLACK];
      while(attacks){ int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0; addMove(list,{(uint8_t)from,(uint8_t)t,BN,(uint8_t)cap,NO_PIECE,fl,0}); }
    }

    bb = bitboards[BB];
    while(bb){ int from=popLSB(bb); attacks=maskBishopAttacks(from,occupancies[BOTH])&~occupancies[BLACK]; while(attacks){ int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0; addMove(list,{(uint8_t)from,(uint8_t)t,BB,(uint8_t)cap,NO_PIECE,fl,0}); } }

    bb = bitboards[BR];
    while(bb){ int from=popLSB(bb); attacks=maskRookAttacks(from,occupancies[BOTH])&~occupancies[BLACK]; while(attacks){ int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0; addMove(list,{(uint8_t)from,(uint8_t)t,BR,(uint8_t)cap,NO_PIECE,fl,0}); } }

    bb = bitboards[BQ];
    while(bb){ int from=popLSB(bb); attacks=(maskBishopAttacks(from,occupancies[BOTH])|maskRookAttacks(from,occupancies[BOTH]))&~occupancies[BLACK]; while(attacks){ int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0; addMove(list,{(uint8_t)from,(uint8_t)t,BQ,(uint8_t)cap,NO_PIECE,fl,0}); } }

    int from=lsb(bitboards[BK]);
    attacks = kingAttacks[from] & ~occupancies[BLACK];
    while(attacks){ int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0; addMove(list,{(uint8_t)from,(uint8_t)t,BK,(uint8_t)cap,NO_PIECE,fl,0}); }

    if(castle & BKC){
      if(!(occupancies[BOTH] & ((1ULL<<61)|(1ULL<<62))) &&
         !squareAttacked(60,WHITE) && !squareAttacked(61,WHITE) && !squareAttacked(62,WHITE))
        addMove(list,{60,62,BK,NO_PIECE,NO_PIECE,8,0});
    }
    if(castle & BQC){
      if(!(occupancies[BOTH] & ((1ULL<<57)|(1ULL<<58)|(1ULL<<59))) &&
         !squareAttacked(60,WHITE) && !squareAttacked(59,WHITE) && !squareAttacked(58,WHITE))
        addMove(list,{60,58,BK,NO_PIECE,NO_PIECE,8,0});
    }
  }
}

// ---- Make/Unmake ------------------------------------------------
bool makeMove(const Move &m){
  History &h = history[histPly];
  h.m = m; h.castle = castle; h.ep = enpassant; h.half = halfmove;

  // move piece
  popBit(bitboards[m.piece], m.from);

  if(m.flags & 4){ // en-passant capture
    int capSq = (side==WHITE ? m.to-8 : m.to+8);
    int capPiece = side==WHITE ? BP : WP;
    popBit(bitboards[capPiece], capSq);
  }
  if(m.capture!=NO_PIECE && !(m.flags & 4)) popBit(bitboards[m.capture], m.to);

  int piece = m.piece;
  int to = m.to;
  if(m.flags & 16){ // promotion
    piece = (side==WHITE ? WQ : BQ);
  }
  setBit(bitboards[piece], to);

  // castling rook moves
  if(m.flags & 8){
    if(m.to==6){ popBit(bitboards[WR],7); setBit(bitboards[WR],5); }
    else if(m.to==2){ popBit(bitboards[WR],0); setBit(bitboards[WR],3); }
    else if(m.to==62){ popBit(bitboards[BR],63); setBit(bitboards[BR],61); }
    else if(m.to==58){ popBit(bitboards[BR],56); setBit(bitboards[BR],59); }
  }

  // update rights
  if(m.piece==WK) castle &= ~(WKC|WQC);
  if(m.piece==BK) castle &= ~(BKC|BQC);
  if(m.from==0 || m.to==0) castle &= ~WQC;
  if(m.from==7 || m.to==7) castle &= ~WKC;
  if(m.from==56 || m.to==56) castle &= ~BQC;
  if(m.from==63 || m.to==63) castle &= ~BKC;

  enpassant = -1;
  if(m.flags & 2) enpassant = (side==WHITE ? m.from+8 : m.from-8);

  updateOccupancies();

  // legality: own king not in check
  int kingSq = lsb(bitboards[ side==WHITE ? WK : BK ]);
  if(squareAttacked(kingSq, side^1)){
    // undo
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

  // undo piece move
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

// ---- Evaluation -------------------------------------------------
const int pieceValue[12] = {
  100,320,330,500,900,0,  // white pieces
  -100,-320,-330,-500,-900,0 // black pieces (neg for convenience)
};

int evaluate(){
  int s=0;
  for(int p=WP; p<=BK; p++) s += pieceValue[p] * countBits(bitboards[p]);
  return (side==WHITE ? s : -s);
}

// ---- Search -----------------------------------------------------
int nodes;
int quiesce(int alpha,int beta){
  int stand = evaluate();
  if(stand >= beta) return beta;
  if(stand > alpha) alpha = stand;

  MoveList list; generateMoves(list);
  for(int i=0;i<list.count;i++){
    if(!(list.moves[i].flags & 1)) continue; // only captures
    if(!makeMove(list.moves[i])) continue;
    nodes++;
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
    nodes++;
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

// ---- FEN parsing ------------------------------------------------
void clearBoard(){ for(int i=0;i<12;i++) bitboards[i]=0ULL; castle=0; enpassant=-1; }

bool loadFEN(const String &fen){
  clearBoard();
  int i=0, sq=56;
  while(i<fen.length() && fen[i]!=' '){
    char c=fen[i++];
    if(c=='/') sq-=16; else if(c>='1'&&c<='8') sq+=c-'0'; else {
      Piece p=NO_PIECE;
      switch(c){
        case 'P': p=WP; break; case 'N': p=WN; break; case 'B': p=WB; break; case 'R': p=WR; break; case 'Q': p=WQ; break; case 'K': p=WK; break;
        case 'p': p=BP; break; case 'n': p=BN; break; case 'b': p=BB; break; case 'r': p=BR; break; case 'q': p=BQ; break; case 'k': p=BK; break;
      }
      if(p!=NO_PIECE){ setBit(bitboards[p], sq); sq++; }
    }
  }
  // side
  if(i>=fen.length()) return false; i++;
  side = (fen[i]=='w') ? WHITE : BLACK;
  while(i<fen.length() && fen[i]!=' ') i++; i++;
  // castling
  if(fen[i]=='-') i++; else {
    while(i<fen.length() && fen[i]!=' '){ char c=fen[i++]; if(c=='K') castle|=WKC; if(c=='Q') castle|=WQC; if(c=='k') castle|=BKC; if(c=='q') castle|=BQC; }
  }
  while(i<fen.length() && fen[i]==' ') i++;
  if(fen[i]=='-'){ enpassant=-1; i++; } else {
    int f=fen[i]-'a', r=fen[i+1]-'1'; enpassant=r*8+f; i+=2;
  }
  updateOccupancies();
  return true;
}

// ---- UCI helpers ------------------------------------------------
String inbuf;

void printMove(const Move&m, char out[6]){
  int f1=m.from%8, r1=m.from/8, f2=m.to%8, r2=m.to/8;
  out[0]='a'+f1; out[1]='1'+r1; out[2]='a'+f2; out[3]='1'+r2;
  if(m.flags & 16) { out[4]='q'; out[5]=0; } else out[4]=0;
}

void setStartPos(){
  loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void parsePosition(const String& s){
  if(s.indexOf("startpos")>=0) setStartPos();
  else {
    int p=s.indexOf("fen ");
    if(p>=0){ String fen=s.substring(p+4); int mpos=fen.indexOf(" moves "); if(mpos>=0) fen=fen.substring(0,mpos); fen.trim(); loadFEN(fen); }
  }
  int m=s.indexOf(" moves ");
  if(m>=0){
    String rest=s.substring(m+7); rest.trim(); int i=0;
    while(i<rest.length()){
      char f1=rest[i++]; char r1=rest[i++]; char f2=rest[i++]; char r2=rest[i++];
      int from=(r1-'1')*8+(f1-'a'); int to=(r2-'1')*8+(f2-'a');
      MoveList list; generateMoves(list);
      for(int j=0;j<list.count;j++){ Move m=list.moves[j]; if(m.from==from && m.to==to){ makeMove(m); break; } }
      if(i<rest.length() && rest[i]=='q') i++;
      while(i<rest.length() && rest[i]==' ') i++;
    }
  }
}

int fixedDepth = 3;

void goCommand(const String& s){
  int p=s.indexOf("depth "); int d=fixedDepth; if(p>=0){ d=s.substring(p+6).toInt(); if(d<=0) d=fixedDepth; }
  Move bm=think(d); char buf[6]; printMove(bm,buf); Serial.print("bestmove "); Serial.println(buf);
}

// ---- Arduino entry points --------------------------------------
void setup(){
  initLeapers();
  Serial.begin(115200); while(!Serial){}
  setStartPos();
}

void loop(){
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
}

