#ifndef ALGORITHMS_H_
#define ALGORITHMS_H_

#include "map.h"

#define MINIMAX 1
#define ALPHABETA 2
#define ALPHABETA_MOVESORTING 3
#define ASPIRATIONAL_WINDOW 4

void initTimer();
Move* getNextMove(Map& map, int searchDepth, int searchTime, uint8_t player, int algorithm, bool isPlayingPhase);
void close();

// For benchmarking only!
uint64_t getNumberOfAnalyzedNodes(void);
long getTimeSpendEvaluating(void);

#endif /* ALGORITHMS_H_ */
