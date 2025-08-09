#pragma once

#include "board.hpp"

static const int pieceValue[12] = {
  100,320,330,500,900,0,
  -100,-320,-330,-500,-900,0
};

inline int evaluate(){
  int s=0;
  for(int p=WP; p<=BK; p++) s += pieceValue[p] * countBits(bitboards[p]);
  return (side==WHITE ? s : -s);
}
