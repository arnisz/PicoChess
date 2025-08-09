#pragma once

#include "board.hpp"

struct Move {
  uint8_t from, to;
  uint8_t piece;
  uint8_t capture;
  uint8_t promo;
  uint8_t flags;
  int score;
};

struct MoveList { Move moves[256]; int count; };

void addMove(MoveList &list, Move m);
void generateMoves(MoveList &list);
