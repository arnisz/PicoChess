#pragma once

#include "platform.hpp"
#include <cstdint>

using U64 = uint64_t;

enum { WHITE, BLACK, BOTH };

enum Piece {
  WP, WN, WB, WR, WQ, WK,
  BP, BN, BB, BR, BQ, BK, NO_PIECE
};

enum CastleRights { WKC=1, WQC=2, BKC=4, BQC=8 };

extern U64 bitboards[12];
extern U64 occupancies[3];
extern int side;
extern int castle;
extern int enpassant;
extern int halfmove, fullmove;

extern U64 pawnAttacks[2][64];
extern U64 knightAttacks[64];
extern U64 kingAttacks[64];

void setBit(U64 &bb, int sq);
void popBit(U64 &bb, int sq);
bool getBit(U64 bb, int sq);
int lsb(U64 bb);
int popLSB(U64 &bb);
int countBits(U64 bb);

void initLeapers();
U64 maskRookAttacks(int sq, U64 block);
U64 maskBishopAttacks(int sq, U64 block);
void updateOccupancies();
int pieceAt(int sq);
bool squareAttacked(int sq, int bySide);

void clearBoard();
bool loadFEN(const String &fen);
void setStartPos();
