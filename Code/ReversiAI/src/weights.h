#ifndef WEIGHTS_H_
#define WEIGHTS_H_

#define TYPE const int

// Percentage phase threshold for evaluation
#define earlyGame 0
#define midGame 100

int WEIGHT_Bombs = 0;
int WEIGHT_OverrideStones = 0;


TYPE WEIGHT_ChoiceStone = -10000000;
TYPE WEIGHT_Frontier = -8;


// Weights for bombing phase only

TYPE WEIGHT_NumberOfBombs 					 = 1;
TYPE WEIGHT_BombRadius 						 = 1;

#endif /* WEIGHTS_H_ */
