#include "move_generator.hpp"

void addMove(MoveList &list, Move m){ list.moves[list.count++] = m; }

void generateMoves(MoveList &list){
  list.count=0;
  U64 bb, attacks;

  if(side==WHITE){
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

    bb = bitboards[WB];
    while(bb){
      int from = popLSB(bb);
      attacks = maskBishopAttacks(from, occupancies[BOTH]) & ~occupancies[WHITE];
      while(attacks){
        int t = popLSB(attacks); int cap = pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0;
        addMove(list,{(uint8_t)from,(uint8_t)t,WB,(uint8_t)cap,NO_PIECE,fl,0});
      }
    }

    bb = bitboards[WR];
    while(bb){
      int from = popLSB(bb);
      attacks = maskRookAttacks(from, occupancies[BOTH]) & ~occupancies[WHITE];
      while(attacks){
        int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0;
        addMove(list,{(uint8_t)from,(uint8_t)t,WR,(uint8_t)cap,NO_PIECE,fl,0});
      }
    }

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

    int from = lsb(bitboards[WK]);
    attacks = kingAttacks[from] & ~occupancies[WHITE];
    while(attacks){
      int t=popLSB(attacks); int cap=pieceAt(t); uint8_t fl=cap!=NO_PIECE?1:0;
      addMove(list,{(uint8_t)from,(uint8_t)t,WK,(uint8_t)cap,NO_PIECE,fl,0});
    }

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
