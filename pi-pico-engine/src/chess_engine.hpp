#pragma once

#include "move_generator.hpp"
#include "evaluation.hpp"

struct History {
  Move m;
  int castle, ep, half;
};

extern History history[128];
extern int histPly;

bool makeMove(const Move &m);
void unmakeMove();
int quiesce(int alpha,int beta);
int search(int depth,int alpha,int beta);
Move thinkDepth(int depth);
Move thinkTime(int milliseconds);
void parsePosition(const String& s);
void goCommand(const String& s);
void initEngine();
