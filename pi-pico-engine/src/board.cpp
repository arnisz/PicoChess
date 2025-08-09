#include "board.hpp"

U64 bitboards[12];
U64 occupancies[3];
int side = WHITE;
int castle = 0;
int enpassant = -1;
int halfmove = 0, fullmove = 1;

U64 pawnAttacks[2][64];
U64 knightAttacks[64];
U64 kingAttacks[64];

void setBit(U64 &bb, int sq){ bb |= 1ULL << sq; }
void popBit(U64 &bb, int sq){ bb &= ~(1ULL << sq); }
bool getBit(U64 bb, int sq){ return bb & (1ULL << sq); }
int lsb(U64 bb){ return __builtin_ctzll(bb); }
int popLSB(U64 &bb){ int sq = lsb(bb); bb &= bb-1; return sq; }
int countBits(U64 bb){ return __builtin_popcountll(bb); }

void initLeapers(){
  for(int sq=0; sq<64; sq++){
    int r = sq / 8, f = sq % 8;
    if(r < 7){
      if(f > 0) pawnAttacks[WHITE][sq] |= 1ULL << (sq + 7);
      if(f < 7) pawnAttacks[WHITE][sq] |= 1ULL << (sq + 9);
    }
    if(r > 0){
      if(f > 0) pawnAttacks[BLACK][sq] |= 1ULL << (sq - 9);
      if(f < 7) pawnAttacks[BLACK][sq] |= 1ULL << (sq - 7);
    }
    if(r + 2 <= 7 && f + 1 <= 7) knightAttacks[sq] |= 1ULL << (sq + 17);
    if(r + 1 <= 7 && f + 2 <= 7) knightAttacks[sq] |= 1ULL << (sq + 10);
    if(r + 2 <= 7 && f - 1 >= 0) knightAttacks[sq] |= 1ULL << (sq + 15);
    if(r + 1 <= 7 && f - 2 >= 0) knightAttacks[sq] |= 1ULL << (sq + 6);
    if(r - 2 >= 0 && f + 1 <= 7) knightAttacks[sq] |= 1ULL << (sq - 15);
    if(r - 1 >= 0 && f + 2 <= 7) knightAttacks[sq] |= 1ULL << (sq - 6);
    if(r - 2 >= 0 && f - 1 >= 0) knightAttacks[sq] |= 1ULL << (sq - 17);
    if(r - 1 >= 0 && f - 2 >= 0) knightAttacks[sq] |= 1ULL << (sq - 10);
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

int pieceAt(int sq){
  for(int p=WP; p<=BK; p++) if(getBit(bitboards[p], sq)) return p;
  return NO_PIECE;
}

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
  if(i>=fen.length()) return false; i++;
  side = (fen[i]=='w') ? WHITE : BLACK;
  while(i<fen.length() && fen[i]!=' ') i++; i++;
  if(fen[i]=='-') i++; else {
    while(i<fen.length() && fen[i]!=' '){ char c=fen[i++]; if(c=='K') castle|=WKC; if(c=='Q') castle|=WQC; if(c=='k') castle|=BKC; if(c=='q') castle|=BQC; }
  }
  while(i<fen.length() && fen[i]==' ') i++;
  if(fen[i]=='-'){ enpassant=-1; i++; } else { int f=fen[i]-'a', r=fen[i+1]-'1'; enpassant=r*8+f; i+=2; }
  updateOccupancies();
  return true;
}

void setStartPos(){
  loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}
