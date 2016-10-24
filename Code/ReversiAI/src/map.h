#ifndef MAP_H_
#define MAP_H_

#include <stdint.h>
#include <istream>
#include <vector>
#include <limits.h>

#define MAX_PLAYER 8

#define MAX_WIDTH 50
#define MAX_HEIGHT 50
#define DIRECTION_COUNT 8
#define LINE_COUNT 4
#define NO_CELL 0xFFFF
#define NO_DIRECTION 0xFF
#define NO_STONE CHAR_MAX
#define NO_PLAYER CHAR_MAX

using namespace std;

/**
 * Defines the static part of each Cell.
 */
typedef struct Cell
{
	uint16_t neighbour[DIRECTION_COUNT]; ///< Contains the indexes of every cell in each Direction. NO_STONE specifies that in that direction is no other cell
	uint8_t direction[DIRECTION_COUNT]; ///< Contains for each direction, in which direction this cell lies, from the other cell's perspective
	int influence;
	int baseStability[DIRECTION_COUNT]; ///< Indicates for every neighbour, on how many lines they have, in at least one direction, no neighbour
	int lineIndex[LINE_COUNT]; ///< Contains the information on which line this cell is in the following order: vertical, antidiagonal, horizontal, diagonal
} Cell;


/**
 * Contains the information which define a move.
 */
typedef struct Move
{
	uint16_t x;
	uint16_t y;
	uint8_t player;
	uint8_t choice;
} Move;

/**
 * This class contains the dynamic information of the board.
 *
 * As it will be copied a lot, the size of each variable is defined as small as possible.
 */
class Map
{
	public:
		Map(istream& is);
		Map();
		~Map();
		void copy(Map& toCopy);
		void draw();

		void initializeNeighbourList(uint8_t player);

		bool isPlayingPhaseMoveValid(uint16_t start, uint8_t player, uint8_t choice);
		bool isBombingPhaseMoveValid(uint16_t start, uint8_t player, uint8_t choice);
		int64_t evaluateForPlayingPhase(uint8_t player);
		int64_t evaluateForBombingPhase(uint8_t player);
		int64_t evaluateForEndOfGame(uint8_t player);

		uint8_t getState(uint16_t cell);
		int getAmountOfOverrideStones(uint8_t player);
		int getAmountOfBombs(uint8_t player);
		int getAmountOfInversionStones();

		int getScore(uint8_t playerID);

	private:
		uint16_t overrideStones[MAX_PLAYER+1];
		uint16_t numberOfBombs[MAX_PLAYER+1];
		uint8_t playerMap[MAX_PLAYER+1];
		uint16_t* amountOfFreeCellsOnLine;
		bool* isStoneStable;
		char* board;
		uint16_t amountOfInversionStones;
		uint16_t amountOfChoiceStones;

		bool isStoneReachable(uint16_t cell);

		int getStabilityRating(uint16_t cell);
		bool isStable(uint16_t cell);
		bool isFrontierStone(uint16_t cell);
		bool isMoveValid(uint16_t start, uint8_t player);

		uint8_t getPlayerStoneOwnership(uint8_t state);
		void adaptStableState();
		void resetStableState();
		std::vector<int> getMoveCaptures(uint16_t start, uint8_t player);
		void bombCell(uint16_t start, int depth);
		void addNeighbour(uint16_t x1, uint16_t y1, uint8_t dir1, uint16_t x2, uint16_t y2, uint8_t dir2);
};

void setConsiderOverrideStones(bool toConsider);
bool getConsiderOverrideStones();
uint8_t getNextPlayer(uint8_t lastPlayer);
void resetNextPlayerList(uint8_t player);
uint8_t getAmountOfConsideredPlayers();

uint16_t getWidth();
uint16_t getHeight();
uint16_t getOffset(uint16_t x, uint16_t y);
void reverseOffset(int* x, int* y, uint16_t  offset);

uint16_t getAmountOfCells();
uint8_t getAmountOfPlayers();
uint8_t getAmountOfActivePlayers();

void disqualifyPlayer(uint8_t player);
bool isDisqualified(uint8_t player);

void freeAllocatedMemory();

#endif
