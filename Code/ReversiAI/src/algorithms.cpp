// The functions provided by this file are used to determine the next best
// based on the board evaluation methodes from the map.cpp file.
// At the moment there are 2 algorithm implemented, minimax and alphabeta
// pruning, both with paranoid search. For alpha beta pruning there is the
// option to use a move sorting and an aspiration window optimization.

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <cmath>
#include <signal.h>
#include <string.h>
#include <list>
#include <iterator>
#include <atomic>
#include <climits>
#include <map>
#include <stdint.h>
#include "algorithms.h"
#include "map.h"

using namespace std;

// Variables for benchmarking tests
struct timespec spec;
long timeSpendEvaluating=0;
long startTime,endTime;
int nodesAnalyzed = 0;

// Variables for the AI
Move bestMove;

double const windowSize = 5; // in percentage
double const windowResize = 20; // half of percentage as we are resizing in both directions
bool wasWindowSuccesfull=false;

int currentChoice=0;
int currentCell=0;

int numberOfPlayers;
int initialDepth=0;
int playerID =0;
long searchTime;
long searchStart;
long currentTime;
int nextPlayer=0;

static volatile sig_atomic_t hasTimePassed, hasHalfTimePassed;

int nodeCount;
int lastNodeCount;
int branchingFactor=15;
int breakCount=0;
int successCount=0;

struct sigaction sa;
struct itimerval timer;

int64_t score;

void setBestMove();
void updateBestMove(uint16_t cell, char choice, int64_t* best);
int64_t moveSorting_firstIte(Map& map, uint8_t turn, multimap<int,int>* nextMoves, bool isPlayingPhase);
int64_t moveSorting(Map& map, uint8_t player, int currentDepth, int64_t alpha, int64_t beta, multimap<int,int>* curMoves, multimap<int,int>* nextMoves, bool isPlayingPhase);
int64_t alphabeta(Map& map, uint8_t turn, int depth, bool isPlayingPhase, int64_t alpha, int64_t beta);
int64_t minimax(Map& map, uint8_t turn, int depth, bool isPlayingPhase);
void startTimer();
void endTimer();

/**
 * The callback function that gets called if a signal is send.
 *
 * @param signum - The type of signal that initiated the callback function. We only use this function to handle the timers signal
 */
void timerHandler(int signum)
{
	// Set the first flag after the first call
	if(!hasHalfTimePassed)
	{
		cout << "G3-Timer: First timer signal has been send, not starting the next iterative deepening iteration." << endl;
		hasHalfTimePassed=true;
	}
	else // if the first flag is set already, set the second one
	{
		cout << "G3-Timer: Time has run out, returning the currently best move." << endl;
		hasTimePassed=true;
	}
}

/**
 * Initialise the callback function for the timer. Call this method before expecting a search time limit to work.
 */
void initTimer()
{
	/* Install timer_handler as the signal handler for SIGVTALRM. */

	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &timerHandler;
	sigaction (SIGALRM, &sa, 0);	// sets the function timer_handler to be executed once the timer triggers

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
}

/**
 * At the moment only used to display the amount of failed and successful time predictions.
 */
void close(){
	cout << "Failures: "<<breakCount <<endl;
	cout << "Success: "<<successCount<<endl;
}

/**
 * The function will look for the best move it can find with the given search depth or time limit constraints.
 * Note that if both limits are set, only the depth limit will apply and the time limit will be set to its default of 90 seconds.
 *
 * @param map - The initial state of the board
 * @param searchDepth - The depth up to which the search tree should be build. Set to 0 to use a time limit.
 * @param searchTime - The time limit set for the move search. Does not apply if searchDepth has been set.
 * @param player - The player who can make a move.
 * @param algorithm - The identifier for the algorithm that should be used
 * @param isPlayeringPhase - Used to distinguish between playing and bombing phase
 *
 * @return A pointer to the move identified as the best move.
 */
Move* getNextMove(Map& map, int searchDepth, int searchTime, uint8_t player, int algorithm, bool isPlayingPhase)
{
	if(searchDepth==0)
	{
		searchTime = (searchTime-150);	// reserve 150 ms return a move and close recursion stacks
		searchDepth = 100; // set search depth to an unreachable amount
	}
	else
	{
		searchTime = 120000; // Default timeout timer, in case a search depth is specified
	}

	hasHalfTimePassed = false;
	hasTimePassed = false;

#ifndef BENCHMARK // disable the timer for benchmarking, as otherwise it will fire after 120 seconds

	int timeBeforeConsideringToStop = (int)((1.0/branchingFactor)*searchTime);

	timer.it_value.tv_sec = timeBeforeConsideringToStop/1000;
	timer.it_value.tv_usec = (timeBeforeConsideringToStop%1000)*1000;
	timer.it_interval.tv_sec = (searchTime-timeBeforeConsideringToStop)/1000;
	timer.it_interval.tv_usec = ((searchTime-timeBeforeConsideringToStop)%1000)*1000;
	setitimer (ITIMER_REAL, &timer, 0);

#endif

	nodeCount=0;

	playerID = player;
	bestMove.player=player;

	int currentDepth=1;
	numberOfPlayers = getAmountOfPlayers();

	if(algorithm==MINIMAX)
	{
		while(currentDepth<=searchDepth && !hasHalfTimePassed)
		{
			cout << "Searching with search depth "<<currentDepth<<endl;
			initialDepth=currentDepth;

			// Execute minimax with the current depth limit
			minimax(map, player, currentDepth, isPlayingPhase);
			if(hasTimePassed)
			{
				break;
			}

			// Save the best move found at that iteration
			setBestMove();

			currentDepth++;
		}
	}
	else if(algorithm==ALPHABETA)
	{
		while(currentDepth<=searchDepth && !hasHalfTimePassed)
		{
			cout << "Searching with tree depth "<<currentDepth << endl;
			initialDepth=currentDepth;

			// Execute alphabeta with the current depth limit
			alphabeta(map, player, currentDepth, isPlayingPhase, INT64_MIN, INT64_MAX);
			if(hasTimePassed)
			{
				break;
			}

			// Save the best move found at that iteration
			setBestMove();

			currentDepth++;
		}
	}
	else if(algorithm==ALPHABETA_MOVESORTING)
	{
		multimap<int,int> curMoves;
		multimap<int,int> nextMoves;

		cout << "Searching with tree depth 1"<<endl;
		nodeCount=0;

		setConsiderOverrideStones(false);
		int score = moveSorting_firstIte(map, player, &curMoves, isPlayingPhase);
		if(curMoves.empty())
		{
			setConsiderOverrideStones(true);
			score = moveSorting_firstIte(map, player, &curMoves, isPlayingPhase);
		}

		lastNodeCount=nodeCount;

		// Save the move the of the first iteration
		setBestMove();

		currentDepth++;

		while(currentDepth<=searchDepth && !hasHalfTimePassed && (score>INT64_MIN+MAX_PLAYER) && (score<INT64_MAX-MAX_PLAYER))
		{

			cout << "Searching with tree depth "<<currentDepth << endl;
			initialDepth=currentDepth;
			nodesAnalyzed=0;

			moveSorting(map, player, currentDepth, INT64_MIN, INT64_MAX, &curMoves, &nextMoves, isPlayingPhase);
			if(hasTimePassed)
			{
				break;
			}

			nodeCount+=nodesAnalyzed;
			lastNodeCount=nodesAnalyzed;

			// Save the best move found at that iteration
			if(isPlayingPhase || score>INT64_MAX-MAX_PLAYER || score<=INT64_MIN+MAX_PLAYER)
			{
				setBestMove();
			}

			curMoves.swap(nextMoves);
			nextMoves.clear();

			currentDepth++;
		}

		if(hasTimePassed)
		{
			breakCount++;
		}
		else
		{
			successCount++;

			if(currentDepth>2)
			{
				branchingFactor = (int)(log(lastNodeCount)/log(currentDepth-1));
			}
			else
			{
				branchingFactor=lastNodeCount;
			}

			// Add one as our predication seem to be a bit to tight(only 66% success)
			branchingFactor+=2;

			cout << "Branching Faktor: "<<branchingFactor <<endl;
		}
	}
	else if(algorithm==ASPIRATIONAL_WINDOW)
	{
		multimap<int,int> curMoves;
		multimap<int,int> nextMoves;

		bool isSecondTry=false;

		cout << "Searching with tree depth 1"<<endl;
		nodesAnalyzed=0;
		int moveValue = moveSorting_firstIte(map, player, &curMoves, isPlayingPhase);
		cout << "Value: "<< moveValue<<endl;
		nodeCount+=nodesAnalyzed;

		// Save the move the of the first iteration
		setBestMove();

		currentDepth++;

		int alpha=INT_MIN,beta=INT_MAX;
		int lastScore=moveValue;

		alpha = moveValue - (windowSize*getAmountOfCells());
		beta = moveValue + (windowSize*getAmountOfCells());

		while(currentDepth<=searchDepth && !hasHalfTimePassed)
		{
			cout << "Searching with tree depth "<<currentDepth << endl;
			initialDepth=currentDepth;
			nodesAnalyzed=0;
			cout << "alpha: "<< alpha << "	beta: "<< beta << endl;

			moveValue = moveSorting(map, player, currentDepth, INT_MIN, INT_MAX, &curMoves, &nextMoves, isPlayingPhase);
			if(hasTimePassed)
			{
				break;
			}
			nodeCount+=nodesAnalyzed;

			cout << "Value: "<< moveValue<<endl;

			if(alpha<moveValue && moveValue<beta)
			{
				currentDepth++;
				lastScore=moveValue;

				// Save the best move found at that iteration
				setBestMove();

				curMoves.swap(nextMoves);
				nextMoves.clear();

				isSecondTry=false;

				alpha = moveValue - (windowSize*getAmountOfCells());
				beta = moveValue + (windowSize*getAmountOfCells());
			}
			else if(!isSecondTry)
			{
				isSecondTry=true;
				if(moveValue<=alpha)// fail low
				{
					alpha = lastScore- windowResize*getAmountOfCells();
				}
				else // fail high
				{
					beta = lastScore + windowResize*getAmountOfCells();
				}
			}
			else
			{
				alpha=INT_MIN;
				beta=INT_MAX;
			}
		}
	}

	//Cancel Timer
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, 0);

	return &bestMove;
}

/**
 * This function uses the data from the move searching algorithm and sets
 * the move that will be returned, if the timer run out, to the one found
 * by that algorithm.
 */
void setBestMove()
{
	int x,y;
	reverseOffset(&x, &y, currentCell);
	bestMove.x=x;
	bestMove.y=y;
	bestMove.choice=currentChoice;

	cout << "Current Best Move: ("<<bestMove.x<<", "<<bestMove.y<<", "<<(int)bestMove.choice<<")" << endl;
}

/**
 * Updates the best move if it was better than the last one. This and the values for
 * score and best move are stored outside of the recursive function so we can use one function for all
 * the algorithms.
 */
void updateBestMove(uint16_t cell, char choice, int64_t* best)
{
	if(score>=*best)
	{
		*best=score;
		currentCell=cell;
		currentChoice=choice;
	}
}

/**
 * Execute the first iteration of move sorting.
 *
 * @param map - The inital state of the game
 * @param player - Identifies who can make the next move
 * @param nextMoves - A pointer to a list to store all possible moves, ordered by there expected value
 * @param isPlayingPhase - Used to distinguish between playing and bombing phase
 */
int64_t moveSorting_firstIte(Map& map, uint8_t player, multimap<int,int>* nextMoves, bool isPlayingPhase)
{
	nodesAnalyzed++;

	Map mapCopy;
	mapCopy.copy(map);

	int64_t best = INT64_MIN;

	if(isPlayingPhase)
	{
		char state;

		for(int cell=0; cell<getAmountOfCells(); ++cell)
		{
			if(hasTimePassed){
				return 0;
			}
			state= map.getState(cell);

			if(state=='b')
			{
				int64_t overrideScore;

				if(mapCopy.isPlayingPhaseMoveValid(cell,player,20)) // If move is valid
				{
					nodesAnalyzed+=2; // We know that the 2 choices are valid

#ifdef BENCHMARK
					startTimer();
#endif

					score=mapCopy.evaluateForPlayingPhase(playerID);
					overrideScore=score;
					if(hasTimePassed)
					{
						return 0;
					}

#ifdef BENCHMARK
					endTimer();
#endif

					updateBestMove(cell, 20, &best);

					// reset the mapCopy state
					mapCopy.copy(map);
				}

				if(mapCopy.isPlayingPhaseMoveValid(cell,player,21)) // If move is valid
				{
#ifdef BENCHMARK
					startTimer();
#endif

					score=mapCopy.evaluateForPlayingPhase(playerID);
					if(hasTimePassed){
						return 0;
					}

#ifdef BENCHMARK
					endTimer();
#endif

					// Insert the move to the list of possible move, such that the highest score is chosen as key
					if(overrideScore>score)
					{
						nextMoves->insert(pair<int,int>(overrideScore,cell));
					}
					else
					{
						nextMoves->insert(pair<int,int>(score,cell));
					}

					updateBestMove(cell, 21, &best);

					// reset the mapCopy state
					mapCopy.copy(map);
				}
			}
			else if(state=='c')
			{
				// use int min to identify the
				int64_t max = INT64_MIN;

				for(int p=1; p<=getAmountOfPlayers(); ++p)
				{
					if(mapCopy.isPlayingPhaseMoveValid(cell, player, p)) // If move is valid
					{
						nodesAnalyzed++;
#ifdef BENCHMARK
						startTimer();
#endif

						score=mapCopy.evaluateForPlayingPhase(playerID);
						if(hasTimePassed){
							return 0;
						}

#ifdef BENCHMARK
						endTimer();
#endif

						if(score>max){
							max=score;
						}

						updateBestMove(cell, p, &best);

						mapCopy.copy(map);
					}
				}
				if(max!=INT64_MIN)
				{
					nextMoves->insert(pair<int,int>(max,cell));
				}
			}
			else // normal cell
			{
				if(mapCopy.isPlayingPhaseMoveValid(cell,player,0)) // If move is valid
				{
					nodesAnalyzed++;
#ifdef BENCHMARK
					startTimer();
#endif

					score=mapCopy.evaluateForPlayingPhase(playerID);
					if(hasTimePassed){
						return 0;
					}

#ifdef BENCHMARK
					endTimer();
#endif

					updateBestMove(cell, 0, &best);

					nextMoves->insert(pair<int,int>(score,cell));

					mapCopy.copy(map);
				}
			}
		}

		return best;
	}
	else // bombing phase
	{
		for(int cell=0; cell<getAmountOfCells(); ++cell)
		{
			if(mapCopy.isBombingPhaseMoveValid(cell,player,0)) // If move is valid
			{
				nodesAnalyzed++;
#ifdef BENCHMARK
				startTimer();
#endif

				score=mapCopy.evaluateForBombingPhase(playerID);
				if(hasTimePassed){
					return 0;
				}

#ifdef BENCHMARK
				endTimer();
#endif

				updateBestMove(cell, 0, &best);

				nextMoves->insert(pair<int,int>(score,cell));

				mapCopy.copy(map);
			}
		}

		return best;
	}
}

/**
 * Execute all except the first iteration of move sorting.
 *
 * @param map - The inital state of the game
 * @param player - Identifies who can make the next move
 * @param currentDepth - The depth to which the subtree should be built
 * @param alpha - The alpha value for alpha-beta-pruning
 * @param beta - The beta value for alpha-beta-pruning
 * @param curMoves - A pointer to a list over which should be iterated for the moves in the root of the search tree
 * @param nextMoves - A pointer to a list to specify the order of moves for the next iteration
 * @param isPlayingPhase - Used to distinguish between playing and bombing phase
 */
int64_t moveSorting(Map& map, uint8_t player, int currentDepth, int64_t alpha, int64_t beta, multimap<int,int>* curMoves, multimap<int,int>* nextMoves, bool isPlayingPhase)
{
	static int numberOfRepeatings=0;

	nodesAnalyzed++;

	if(currentDepth==0)
	{
#ifdef BENCHMARK
		startTimer();
#endif

		if(isPlayingPhase)
		{
			score = map.evaluateForPlayingPhase(playerID);
		}
		else
		{

			score = map.evaluateForBombingPhase(playerID);
		}

#ifdef BENCHMARK
		endTimer();
#endif

		return score;
	}

	Map mapCopy;
	mapCopy.copy(map);

	int64_t best=INT64_MIN;

	uint8_t nextPlayer=getNextPlayer(player);

	if(currentDepth==initialDepth)
	{

		numberOfRepeatings=0;
		if(isPlayingPhase)
		{
			char state;

			for (multimap<int,int>::reverse_iterator cell = curMoves->rbegin(); cell != curMoves->rend(); ++cell)
			{
				state= map.getState((*cell).second);

				if(state=='b')
				{
					int64_t overrideScore;

					if(mapCopy.isPlayingPhaseMoveValid((*cell).second,player,20)) // If move is valid
					{
						score = moveSorting(mapCopy, nextPlayer, currentDepth-1, alpha, beta, NULL, NULL, true);
						overrideScore=score;
						if(hasTimePassed){
							return 0;
						}

						if(overrideScore>best)
						{
							best=overrideScore;
						}

						if(overrideScore>alpha)
						{
							alpha=overrideScore;
							updateBestMove((*cell).second, 20, &best);

						}
						mapCopy.copy(map);
					}

					if(mapCopy.isPlayingPhaseMoveValid((*cell).second,player,21)) // If move is valid
					{
						score = moveSorting(mapCopy, nextPlayer, currentDepth-1, alpha, beta, NULL, NULL, true);
						if(hasTimePassed){
							return 0;
						}

						if(score>best)
						{
							best=score;
						}

						if(score>alpha)
						{
							alpha=score;
							updateBestMove((*cell).second, 21, &best);
						}

						if(score>overrideScore)
						{
							nextMoves->insert(pair<int,int>(score,(*cell).second));
						}
						else
						{
							nextMoves->insert(pair<int,int>(overrideScore,(*cell).second));
						}

						mapCopy.copy(map);
					}
				}
				else if(state=='c')
				{
					int64_t max=INT64_MIN;

					for(uint8_t p=1; p<=getAmountOfPlayers(); ++p)
					{
						if(mapCopy.isPlayingPhaseMoveValid((*cell).second, player, p)) // If move is valid
						{
							score=moveSorting(mapCopy, nextPlayer ,currentDepth-1, alpha, beta, NULL, NULL, true); // Go deeper into the tree
							if(hasTimePassed){
								return 0;
							}

							if(score>max)
							{
								max=score;
							}

							if(score>best)
							{
								best=score;
							}

							if(score>alpha)
							{
								alpha=score;
								updateBestMove((*cell).second, p, &best);
							}
							mapCopy.copy(map);
						}
					}

					nextMoves->insert(pair<int,int>(max, (*cell).second));
				}
				else
				{
					if(mapCopy.isPlayingPhaseMoveValid((*cell).second,player,0)) // If move is valid
					{
						int score = moveSorting(mapCopy, nextPlayer, currentDepth-1, alpha, beta, NULL, NULL, true);
						if(hasTimePassed){
							return 0;
						}

						if(score>best)
						{
							best=score;
						}

						if(score>alpha)
						{
							alpha=score;
							updateBestMove((*cell).second, 0, &best);
						}

						nextMoves->insert(pair<int,int>(score,(*cell).second));

						mapCopy.copy(map);
					}
				}
			}
		}
		else // bombing phase
		{
			for (multimap<int,int>::reverse_iterator cell = curMoves->rbegin(); cell != curMoves->rend(); ++cell)
			{
				if(mapCopy.isBombingPhaseMoveValid((*cell).second, player, 0)) // If move is valid
				{
					score = moveSorting(mapCopy, nextPlayer, currentDepth-1, alpha, beta, NULL, NULL, false);
					if(hasTimePassed){
						return 0;
					}

					if(score>best)
					{
						best=score;
					}

					if(score>alpha)
					{
						alpha=score;
						updateBestMove((*cell).second, 0, &best);
					}

					nextMoves->insert(pair<int,int>(score,(*cell).second));

					mapCopy.copy(map);
				}
			}
		}

		return best; // Return alpha to indicate the score of the best move found
	}

	// Any node that is not the root or a leaf

	bool hasFoundMove=false;

	if(player!=playerID)
	{
		best=INT_MAX;
	}

	if(isPlayingPhase)
	{
		char state;

		for(int cell=0; cell<getAmountOfCells(); ++cell)
		{
			state= map.getState(cell);
			if(state=='b')
			{
				if(mapCopy.isPlayingPhaseMoveValid(cell,player,21)) // If move is valid
				{
					hasFoundMove=true;
					numberOfRepeatings=0;

					score = moveSorting(mapCopy, nextPlayer, currentDepth-1, alpha, beta, NULL, NULL, true);
					if(hasTimePassed){
						return 0;
					}

					if(player==playerID) // maximizer
					{
						if(score>best){
							best=score;
							if(score>alpha)
							{
								alpha=score;
							}
						}
					}
					else // minimizer
					{
						if(score<best)
						{
							best=score;
							if(score<beta)
							{
								beta=score;
							}
						}
					}
					// Check if node can be pruned away
					if(alpha>=beta)
					{
						return best;
					}

					mapCopy.copy(map);
				}

				if(mapCopy.isPlayingPhaseMoveValid(cell,player,21)) // If move is valid
				{
					score = moveSorting(mapCopy, nextPlayer, currentDepth-1, alpha, beta, NULL, NULL, true);
					if(hasTimePassed){
						return 0;
					}

					if(player==playerID) // maximizer
					{
						if(score>best){
							best=score;
							if(score>alpha)
							{
								alpha=score;
							}
						}
					}
					else // minimizer
					{
						if(score<best)
						{
							best=score;
							if(score<beta)
							{
								beta=score;
							}
						}
					}
					// Check if node can be pruned away
					if(alpha>=beta)
					{
						return best;
					}

					mapCopy.copy(map);
				}
			}
			else if(state=='c')
			{
				for(uint8_t p=1; p<=getAmountOfPlayers();  ++p)
				{
					if(mapCopy.isPlayingPhaseMoveValid(cell, player, p)) // If move is valid
					{
						hasFoundMove=true;
						numberOfRepeatings=0;

						score = moveSorting(mapCopy, nextPlayer, currentDepth-1, alpha, beta, NULL, NULL, true);
						if(hasTimePassed){
							return 0;
						}

						if(player==playerID) // maximizer
						{
							if(score>best){
								best=score;
								if(score>alpha)
								{
									alpha=score;
								}
							}
						}
						else // minimizer
						{
							if(score<best)
							{
								best=score;
								if(score<beta)
								{
									beta=score;
								}
							}
						}
						// Check if node can be pruned away
						if(alpha>=beta)
						{
							return best;
						}

						mapCopy.copy(map);
					}
				}
			}
			else
			{
				if(mapCopy.isPlayingPhaseMoveValid(cell,player,0)) // If move is valid
				{
					hasFoundMove=true;
					numberOfRepeatings=0;

					score = moveSorting(mapCopy, nextPlayer, currentDepth-1, alpha, beta, NULL, NULL, true);
					if(hasTimePassed){
						return 0;
					}

					if(player==playerID) // maximizer
					{
						if(score>best){
							best=score;
							if(score>alpha)
							{
								alpha=score;
							}
						}
					}
					else // minimizer
					{
						if(score<best)
						{
							best=score;
							if(score<beta)
							{
								beta=score;
							}
						}
					}

					// Check if node can be pruned away
					if(alpha>=beta)
					{
						return best;
					}

					mapCopy.copy(map);
				}
			}
		}
	}
	else // bombing phase
	{
		for (int cell=0; cell<getAmountOfCells(); ++cell)
		{
			if(mapCopy.isBombingPhaseMoveValid(cell, player, 0)) // If move is valid
			{
				hasFoundMove=true;
				numberOfRepeatings=0;
				score = moveSorting(mapCopy, nextPlayer, currentDepth-1, alpha, beta, NULL, NULL, false);
				if(hasTimePassed){
					return 0;
				}

				if(player==playerID) // maximizer
				{
					if(score>best){
						best=score;
						if(score>alpha)
						{
							alpha=score;
						}
					}
				}
				else // minimizer
				{
					if(score<best)
					{
						best=score;
						if(score<beta)
						{
							beta=score;
						}
					}
				}
				// Check if node can be pruned away
				if(alpha>=beta)
				{
					return best;
				}

				mapCopy.copy(map);
			}
		}
	}

	// If no move has been found execute it for the next player in the list, except if no player can make a move
	if(!hasFoundMove && numberOfRepeatings<getAmountOfConsideredPlayers())
	{
		numberOfRepeatings++;
		return moveSorting(mapCopy, nextPlayer, currentDepth, alpha, beta, NULL, NULL, isPlayingPhase);
	}
	else if(!hasFoundMove) // If no player can make a move in the current phase
	{
		if(isPlayingPhase) // Change to bombing phase if we are in the playing phase
		{
			if(!getConsiderOverrideStones())
			{
				setConsiderOverrideStones(true);
				numberOfRepeatings=0;
				return moveSorting(mapCopy, nextPlayer, currentDepth, alpha, beta, NULL, NULL, true);
			}
			return moveSorting(mapCopy, nextPlayer, currentDepth, alpha, beta, NULL, NULL, false);
		}
		else // Evaluate for end of game otherwise
		{
			return map.evaluateForEndOfGame(playerID);
		}
	}

	return best;
}

/**
 * Execute minimax with alphabeta pruning to a specified depth. If the timer for the move runs out, 0 is returned immediately.
 * For alphabeta pruning every node has the information which is the minimal(in case of minimizer nodes) or the maximal(in case of maximizer nodes) value
 * of its parent node. While building its subtree, it constantly checks whether its value is higher than the reference value(if parent=minimizer) or lower
 * otherwise. If this is the case, the node returns immediatly and the subtree is pruned away.
 *
 * @param map - The initial game state
 * @param turn - The player who has to make a move
 * @param depth - Specifies to what depth the game tree should be built
 * @param isPlayingPhase - Specifies in which phase of the game currently is active
 * @param alpha - The currently highest value of a parent maximizer
 * @param beta - The currently lowest value of a parant minimizer
 *
 * @return The minimal value of all child nodes in case of minimizer, the maximum value otherwise
 */
int64_t alphabeta(Map& map, uint8_t turn, int depth, bool isPlayingPhase, int64_t alpha, int64_t beta)
{
	static int numberOfRepeatings = 0;

	if(hasTimePassed){
		return 0;
	}

	nodesAnalyzed++;

	int64_t score;

	if(depth==0)	// If a leaf is reached, analyze the state for every player and return the value
	{

#ifdef BENCHMARK
		startTimer();
#endif


		if(isPlayingPhase)
		{
			score = map.evaluateForPlayingPhase(playerID);
		}
		else
		{
			score = map.evaluateForBombingPhase(playerID);
		}

#ifdef BENCHMARK
		endTimer();
#endif

		return score;
	}

	Map mapCopy;

	bool foundMove = false;

	uint8_t state;

	int64_t best=INT64_MIN;

	if(turn!=playerID)
	{
		best=INT64_MAX;
	}

	for(int i=0; i<getAmountOfCells(); ++i)
	{
		mapCopy.copy(map);

		if(hasTimePassed)
		{
			return 0;
		}

		if(isPlayingPhase)
		{
			state = map.getState(i);

			switch(state)
			{
				case 'b':	if(mapCopy.isPlayingPhaseMoveValid(i,turn,20)) // If move is valid
							{
								if(!foundMove)	// if no move has been found so far, use the first one
								{
									foundMove=true;
									numberOfRepeatings=0;	// reset counter
								}

								// Get the next player, who has not been disqualified
								nextPlayer=(turn%numberOfPlayers)+1;
								while(isDisqualified(nextPlayer))
								{
									nextPlayer=(nextPlayer%numberOfPlayers)+1;
								}

								int score;
								score=alphabeta(mapCopy, nextPlayer, depth-1, true, alpha, beta); // Go deeper into the tree

								if(turn==playerID) // maximizer
								{
									if(score>best){
										best=score;
										if(score>alpha)
										{
											alpha=score;
											if(depth==initialDepth)
											{
												currentCell=i;
												currentChoice=20;
											}
										}
									}
								}
								else // minimizer
								{
									if(score<best)
									{
										best=score;
										if(score<beta)
										{
											beta=score;
										}
									}
								}

								// Prune away the rest of the tree
								if(alpha>=beta)
								{
									return score;
								}
							}

							if(mapCopy.isPlayingPhaseMoveValid(i,turn,21)) // If move is valid
							{
								if(!foundMove)	// if no move has been found so far, use the first one
								{
									foundMove=true;
									numberOfRepeatings=0;	// reset counter
								}

								// Get the next player, who has not been disqualified
								nextPlayer=(turn%numberOfPlayers)+1;
								while(isDisqualified(nextPlayer))
								{
									nextPlayer=(nextPlayer%numberOfPlayers)+1;
								}

								int score;
								score=alphabeta(mapCopy, nextPlayer, depth-1, true, alpha, beta); // Go deeper into the tree

								if(turn==playerID) // maximizer
								{
									if(score>best){
										best=score;
										if(score>alpha)
										{
											alpha=score;
											if(depth==initialDepth)
											{
												currentCell=i;
												currentChoice=21;
											}
										}
									}
								}
								else // minimizer
								{
									if(score<best)
									{
										best=score;
										if(score<beta)
										{
											beta=score;
										}
									}
								}

								// Prune away the rest of the tree
								if(alpha>=beta)
								{
									return score;
								}
							}
							break;

				case 'c':	for(uint8_t p=1; p<=getAmountOfPlayers(); ++p)
							{
								if(!foundMove)	// if no move has been found so far, use the first one
								{
									foundMove=true;
									numberOfRepeatings=0;	// reset counter
								}

								if(mapCopy.isPlayingPhaseMoveValid(i,turn,p)) // If move is valid
								{
									// Get the next player, who has not been disqualified
									nextPlayer=(turn%numberOfPlayers)+1;
									while(isDisqualified(nextPlayer))
									{
										nextPlayer=(nextPlayer%numberOfPlayers)+1;
									}

									int score;
									score=alphabeta(mapCopy, nextPlayer, depth-1, true, alpha, beta); // Go deeper into the tree

									if(turn==playerID) // maximizer
									{
										if(score>best){
											best=score;
											if(score>alpha)
											{
												alpha=score;
												if(depth==initialDepth)
												{
													currentCell=i;
													currentChoice=p;
												}
											}
										}
									}
									else // minimizer
									{
										if(score<best)
										{
											best=score;
											if(score<beta)
											{
												beta=score;
											}
										}
									}

									// Prune away the rest of the tree
									if(alpha>=beta)
									{
										return score;
									}
								}
							}
							break;

				default:	if(mapCopy.isPlayingPhaseMoveValid(i,turn,0)) // If move is valid
							{
								if(!foundMove)	// if no move has been found so far, use the first one
								{
									foundMove=true;
									numberOfRepeatings=0;	// reset counter
								}

								// Get the next player, who has not been disqualified
								nextPlayer=(turn%numberOfPlayers)+1;
								while(isDisqualified(nextPlayer))
								{
									nextPlayer=(nextPlayer%numberOfPlayers)+1;
								}

								int score;
								score=alphabeta(mapCopy, nextPlayer, depth-1, true, alpha, beta); // Go deeper into the tree

								if(turn==playerID) // maximizer
								{
									if(score>best){
										best=score;
										if(score>alpha)
										{
											alpha=score;
											if(depth==initialDepth)
											{
												currentCell=i;
												currentChoice=0;
											}
										}
									}
								}
								else // minimizer
								{
									if(score<best)
									{
										best=score;
										if(score<beta)
										{
											beta=score;
										}
									}
								}

								// Prune away the rest of the tree
								if(alpha>=beta)
								{
									return score;
								}
							}
							break;
			}
		}
		else //-----BOMBING PHASE------
		{
			if(mapCopy.isBombingPhaseMoveValid(i,turn,0)) // If move is valid
			{
				if(!foundMove)	// if no move has been found so far, use the first one
				{
					foundMove=true;
					numberOfRepeatings=0;	// reset counter
				}

				// Get the next player, who has not been disqualified
				nextPlayer=(turn%numberOfPlayers)+1;
				while(isDisqualified(nextPlayer))
				{
					nextPlayer=(nextPlayer%numberOfPlayers)+1;
				}

				int score;
				score=alphabeta(mapCopy, nextPlayer, depth-1, false, alpha, beta); // Go deeper into the tree

				if(turn==playerID) // maximizer
				{
					if(score>best){
						best=score;
						if(score>alpha)
						{
							alpha=score;
							if(depth==initialDepth)
							{
								currentCell=i;
								currentChoice=0;
							}
						}
					}
				}
				else // minimizer
				{
					if(score<best)
					{
						best=score;
						if(score<beta)
						{
							beta=score;
						}
					}
				}

				// Prune away the rest of the tree
				if(alpha>=beta)
				{
					return score;
				}
			}
		}
	}

	// If no move has been found execute it for the next player in the list, except if no player can make a move
	if(!foundMove && numberOfRepeatings<numberOfPlayers)
	{
		numberOfRepeatings++;

		nextPlayer=(turn%numberOfPlayers)+1;
		while(isDisqualified(nextPlayer))
		{
			nextPlayer=(nextPlayer%numberOfPlayers)+1;
		}

		return alphabeta(mapCopy, nextPlayer, depth-1, true, alpha, beta);
	}
	else if(!foundMove)			// If no player can make a move switch to bombing phase if
	{
		if(isPlayingPhase)		// We are in the playing phase. Else just evaluate the board.
		{
			nextPlayer=(turn%numberOfPlayers)+1;
			while(isDisqualified(nextPlayer))
			{
				nextPlayer=(nextPlayer%numberOfPlayers)+1;
			}

			return alphabeta(mapCopy, nextPlayer, depth, false, alpha, beta);

		}
		else
		{
			return map.evaluateForEndOfGame(playerID);
		}
	}

	return best;	// Should only be reached if no player can make a move
}

/**
 * Executing minimax with paranoid strategy.
 * The board will be analyzed for the given player at the leafs of the game tree.
 * At each node the value shall be maximized for the given player turn, minimized for others.
 * A player's turn is represented by the branches leaving that specific node.
 *
 * @param map			 	- The current board state
 * @param turn				- The player who has to make a move
 * @param depth				- How deep(amount of successive moves) the algorithm should look from a specific node
 * @param isPlayingPhase	- Set if we are in the playing phase or bombing phase
 * @return		The evaluation value of the best move for that node
 */
int64_t minimax(Map& map, uint8_t turn, int depth, bool isPlayingPhase)
{
	static int numberOfRepeatings = 0;

	if(hasTimePassed){
		return 0;
	}

#ifdef BENCHMARK
	nodesAnalyzed++;
#endif

	if((depth<=0))	// If a leaf is reached, analyze the state for every player and return the value
	{

#ifdef BENCHMARK
		startTimer();
#endif

		int64_t evaValue;
		if(isPlayingPhase)
		{
			evaValue=map.evaluateForPlayingPhase(playerID);
		}
		else
		{
			evaValue = map.evaluateForBombingPhase(playerID);
		}

#ifdef BENCHMARK
		endTimer();
#endif

		return evaValue;
	}

	Map mapCopy;

	bool foundMove=false;
	int64_t best = INT_MIN;

	uint8_t state;

	for(int i=0; i<getAmountOfCells(); i++)
	{
		if(isPlayingPhase)
		{
			state = map.getState(i);

			if(state=='b')	// If that specific cell is a bonus cell, execute both moves:
			{				// Picking a bomb or picking an override stone

				mapCopy.copy(map);

				if(mapCopy.isPlayingPhaseMoveValid(i,turn,20)) // If move is valid
				{
					if(!foundMove)
					{
						foundMove=true;
					}

					numberOfRepeatings=0;	//reset counter

					int32_t score;

					nextPlayer=(turn%numberOfPlayers)+1;
					while(isDisqualified(nextPlayer))
					{
						nextPlayer=(nextPlayer%numberOfPlayers)+1;
					}

					score=minimax(mapCopy,nextPlayer ,depth-1,true); // Go deeper into the tree
					if(hasTimePassed)	// stop the search if no time is left.
					{
						return 0;	// Close every recursion stack and use the previous found move
					}

					if(turn==playerID)	// Maximizer node
					{
						if(depth==initialDepth)
						{
							updateBestMove(i, 20, &best);
						}
						else if (score>best)
						{
							best=score;
						}
					}
					else if(best<score)			// Minimizer node
					{
						best=score;
					}
				}

				mapCopy.copy(map);

				if(mapCopy.isPlayingPhaseMoveValid(i,turn,21)) // If move is valid
				{
					if(!foundMove)
					{
						foundMove=true;
					}

					numberOfRepeatings=0;	// Reset counter

					int64_t score;

					nextPlayer=(turn%numberOfPlayers)+1;
					while(isDisqualified(nextPlayer))
					{
						nextPlayer=(nextPlayer%numberOfPlayers)+1;
					}

					score=minimax(mapCopy,nextPlayer ,depth-1,true); // Go deeper into the tree
					if(hasTimePassed)	// stop the search if no time is left.
					{
						return 0;	// Close every recursion stack and use the previous found move
					}

					if(turn==playerID)	// Maximizer node
					{
						if(depth==initialDepth)
						{
							updateBestMove(i, 21, &best);
						}
						else if (score>best)
						{
							best=score;
						}
					}
					else if(best<score)			// Minimizer node
					{
						best=score;
					}
				}
			}
			else if(state=='c')					// If the cell is a choose stone,
			{									// Swap with every possible player.
				for(uint8_t p=1; p<=getAmountOfPlayers();p++)
				{
					mapCopy.copy(map);

					if(mapCopy.isPlayingPhaseMoveValid(i,turn,p)) // If move is valid
					{
						if(!foundMove)
						{
							foundMove=true;
						}

						numberOfRepeatings=0;	// Reset counter

						int32_t score;

						nextPlayer=(turn%numberOfPlayers)+1;
						while(isDisqualified(nextPlayer))
						{
							nextPlayer=(nextPlayer%numberOfPlayers)+1;
						}

						score=minimax(mapCopy,nextPlayer ,depth-1,true); // Go deeper into the tree

						if(hasTimePassed)	// Stop the search if no time is left
						{
							return 0;	// Close every recursion stack and use the previous found move
						}

						if(turn==playerID)	// Maximizer node
						{
							if(depth==initialDepth)
							{
								updateBestMove(i, 21, &best);
							}
							else if (score>best)
							{
								best=score;
							}
						}
						else if(best<score)			// Minimizer node
						{
							best=score;
						}
					}
				}
			}
			else	// In every other case just try playing a stone
			{
				mapCopy.copy(map);

				if(mapCopy.isPlayingPhaseMoveValid(i,turn,0)) // If move is valid
				{
					if(!foundMove)
					{
						foundMove=true;
					}

					numberOfRepeatings=0;	// Reset counter

					int32_t score;

					nextPlayer=(turn%numberOfPlayers)+1;
					while(isDisqualified(nextPlayer))
					{
						nextPlayer=(nextPlayer%numberOfPlayers)+1;
					}

					score=minimax(mapCopy,nextPlayer ,depth-1,true); // Go deeper into the tree
					if(hasTimePassed)	// Stop the search if no time is left.
					{
						return 0;	// Close every recursion stack and use the previous found move
					}

					if(turn==playerID)	// Maximizer node
					{
						if(depth==initialDepth)
						{
							updateBestMove(i, 21, &best);
						}
						else if (score>best)
						{
							best=score;
						}
					}
					else if(best<score)			// Minimizer node
					{
						best=score;
					}
				}
			}
		}
		else //-----BOMBING PHASE------
		{
			mapCopy.copy(map);

			if(mapCopy.isBombingPhaseMoveValid(i,turn,0)) // If move is valid
			{
				if(!foundMove)
				{
					foundMove=true;
				}

				numberOfRepeatings=0;	// Reset counter

				int score=0;

				nextPlayer=(turn%numberOfPlayers)+1;
				while(isDisqualified(nextPlayer))
				{
					nextPlayer=(nextPlayer%numberOfPlayers)+1;
				}

				score=minimax(mapCopy,nextPlayer ,depth-1,false); // Go deeper into the tree

				if(hasTimePassed)	// Stop the search if no time is left.
				{
					return 0;	// Close every recursion stack and use the previous found move
				}

				if(turn==playerID)	// Maximizer node
				{
					if(depth==initialDepth)
					{
						updateBestMove(i, 21, &best);
					}
					else if (score>best)
					{
						best=score;
					}
				}
				else if(best<score)			// Minimizer node
				{
					best=score;
				}
			}
		}
	}

	// If no move has been found execute it for the next player in the list, except if no player can make a move
	if(!foundMove && numberOfRepeatings<numberOfPlayers)
	{
		numberOfRepeatings++;

		nextPlayer=(turn%numberOfPlayers)+1;
		while(isDisqualified(nextPlayer))
		{
			nextPlayer=(nextPlayer%numberOfPlayers)+1;
		}

		return minimax(mapCopy, nextPlayer, depth-1, true); // Go deeper into the tree

	}
	else if(!foundMove)// If no player can make a move switch to bombing phase if
	{
		if(isPlayingPhase) // We are in the playing phase. Else just evaluate the board.
		{
			nextPlayer=(turn%numberOfPlayers)+1;
			while(isDisqualified(nextPlayer))
			{
				nextPlayer=(nextPlayer%numberOfPlayers)+1;
			}

			return minimax(mapCopy, nextPlayer ,depth, false);

		}
		else
		{
			return map.evaluateForEndOfGame(playerID);
		}
	}

	return best;
}

/**
 * Start the timer during benchmarking.
 */
void startTimer()
{
	clock_gettime(CLOCK_REALTIME, &spec);
	startTime = spec.tv_sec*1000000000 + (spec.tv_nsec);
}

/**
 * Stops the timer during benchmarking.
 */
void endTimer()
{
	clock_gettime(CLOCK_REALTIME, &spec);
	endTime = spec.tv_sec*1000000000 + (spec.tv_nsec);
	timeSpendEvaluating += endTime-startTime;				// only for evaluation
}

/**
 * Returns the number of analysed nodes if the BENCHMARK flag has been set during compilation
 *
 * @return The number of created nodes(incl. leafs) during the move search
 */
uint64_t getNumberOfAnalyzedNodes()
{
	uint64_t temp=nodesAnalyzed;
	nodesAnalyzed=0;
	return temp;
}

/**
 * Returns the time spend evaluating leafs if the BENCHMARK flag has been set during compilation
 *
 * @return The time in milliseconds spend evaluating game states.
 */
long getTimeSpendEvaluating()
{
	long temp=timeSpendEvaluating/1000000;
	timeSpendEvaluating=0;
	return temp;
}
