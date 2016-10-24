/** 
 * The map class and the functions in this class are used to represent a board in a
 * specific game state. This can be copied into another instance to try out different moves. 
 * It can be checked whether a move is valid or not and execute it.
 * Furthermore the map class provides a rating function for the perspective of a given player.
 * In addition this file provides some useful and necessary information for other parts,
 * especially for the move searching algorithms.
 **/ 

#include <iostream>		// std::cout
#include <string.h>		// std::string
#include <sstream>		// std::istringstream
#include <math.h>		// std::pow
#include <algorithm>	// fill_n, std::sort
#include <cstring>		// memset
#include <set>
#include <list>
#include <queue>
#include <cstdint>

#include "map.h"
#include "weights.h"


using namespace std;

//TODO add opti data
//TODO adjust weights
//TODO profile

int freeCells;
int width;
int height;
int lineCount;
int cellcount;
int bombExplosionRadius;
int amountOfPlayers;
int amountOfActivePlayers;

vector<uint8_t> nextPlayers;

bool toConsiderOverrideStones=true;

/**
 * Contains the offset of the cells on the board (in class Map) and existentCell to the right cell.
 */
int offsetMap[MAX_WIDTH][MAX_HEIGHT];

/**
 * Contains the x coordinate of the cell that is at offset position in the array.
 */
int reverseOffsetX[MAX_WIDTH * MAX_HEIGHT];

/**
 * Contains the y coordinate of the cell that is at offset position in the array.
 */
int reverseOffsetY[MAX_WIDTH * MAX_HEIGHT];

/**
 * Contains a list of the players that got disqualified.
 */
bool disqualified[MAX_PLAYER+1];

/**
 * Contains all information regarding neighbours and their direction of the cells on the board.
 * Slots correlate with the board in the class Map.
 */
Cell* existentCell;

vector<int>* cellsOnLine;

////////////////////////////////////////
////			     				////
////  PUBLIC METHODES OF MAP CLASS  ////
////                    			////
////////////////////////////////////////

/**
 * Constructor for a map instance for reading a map from a text file or the server. 
 * This should always be used for the initial construction of a map. 
 * The default constructor should only be used if another map will
 * be copied into it via the copy() function.
 *
 * @param is - A reference to a byte stream which contains the information of a map
 */
Map::Map(istream& is)
{
	/** 
	* This char array will be used to read the numbers in as string and will then 
	* be parsed to a number. The size is chosen so that every number that could occur 
	* in a map definition can be parsed without a segmentation fault.
	*/
    const int bufferSize=5;
	char s_currentNumber[bufferSize];

	// Parse the basic settings for the map
	is.getline(s_currentNumber,bufferSize);
	istringstream (s_currentNumber) >> amountOfPlayers;
	amountOfActivePlayers = amountOfPlayers;

	is.getline(s_currentNumber,bufferSize);
	istringstream (s_currentNumber) >> overrideStones[1];

	is.getline(s_currentNumber,bufferSize,' ');
	istringstream (s_currentNumber) >> numberOfBombs[1];



	is.getline(s_currentNumber,bufferSize);
	istringstream (s_currentNumber) >> bombExplosionRadius;

	is.getline(s_currentNumber,bufferSize,' ');
	istringstream (s_currentNumber) >> height;

	is.getline(s_currentNumber,bufferSize);
	istringstream (s_currentNumber) >> width;

	// Initialize the override stone and bomb count for every player.
	// At the same time initialize the player map and the disqualification array
	disqualified[0] = true;
	playerMap[0] = 0;	// Zero will always map to itself

	for(int p=1; p<=MAX_PLAYER; ++p)
	{
		if(p <= getAmountOfPlayers())
		{
			overrideStones[p] = overrideStones[1];
			numberOfBombs[p] = numberOfBombs[1];
			playerMap[p] = p;
			disqualified[p] = false;
		}
		else
		{
			playerMap[p] = NO_PLAYER; // Mark player as 'not a player'
			disqualified[p] = true;
		}
	}

	/**
	* We only want to keep the cells in our array and discard all the holes.
	* To avoid having to reopen the map after counting the amount of cells,
	* we save everything into an temporary array. As we do not need to know 
	* the amount of cells to know the offsets, we initialize the array right away.
	*/
	char boardstate[getWidth()][getHeight()];

	cellcount=0; // Save the number of cells needed
	amountOfInversionStones=0;
	amountOfChoiceStones=0;
	char c=0;

	for(int y = 0; y < getHeight(); ++y)
	{
		for(int x = 0; x < getWidth(); ++x)
		{
			c = is.get();

			if(c != '-')
			{
				offsetMap[x][y] = cellcount;
				reverseOffsetX[cellcount] = x;
				reverseOffsetY[cellcount] = y;
				cellcount++;
				if('0' <= c && c <= '0' + MAX_PLAYER)
				{	// We store the information of the players as the number and not Unicode,
					// to avoid transforming between indexes and representation during the computations
					boardstate[x][y] = c - '0';
				}
				else
				{	// Mark special cells
					boardstate[x][y] =  c;
				}
			}
			else
			{
				offsetMap[x][y] = NO_CELL;
			}

			c = is.get(); // Ignore space
		}
		if(c!='\n')
		{
			is.get(); // Ignore line break
		}
	}

	// Create board and transition map (existentCell), now that the size is known
	board = new char[getAmountOfCells()];
	existentCell = new Cell[getAmountOfCells()];
	isStoneStable = new bool[LINE_COUNT*getAmountOfCells()];

	// Now we get to the fun part! You should grab a coffee.. two might be better ;)

	// Initialize every neighbour to be non existent
	for(int cell=0; cell<getAmountOfCells(); ++cell)
	{
		for(uint8_t dir=0; dir<DIRECTION_COUNT; ++dir)
		{
			existentCell[cell].direction[dir] = NO_DIRECTION;
			existentCell[cell].neighbour[dir] = NO_CELL;
		}
	}

	// Copy the states of the cells into our array that has minimal size
	for(uint32_t y=0; y<getHeight(); ++y)
	{
		for(uint32_t x=0; x<getWidth(); ++x)
		{
			if(offsetMap[x][y] != NO_CELL)
			{
				board[getOffset(x,y)] = boardstate[x][y];
			}
		}
	}

	// Add all trivial neighbours that are given by direct neighbourhood
	for(uint32_t x = 0; x < getWidth(); x++)
	{
		for(uint32_t y = 0; y < getHeight(); y++)
		{
			if(offsetMap[x][y] != NO_CELL)
			{
				if(y > 0 && offsetMap[x][y-1] != NO_CELL)
				{
					addNeighbour(x, y, 0, x, y-1, 4);
				}
				if(x+1 < getWidth() && y > 0 && offsetMap[x+1][y-1] != NO_CELL)
				{
					addNeighbour(x, y, 1, x+1, y-1, 5);
				}
				if(x+1 < getWidth() && offsetMap[x+1][y] != NO_CELL)
				{
					addNeighbour(x, y, 2, x+1, y, 6);
				}
				if(x+1 < getWidth() && y+1 < getHeight() && offsetMap[x+1][y+1] != NO_CELL)
				{
					addNeighbour(x, y, 3, x+1, y+1, 7);
				}
				if(y+1 < getHeight() && offsetMap[x][y+1] != NO_CELL)
				{
					addNeighbour(x, y, 4, x, y+1, 0);
				}
				if(x > 0 && y+1 < getHeight() && offsetMap[x-1][y+1] != NO_CELL)
				{
					addNeighbour(x, y, 5, x-1, y+1, 1);
				}
				if(x > 0 && offsetMap[x-1][y] != NO_CELL)
				{
					addNeighbour(x, y, 6, x-1, y, 2);
				}
				if(x > 0 && y > 0 && offsetMap[x-1][y-1] != NO_CELL)
				{
					addNeighbour(x, y, 7, x-1, y-1, 3);
				}
			}
		}
	}

	/**
	* To make it easier to determine which stones are stable later on, we need to know 
	* which stone is on what line in each direction.
	* Explanation of the algorithm:
	* We overlay the board with horizontal, vertical, anti-diagonal and diagonal lines. 
	* Whenever a hole is in the way the line gets separated into two. The lines get labeled from 1 to linecount the following way: 
	* First come all the horizontal lines, then the vertical ones, then anti-diagonal ones and finally the diagonal ones. 
	* If a line has been broken into more segments, they will be ordered from left-top to right-bottom.
	* Later we will merge the lines that get through non-trivial neighbours.
	* The lineIndex are ordered in a way that (direction%4)=index (vertical, anti-diagonal, horizontal, diagonal).
	*/

	// Set the line index for the horizontal lines
	int index=-1;
	bool nextIsNew=true;
	for(int y=0; y<getHeight(); ++y)
	{
		for(int x=0; x<getWidth(); ++x)
		{
			int offset = offsetMap[x][y];

			if(offset == NO_CELL)
			{
				nextIsNew=true;
			}
			else
			{
				if(nextIsNew)
				{
					index++;
					nextIsNew=false;
				}

				existentCell[offset].lineIndex[2]=index;
			}
		}
		nextIsNew=true;
	}

	// Set the line index for the vertical lines
	nextIsNew=true;
	for(int x=0; x<getWidth(); ++x)
	{
		for(int y=0; y<getWidth(); ++y)
		{
			int offset = offsetMap[x][y];

			if(offset == NO_CELL)
			{
				nextIsNew=true;
			}
			else
			{
				if(nextIsNew)
				{
					index++;
					nextIsNew=false;
				}

				existentCell[offset].lineIndex[0]=index;
			}
		}
		nextIsNew=true;
	}

	// Set the line index for the anti-diagonals
	nextIsNew=true;
	for(int i=0; i<getWidth(); ++i)
	{
		int x=i;
		int y=0;

		while((x>=0) && (y<getHeight()))
		{
			int offset = offsetMap[x][y];

			if(offset == NO_CELL)
			{
				nextIsNew=true;
			}
			else
			{
				if(nextIsNew)
				{
					index++;
					nextIsNew=false;
				}

				existentCell[offset].lineIndex[1]=index;
			}

			x--;
			y++;
		}

		nextIsNew=true;
	}

	nextIsNew=true;
	for(int i=1; i<getHeight(); ++i)
	{
		int x=getWidth()-1;
		int y=i;

		while((y<getHeight()) && (x>=0))
		{
			int offset = offsetMap[x][y];

			if(offset == NO_CELL)
			{
				nextIsNew=true;
			}
			else
			{
				if(nextIsNew)
				{
					index++;
					nextIsNew=false;
				}

				existentCell[offset].lineIndex[1]=index;
			}

			x--;
			y++;
		}

		nextIsNew=true;
	}

	// Set the line index for the diagonals
	nextIsNew=true;
	for(int i=getHeight(); i>=0; --i)
	{
		int x=0;
		int y=i;

		while((x<getWidth()) && (y<getHeight()))
		{
			int offset = offsetMap[x][y];

			if(offset == NO_CELL)
			{
				nextIsNew=true;
			}
			else
			{
				if(nextIsNew)
				{
					index++;
					nextIsNew=false;
				}

				existentCell[offset].lineIndex[3]=index;
			}

			x++;
			y++;
		}

		nextIsNew=true;
	}

	nextIsNew=true;
	for(int i=1; i<getWidth(); ++i)
	{
		int x=i;
		int y=0;

		while((y<getHeight()) && (x<getWidth()))
		{
			int offset = offsetMap[x][y];

			if(offset == NO_CELL)
			{
				nextIsNew=true;
			}
			else
			{
				if(nextIsNew)
				{
					index++;
					nextIsNew=false;
				}

				existentCell[offset].lineIndex[3]=index;
			}

			x++;
			y++;
		}

		nextIsNew=true;
	}

	// Add all non-trivial neighbours given by <->-transitions
	uint16_t x1, y1, dir1, x2, y2, dir2;

	// Every element of toMerge is a list of line indices that have to be merged
	vector<vector<int> > toMerge;

	while(is.good())
	{
		is.getline(s_currentNumber,bufferSize,' ');
		if(is.eof()) // Fix for: file has an empty line at the end
		{
			break;
		}
		istringstream (s_currentNumber) >> x1;

		is.getline(s_currentNumber,bufferSize,' ');
		istringstream (s_currentNumber) >> y1;

		is.getline(s_currentNumber,bufferSize,' ');
		istringstream (s_currentNumber) >> dir1;

		is.getline(s_currentNumber,bufferSize,' '); // discard <->

		is.getline(s_currentNumber,bufferSize,' ');
		istringstream (s_currentNumber) >> x2;

		is.getline(s_currentNumber,bufferSize,' ');
		istringstream (s_currentNumber) >> y2;

		is.getline(s_currentNumber,bufferSize);
		istringstream (s_currentNumber) >> dir2;

		if(dir2!=-1)
		{
			addNeighbour(x1, y1, (uint8_t) dir1, x2, y2, (uint8_t) dir2);
		}

		// Check if the new lines have to merged with other ones or only with themselves for the moment
		int first = existentCell[getOffset(x1,y1)].lineIndex[dir1%LINE_COUNT];
		int second = existentCell[getOffset(x2,y2)].lineIndex[dir2%LINE_COUNT];
		bool hasAdded=false;
		for(vector<vector<int> >::iterator it=toMerge.begin(); it!=toMerge.end(); ++it) {
			// If the first element is already in a list, add the second as well
			if(it->end() != find(it->begin(), it->end(), first))
			{
				//If the second element is not yet in the list, add it
				if(find(it->begin(), it->end(), second) == it->end())
				{
					it->push_back(second);
				}
				hasAdded=true;
				break;
			}
			// Same for the second already being in a list
			if(it->end() != find(it->begin(), it->end(), second))
			{
				// The first element is not in the list as checked before, so add it
				it->push_back(first);

				hasAdded=true;
				break;
			}
		}

		// If the elements have not been added to a list, create a new one and add it to toMerge
		if(!hasAdded)
		{
			vector<int> temp;
			temp.push_back(first);
			if(first!=second)
			{
				temp.push_back(second);
			}
			toMerge.push_back(temp);
		}
	}

	// Every line index has to be changed to the smallest one in its toMerge list, if it is in any
	// We keep a list of all of those lines that do not get used afterwards
	vector<int> toDelete;
	bool isFirstSet;
	int first;

	for(vector<vector<int> >::iterator vecIt=toMerge.begin(); vecIt!=toMerge.end(); ++vecIt) {
		isFirstSet=false;
		sort(vecIt->begin(),vecIt->end());
		for(vector<int>::iterator listIt = vecIt->begin(); listIt!=vecIt->end(); ++listIt) {
			if(isFirstSet)
			{
				int line=*listIt;
				// Iterate over all elements on line
				for(int cell=0; cell<getAmountOfCells(); ++cell)
				{
					for(int d=0; d<4; ++d)
					{
						// Change the values to the line it was merged to
						if(existentCell[cell].lineIndex[d]==line)
						{
							existentCell[cell].lineIndex[d]=first;
						}
					}
				}
				// Add line to the lines that will be deleted at the end
				toDelete.push_back(line);
			}
			else
			{
				first=*listIt;
				isFirstSet=true;
			}
		}
	}

	sort(toDelete.begin(),toDelete.end());
	toDelete.erase(unique(toDelete.begin(), toDelete.end()), toDelete.end());

	// Now we know exactly how many unique lines are on the board, and can create the final arrays
	lineCount=(index+1)-toDelete.size();

	cellsOnLine = new vector<int>[lineCount];
	amountOfFreeCellsOnLine = new uint16_t[lineCount];
	memset(amountOfFreeCellsOnLine,0,lineCount*sizeof(uint16_t));

	// Correct the offset errors created by deleting unused lines
	for(int del=toDelete.size()-1; 0<=del; --del)
	{
		for(int cell=0; cell<getAmountOfCells(); ++cell)
		{
			for(int dir=0; dir<LINE_COUNT; ++dir)
			{
				if(existentCell[cell].lineIndex[dir]>toDelete[del])
				{
					existentCell[cell].lineIndex[dir]--;
				}
			}
		}
	}

	// As the line indices are now correct for every cell, we can use that information
	// to initialize the counters and list of cells for every line
	for(int cell=0; cell<getAmountOfCells(); ++cell)
	{
		set<int> index;
		for(int dir=0; dir<4; ++dir)
		{
			index.insert(existentCell[cell].lineIndex[dir]);
		}

		// Use set to avoid duplicates
		for(set<int>::iterator it=index.begin(); it!=index.end(); ++it)
		{
			cellsOnLine[*it].push_back(cell);
			if(board[cell]<1 || (board[cell]>MAX_PLAYER && board[cell]!='x') )
			{
				amountOfFreeCellsOnLine[*it]++;
			}
		}
	}

	// Set the base stability for every cell (=stability given by holes as direct neighbours)
	// This is used to avoid placing a stone near to corners or walls
	int stability[getAmountOfCells()];
	memset(stability, 0, getAmountOfCells()*sizeof(int));

	for(int cell=0; cell<getAmountOfCells(); ++cell)
	{
		for(int dir=0; dir<4; ++dir)
		{
			if((existentCell[cell].neighbour[dir]==NO_CELL) || (existentCell[cell].neighbour[dir+4]==NO_CELL))
			{
				stability[cell]++;
				isStoneStable[4*cell+dir]=true;
			}
			else if(amountOfFreeCellsOnLine[existentCell[cell].lineIndex[dir]]==0)
			{
				isStoneStable[4*cell+dir]=true;
			}
			else
			{
				isStoneStable[4*cell+dir]=false;
			}
		}
	}

	for(int cell=0; cell<getAmountOfCells(); ++cell)
	{
		for(int dir=0; dir<8; ++dir)
		{
			if(existentCell[cell].neighbour[dir]!=NO_CELL)
			{
				existentCell[cell].baseStability[dir]=stability[existentCell[cell].neighbour[dir]];
			}
			else
			{
				existentCell[cell].baseStability[dir]=0;
			}
		}
	}

	//Set the amount of likely reachable inversion and choice stones
	for(uint16_t cell=0; cell<getAmountOfCells(); ++cell)
	{
		if(getState(cell)=='i')
		{
			if(isStoneReachable(cell))
			{
				amountOfInversionStones++;
			}
		}
		else if(getState(cell)=='c')
		{
			if(isStoneReachable(cell))
			{
				amountOfChoiceStones++;
			}
		}
	}

	cout << "Amount of reachable inversion stones: "<< amountOfInversionStones<<endl;
	cout << "Amount of reachable choice stones: "<< amountOfChoiceStones<<endl;


	//Set the influence of every cell to the amount of stones that are on the same lines
	int totalInfluence=0;
	for(uint16_t cell=0; cell<getAmountOfCells(); ++cell)
	{
		int sum=0;
		for(int d=0; d<LINE_COUNT; ++d)
		{
			sum+=cellsOnLine[existentCell[cell].lineIndex[d]].size();
		}

		totalInfluence+=sum-3;
		existentCell[cell].influence=sum-3;
	}

	WEIGHT_OverrideStones = (totalInfluence/getAmountOfCells()) * 200;

	int totalNeigbours=0;
	for(uint16_t cell=0; cell<getAmountOfCells(); ++cell)
	{
		for(int d=0; d<DIRECTION_COUNT; ++d)
		{
			if(existentCell[cell].neighbour[d]!=NO_CELL)
			{
				totalNeigbours++;
			}
		}
	}

	WEIGHT_Bombs = (int)(pow(((double)totalNeigbours/getAmountOfCells()), bombExplosionRadius) * 200);

	adaptStableState();
}

/**
 * Create an empty map. All the array are initialized to the correct sizes for the current game. 
 * Copying a specific state into them has to be done with the Map::copy() method.
 */
Map::Map()
{
	amountOfInversionStones=0;
	amountOfChoiceStones=0;
	board = new char[getAmountOfCells()];
	amountOfFreeCellsOnLine = new uint16_t[lineCount];
	isStoneStable = new bool[getAmountOfCells()*LINE_COUNT];
}

/**
 * Destructor for a map instance. Free all the memory that is needed for
 * an instance of map, which stores the dynamic parts of a map.
 */
Map::~Map()
{
	delete [] board;
	delete [] amountOfFreeCellsOnLine;
	delete [] isStoneStable;
}

/**
 * Copies the state of the map passed as argument to this instance of Map.
 *
 * @param toCopy - The map that shall be copied
 */
void Map::copy(Map& toCopy)
{
	memcpy(board, toCopy.board, getAmountOfCells()*sizeof(char));

	memcpy(overrideStones, toCopy.overrideStones, sizeof(uint16_t)*(MAX_PLAYER+1));
	memcpy(numberOfBombs, toCopy.numberOfBombs, sizeof(uint16_t)*(MAX_PLAYER+1));
	memcpy(playerMap, toCopy.playerMap, MAX_PLAYER+1);

	amountOfInversionStones = toCopy.amountOfInversionStones;
	amountOfChoiceStones = toCopy.amountOfChoiceStones;

	memcpy(amountOfFreeCellsOnLine, toCopy.amountOfFreeCellsOnLine, lineCount*sizeof(uint16_t));

	memcpy(isStoneStable, toCopy.isStoneStable, getAmountOfCells()*sizeof(bool)*4);
}

/**
 * Draw the map to the standard output stream.
 */
void Map::draw()
{
	for(int y=0; y<getHeight(); ++y)
	{
		for(int x=0; x<getWidth(); ++x)
		{
			int index = getOffset(x, y);

			if(index == NO_CELL || board[index]==NO_STONE)
			{
				cout << "- ";
			}
			else if((0 <= board[index]) && (board[index] <= MAX_PLAYER))
			{
				cout << (int)getPlayerStoneOwnership(board[index]);
				if(isStable(index))
				{
					cout << "\'";
				}
				else
				{
					cout <<" ";
				}
			}
			else
			{
				cout << board[index] << " ";
			}
		}
		cout << endl;
	}
}

/**
 * Initialize the vector of neighbours.
 *
 * @param player - The player whose neighbours shall be determined (0 to set the list to all players)
 */
void Map::initializeNeighbourList(uint8_t player)
{

	set<uint8_t> neighbouringPlayers;
	set<uint16_t> neighbouringCells;

	// Use player=0 to indicate that we are in the bombing phase and all players should be considered
	if(player==0)
	{
		for(uint8_t p=1; p<=getAmountOfPlayers(); ++p)
		{
			neighbouringPlayers.insert(p);
		}
		return;
	}

	// Add all the cell of player to the region we consider
	for(uint16_t cell=0; cell<getAmountOfCells(); ++cell)
	{
		if(getPlayerStoneOwnership(getState(cell))== player)
		{
			neighbouringCells.insert(cell);
		}
	}

	bool reiterate=true;
	// While new stones get added, reiterate
	while(reiterate)
	{
		reiterate=false;

		for(uint16_t cell=0; cell<getAmountOfCells(); ++cell)
		{
			// If the cell is in the region
			if(neighbouringCells.find(cell)!=neighbouringCells.end())
			{
				if(getState(cell)!=0 && getState(cell)!='i' && getState(cell)!='c')
				{
					// Look at every neighbour
					for(int dir=0; dir<DIRECTION_COUNT; ++dir)
					{
						int neighbour=existentCell[cell].neighbour[dir];
						// If the neighbour is joining stone, add it to the list
						if((neighbour!=NO_CELL) && neighbouringCells.find(neighbour)==neighbouringCells.end())
						{
							neighbouringCells.insert(neighbour);
							reiterate=true;
						}
					}
				}
				else
				{
					// Look at every neighbour
					for(int dir=0; dir<DIRECTION_COUNT; ++dir)
					{
						int neighbour=existentCell[cell].neighbour[dir];
						// Add it only if it is a player or expansion stone
						if((neighbour!=NO_CELL) && (neighbouringCells.find(neighbour)==neighbouringCells.end())
								&& (getState(neighbour)=='x' || (getState(neighbour)!=0 && getState(neighbour)<=MAX_PLAYER)))
						{
							neighbouringCells.insert(neighbour);
							reiterate=true;
						}
					}
				}
			}
		}
	}

	neighbouringPlayers.insert(player);
	for(set<uint16_t>::iterator it=neighbouringCells.begin(); it!=neighbouringCells.end(); ++it)
	{
		if((getState(*it)!=0) && (getState(*it)<=MAX_PLAYER) && !isDisqualified(getPlayerStoneOwnership(getState(*it))))
		{
			neighbouringPlayers.insert(getPlayerStoneOwnership(getState(*it)));
		}
	}

	nextPlayers.clear();
	nextPlayers.push_back(player);

	for(set<uint8_t>::iterator it=neighbouringPlayers.begin(); it!=neighbouringPlayers.end(); ++it)
	{
		if(*it>player)
		{
			nextPlayers.push_back(*it);
		}
	}

	for(set<uint8_t>::iterator it=neighbouringPlayers.begin(); it!=neighbouringPlayers.end(); ++it)
	{
		if(*it<player)
		{
			nextPlayers.push_back(*it);
		}
	}
}

/**
 * Tries placing a stone at the cell at coordinates (x,y) for the given player.
 * If the move is valid, the stones get recoloured, and the map drawn to the console.
 *
 * @param start  - The index of the Cell on which a stone should be played
 * @param player - The player who makes the move
 * @param choice - Contains information for choice or bonus stones: 
						 0	represents no information 
	 1 to MAX_PLAYER_COUNT	represents the player that is target of a choice cell
						20	represents the choice for a bomb of a bonus cell
						21	represents the choice for an override stone of a bonus cell
 * @return	True if the move was valid and could be executed, false otherwise
 */
bool Map::isPlayingPhaseMoveValid(uint16_t start, uint8_t player, uint8_t choice)
{
	// Check if cell exists
	if(start == NO_CELL)
	{
		return false;
	}

	uint8_t state=board[start];

	// Check if player has enough override stones (if necessary for the move)
	if((overrideStones[player]==0 || !toConsiderOverrideStones) && ((state>0 && state<=MAX_PLAYER)|| state=='x'))
	{
		return false;
	}

	// Get the cells that are to be recoloured by the move
	vector<int> recolourList = getMoveCaptures(start, player);

	uint8_t curPlayer = playerMap[player];

	// If there are stones that get turned, the move is valid and it can be executed
	if(recolourList.size()>0)
	{
		if(state=='c')
		{
			amountOfChoiceStones--;
			if((choice==0) || (choice>getAmountOfPlayers()))
			{
				return false;
			}
			int helper = playerMap[player];
			playerMap[player] = playerMap[choice];
			playerMap[choice] = helper;
			resetStableState();
		}
		else if(state=='b')
		{
			if(choice == 20)
			{	// 20 represents the choice for a bomb
				numberOfBombs[player]++;
			}
			else if(choice == 21)
			{	// 21 represents the choice for an override stone
				overrideStones[player]++;
			}
		}
		else if(state=='i')
		{
			amountOfInversionStones--;
			// Switch each player with his corresponding target
			int temp = playerMap[getAmountOfPlayers()];
			for(int i=getAmountOfPlayers(); i>0; i--)
			{
				playerMap[i]=playerMap[i-1];
			}
			playerMap[1]=temp;
			resetStableState();
		}
		else if(state==0)
		{
			// Do nothing here
		}
		else
		{
			overrideStones[player]--;
			resetStableState();
		}

		if(state==0 || (state>MAX_PLAYER && state!='x'))
		{
			int previous[4]={-1,-1,-1,-1};

			for(int dir=0; dir<LINE_COUNT; ++dir)
			{
				int line=existentCell[start].lineIndex[dir];

				if(line!=previous[0] && line!=previous[1] && line!=previous[2] &&(--amountOfFreeCellsOnLine[line])==0)
				{
					for (vector<int>::iterator it = cellsOnLine[line].begin(); it != cellsOnLine[line].end(); ++it)
					{
						for(int i=0; i<LINE_COUNT; ++i)
						{
							if(existentCell[*it].lineIndex[i]==line)
							{
								isStoneStable[4*(*it)+i]=true;
							}
						}
					}
				}
				previous[dir]=line;
			}
		}

		board[start] = curPlayer;

		// Recolour all the cells in the recolouring list, draw map and return success
		for(unsigned int i=0; i<recolourList.size(); ++i)
		{
			board[recolourList[i]] = curPlayer;
		}

		adaptStableState();

		return true;
	}
	else if(board[start] == 'x') // Expansion rule
	{	// Recolour the starting cell, draw the new map and return success
		board[start] = curPlayer;
		overrideStones[player]--;

		adaptStableState();

		return true;
	}

	// Return false if no stones could be flipped by the move
	return false;
}

/**
 * Tries to place a bomb at the cell at (x,y) for the given player.
 * If the move is valid, the player loses one bomb and the bomb is thrown on that cell.
 *
 * @param start  - The index of the cell on the board on which the bomb is supposed to be dropped
 * @param player - The player who makes the move
 * @return	Returns true if the move is valid and could be executed, false otherwise
 */
bool Map::isBombingPhaseMoveValid(uint16_t start, uint8_t player, uint8_t choice)
{
	// Check if player has enough bombs, or if the cell has already been destroyed
	if(start==NO_CELL || board[start]==NO_STONE || numberOfBombs[player]==0)
	{
		return false;
	}

	// Delete the cells that get hit by the bomb
	bombCell(start, bombExplosionRadius);
	for(int cell=0; cell<getAmountOfCells(); ++cell)
	{
		if(board[cell] >= (NO_STONE-bombExplosionRadius))
		{
			board[cell]=NO_STONE;
		}
	}
	numberOfBombs[player]--;

	return true;
}

/**
 * Associate a score to the current board state from the perspective of the specified player. 
 * The heuristic generates a higher score the more promising a position looks.
 *
 * @param player - The player from whose perspective the board is evaluated
 * @return	The generated score
 */
int64_t Map::evaluateForPlayingPhase(uint8_t player)
{
	uint8_t expectedPlayerOffset = getAmountOfInversionStones();

	int64_t rating[getAmountOfPlayers()];

	for(int p=0; p<getAmountOfPlayers();p++)
	{
		rating[p]=overrideStones[p+1]*WEIGHT_OverrideStones;
		rating[p]+=numberOfBombs[p+1]*WEIGHT_Bombs;
	}

	//TODO implement maybe in search tree(be only the second best!)
	//rating+=amountOfChoiceStones*WEIGHT_ChoiceStone;

	char state;

	for(int16_t cell=0; cell<getAmountOfCells(); ++cell)
	{
		state=board[cell];

		if(state!=0 && state<=getAmountOfPlayers())
		{
			rating[(getPlayerStoneOwnership(state)+expectedPlayerOffset-1)%getAmountOfPlayers()]+=getStabilityRating(cell);
			if(isFrontierStone(cell))
			{
				rating[(getPlayerStoneOwnership(state)+expectedPlayerOffset-1)%getAmountOfPlayers()]+=WEIGHT_Frontier*existentCell[cell].influence;
			}
		}
	}

	int64_t ourRating = rating[(player+expectedPlayerOffset-1)%getAmountOfPlayers()];

	int betterPlayers=0;
	int64_t total=0;
	for(int p=0; p<getAmountOfPlayers();p++)
	{
		if(p != (player+expectedPlayerOffset-1)%getAmountOfPlayers())
		{
			total+=rating[p];
			if(rating[p]>ourRating)
			{
				betterPlayers++;
			}
		}
	}

	if(amountOfChoiceStones>0)
	{
		if(betterPlayers==0)
			betterPlayers=(getAmountOfPlayers()/2);
	}

	return ((getAmountOfPlayers()*ourRating-total)-betterPlayers*10000000);
}

/**
 * Associate a score to the current board state from the perspective of the specified player. 
 * The heuristic generates a higher score the more promising a position looks.
 *
 * @param player - The player from whose perspective the board is evaluated
 * @return	The generated score
 */
int64_t Map::evaluateForBombingPhase(uint8_t player)
{
	// Set all values to 0
	int numberOfStones[getAmountOfPlayers()];
	memset(numberOfStones, 0, getAmountOfPlayers()*sizeof(int));

	// Count the amount of cells owned for each player
	unsigned char cell;
	for(int i = 0; i < getAmountOfCells(); i++)
	{
		cell=board[i];
		if(cell!=NO_STONE && cell>=1 && cell<=getAmountOfPlayers())
		{
			numberOfStones[cell-1]++;
		}
	}

	int stonesOwnedByUs = numberOfStones[playerMap[player]-1];

	int stonesOfPlayersAboveUs = 0;
	int stonesOfOtherPlayers = 0;

	for(int i=0; i<getAmountOfPlayers(); ++i)
	{
		if(!isDisqualified(i) && numberOfStones[i]>=stonesOwnedByUs)
		{
			stonesOfPlayersAboveUs += numberOfStones[i];
		}
		else if(!isDisqualified(i) && i!=playerMap[player]-1)
		{
			stonesOfOtherPlayers += numberOfStones[i];
		}
	}

	if(stonesOfPlayersAboveUs>0)
	{
		return (stonesOwnedByUs*(getAmountOfPlayers()-1))-stonesOfPlayersAboveUs;
	}
	else
	{
		return (MAX_WIDTH*MAX_HEIGHT)*getAmountOfPlayers()+(stonesOwnedByUs*(getAmountOfPlayers()-1))-stonesOfOtherPlayers;
	}

}

/**
 * Evaluates the board for the end of the game(=noone can make any move).
 * Returns the maximum value if the player has won and minimum+1 if he has not.
 *
 * @param player - The player from whose perspective the board is evaluated
 * @return	The generated score
 */
int64_t Map::evaluateForEndOfGame(uint8_t player)
{
	int stoneCount[MAX_PLAYER+1];
	memset(stoneCount, 0, sizeof(int)*(MAX_PLAYER+1));

	for(int cell=0; cell<getAmountOfCells(); ++cell)
	{
		if((board[cell]!=0) && (board[cell]<MAX_PLAYER))
		{
			stoneCount[playerMap[(uint8_t)board[cell]]]++;
		}

	}

	int amountOfPlayersWithMoreStones=0;
	int amountOfPlayersWithEqualStones=0;
	for(int p=1; p<=getAmountOfPlayers(); ++p)
	{
		if(stoneCount[p]>stoneCount[player])
		{
			amountOfPlayersWithMoreStones++;
		}
		else if(stoneCount[p]==stoneCount[player])
		{
			amountOfPlayersWithEqualStones++;
		}
	}

	if(amountOfPlayersWithMoreStones==0)
	{
		return INT_MAX-amountOfPlayersWithEqualStones;
	}
	else
	{
		return INT_MIN+(MAX_PLAYER-amountOfPlayersWithMoreStones);
	}

}

/**
 * Getter function for the state of a cell.
 *
 * @param cell - The index of the cell for which the state shall be returned
 * @return The state of the cell
 */
uint8_t Map::getState(uint16_t cell)
{
	return board[cell];
}

/**
 * Getter function to get the amount of override stones for a specific player.
 *
 * @param player - The player for whom the amount of override stones are returned
 * @return	The amount of override stones left for the specified player
 */
int Map::getAmountOfOverrideStones(uint8_t player)
{
	return overrideStones[player];
}

/**
 * Getter function to get the amount of bombs for a specific player.
 *
 * @param player - The player for whom the amount of bombs are returned
 * @return	The amount of bombs left for the specified player
 */
int Map::getAmountOfBombs(uint8_t player)
{
	return numberOfBombs[player];
}

/**
 * Getter function for the amount of inversion stones, 
 * that are expected to be reached until the end of the game.
 */
int Map::getAmountOfInversionStones()
{
	return amountOfInversionStones;
}

/**
 * @return The score we would have gotten in the tournament
 */
int Map::getScore(uint8_t playerID){
	int stoneCount[MAX_PLAYER+1];
	memset(stoneCount, 0, sizeof(int)*MAX_PLAYER);

	for(int i=0; i<getAmountOfCells(); i++)
	{
		if((board[i]!=0) && (board[i]<=MAX_PLAYER))
		{
			stoneCount[getPlayerStoneOwnership(board[i])]++;
		}

	}

	int res=0;
	for(int i=1; i<=getAmountOfPlayers(); i++)
	{
		if(stoneCount[i]>stoneCount[playerID])
		{
			res++;
		}
	}

	if(res==0)
	{
		return 25;
	}
	else if(res==1)
	{
		return 11;
	}
	else if(res==2)
	{
		return 5;
	}
	else if(res==3)
	{
		return 2;
	}
	else if(res==4)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/////////////////////////////////////////
////			     				 ////
////  PRIVATE METHODES OF MAP CLASS  ////
////                    			 ////
/////////////////////////////////////////

/**
 * Checks wether a certain stone is likely to be reached during a game or not.
 *
 * @param cell - The cell that should be analyzed for reachability
 * @return True if the cell is likely to be reached, false otherwise.
 */
bool Map::isStoneReachable(uint16_t cell)
{
	bool hasLineWithMoreThanThreeElements=false;
	for(int d=0; d<4; ++d)
	{
		if(cellsOnLine[existentCell[cell].lineIndex[d]].size()>3)
		{
			hasLineWithMoreThanThreeElements=true;
			break;
		}
	}
	if(!hasLineWithMoreThanThreeElements)
	{
		return false;
	}

	bool gotVisited[getAmountOfCells()];
	memset(gotVisited, false, getAmountOfCells()*sizeof(bool));

	queue<uint16_t> toExamine;

	toExamine.push(cell);

	uint16_t next;

	while(!toExamine.empty())
	{
		next=toExamine.front();
		toExamine.pop();

		if(next!= NO_CELL && !gotVisited[next])
		{
			gotVisited[next]=true;

			for(int dir=0; dir<DIRECTION_COUNT; ++dir)
			{
				toExamine.push(existentCell[next].neighbour[dir]);
			}

			char state=getState(next);
			if(state!=0 && state<=MAX_PLAYER)
			{
				int count=0;
				for(int dir=0; dir<DIRECTION_COUNT; ++dir)
				{
					char neigbourState = getState(existentCell[next].neighbour[dir]);
					if(neigbourState!=0 && (neigbourState<= MAX_PLAYER || neigbourState=='x') && neigbourState!=state)
					{
						count++;
					}
				}
				if(count>1)
				{
					return true;
				}
			}
		}
	}

	return false;
}

/**
 * Associates a score to the stability of the stone placed on the cell.
 *
 * @param cell - The cell, on which the stone is placed for which the stability should be determined
 * @return A score representing the stability of the stone on the cell
 */
int Map::getStabilityRating(uint16_t cell){

	int counter=0;
	for(int l=0; l<LINE_COUNT; ++l){
		if(isStoneStable[LINE_COUNT*cell+l])
		{
			counter++;
		}
	}

	int score;
	if(counter==0)
	{
		score=15*existentCell[cell].influence;
	}
	else if(counter==1)
	{
		score=25*existentCell[cell].influence;//*=WEIGHT_stabilityRise;
	}
	else if(counter==2)
	{
		score=60*existentCell[cell].influence;//*=WEIGHT_stabilityRise;
	}
	else if(counter==3)
	{
		score=120*existentCell[cell].influence;//*=WEIGHT_stabilityRise;
	}
	else
	{
		score=400*existentCell[cell].influence;//*=WEIGHT_stabilityRise;
	}

	int max=0;
	for(int dir=0; dir<DIRECTION_COUNT; ++dir){
		int neighbour = existentCell[cell].neighbour[dir];
		if(neighbour!=NO_CELL)
		{
			if(board[neighbour]=='b'){
				return -400*WEIGHT_OverrideStones;
			}
			if(!(board[neighbour]>0 && board[neighbour]<=MAX_PLAYER) && existentCell[cell].baseStability[dir]>max){
				max = existentCell[cell].baseStability[dir];
			}
		}
	}

	if(max>counter){

		if(max==1)
		{
			score=-25*existentCell[cell].influence;//*=WEIGHT_stabilityRise;
		}
		else if(max==2)
		{
			score=-60*existentCell[cell].influence;//*=WEIGHT_stabilityRise;
		}
		else if(max==3)
		{
			score=-120*existentCell[cell].influence;//*=WEIGHT_stabilityRise;
		}
		else
		{
			score=-400*existentCell[cell].influence;//*=WEIGHT_stabilityRise;
		}
	}
	return score;
}

/**
 * Checks whether a cell is stable or not.
 *
 * @param cell - The cell that should be tested for its stability
 * @return True if cell is stable, false otherwise
 */
inline bool Map::isStable(uint16_t cell)
{
	if(cell==NO_CELL || board[cell]==0 || board[cell]>MAX_PLAYER)
	{
		return false;
	}

	for(int i=0; i<4; i++){
		if(!isStoneStable[4*cell+i])
		{
			return false;
		}
	}
	return true;
}

/**
 * Checks whether a stone is a frontier stone or not. 
 * A frontier stone is a stone that has at least one free cell as neighbour.
 *
 * @param cell - The cell on which the stone, for which should be determined if it is a frontier cell or not, is placed
 * @return True if the stone on the cell is a frontier stone, false otherwise
 */
inline bool Map::isFrontierStone(uint16_t cell){
	int neighbour;
	for(int dir=0; dir<DIRECTION_COUNT; ++dir)
	{
		neighbour = existentCell[cell].neighbour[dir];
		if(neighbour!=NO_CELL)
		{
			if(board[neighbour]==0 || (board[neighbour]>MAX_PLAYER && board[neighbour]!='x'))
			{
				return true;
			}
		}
	}

	return false;
}


/**
 * Returns whether placing a stone of a specified player to the start cell would result in a valid move.
 * Ignoring override stones.
 *
 * @param start  - The offset of the cell where a stone should be placed on
 * @param player - The player for whom the move is tested
 * @return	True if the move is valid, false if not
 */
bool Map::isMoveValid(uint16_t start, uint8_t player)
{
	int curDirection;
	int curCell;
	int prevCell;
	int curPlayer;
	int curBoardState;

	if(start==NO_CELL)
	{
		return false;
	}

	curPlayer = playerMap[player];
	curBoardState=board[start];

	if(curBoardState!=0 && curBoardState!='b' && curBoardState!='c' && curBoardState!='i')
	{
		return false;
	}

	int distance=0;

	for(int dir=0; dir<DIRECTION_COUNT; ++dir) // Check all directions
	{
		curCell = existentCell[start].neighbour[dir];
		curDirection = existentCell[start].direction[dir];

		distance=0;

		// while (next cell still a player or expansion cell AND not the start cell)
		while(curCell != NO_CELL && curCell != start && board[curCell] != curPlayer
			&& board[curCell] != 'c' && board[curCell] != 0
			&& board[curCell] != 'b' && board[curCell] != 'i')
		{
			distance++;
			prevCell = curCell;
			curCell = existentCell[prevCell].neighbour[curDirection];		// Get the next cell with the saved direction
			curDirection = existentCell[prevCell].direction[curDirection];	// Get the new direction
		}

		// Check if the current cell builds a line with the placed cell for the player
		if(curCell != NO_CELL && board[curCell] == curPlayer && curCell!=start && distance>=1)
		{
			return true;
		}
	}

	return false;
}

/**
 * Method that returns the real player that owns stone of that state on the board.
 *
 * @param state - The state which ownership get clarified
 * @return Owner of the stone
 */
inline uint8_t Map::getPlayerStoneOwnership(uint8_t state)
{	
	// The check from zero is necessary because of the map drawing and 0 mapping on itself
	for(uint8_t p=0; p<=getAmountOfPlayers(); ++p)
	{
		if(playerMap[p] == state)
		{
			return p;
		}
	}
	
	return NO_PLAYER;
}

/**
 * Updates which stones are stable depending on the stable neighbours
 */
void Map::adaptStableState(){

	// Define stable stones
	bool hasChanged=true;

	while(hasChanged)
	{
		hasChanged=false;
		for(int cell=0; cell<getAmountOfCells(); ++cell)
		{
			if(isStable(cell))
			{
				for(int dir=0; dir<DIRECTION_COUNT; ++dir)
				{
					int neighbour=existentCell[cell].neighbour[dir];
					if((neighbour!=NO_CELL) && (board[neighbour]==board[cell]))
					{
						if(!isStoneStable[LINE_COUNT*neighbour + (existentCell[cell].direction[dir]%LINE_COUNT)])
						{
							isStoneStable[LINE_COUNT*neighbour + (existentCell[cell].direction[dir]%LINE_COUNT)]=true;
							hasChanged=true;
						}
					}
				}
			}
		}
	}
}

void Map::resetStableState()
{
	memset(amountOfFreeCellsOnLine,0,lineCount*sizeof(uint16_t));

	for(int i=0; i<lineCount; ++i)
	{
		for(vector<int>::iterator it=cellsOnLine[i].begin(); it!=cellsOnLine[i].end(); ++it)
		{
			if(board[*it]==0 || (board[*it]>MAX_PLAYER && board[*it]!='x') )
			{
				amountOfFreeCellsOnLine[i]++;
			}
		}
	}

	// Set the base stability for every cell (=stability given by holes as direct neighbours)
	// This is used to avoid placing a stone near to corners or walls
	for(int cell=0; cell<getAmountOfCells(); ++cell)
	{
		for(int dir=0; dir<4; ++dir)
		{
			if((existentCell[cell].neighbour[dir]==NO_CELL) || (existentCell[cell].neighbour[dir+4]==NO_CELL))
			{
				isStoneStable[4*cell+dir]=true;
			}
			else if(amountOfFreeCellsOnLine[existentCell[cell].lineIndex[dir]]==0)
			{
				isStoneStable[4*cell+dir]=true;
			}
			else
			{
				isStoneStable[4*cell+dir]=false;
			}
		}
	}
}

/**
 * Returns the offset of all the cells that would get recoloured.
 * The offset may be included multiple times.
 *
 * @param start  - The offset of the cell that should get checked for re-colouring
 * @param player - The player for who it should get checked
 * @return	A vector of all the cells on which would get recoloured for that player
 */
vector<int> Map::getMoveCaptures(uint16_t start, uint8_t player)
{
	uint8_t curPlayer = playerMap[player];

	// Get all cells that have to be recoloured
	uint8_t curDirection = 0, prevDirection = 0;
	int curCell, prevCell;
	vector<int> recolourList;

	// Check all directions
	for(uint8_t dir=0; dir<DIRECTION_COUNT; ++dir)
	{
		curDirection = dir;
		curCell = existentCell[start].neighbour[curDirection];
		curDirection = existentCell[start].direction[curDirection];
		prevCell = start;

		// while(next cell still a player or expansion cell AND not the start cell)
		while(curCell != NO_CELL && curCell != start && board[curCell] != curPlayer
			&& board[curCell] != 'c' && board[curCell] != 0
			&& board[curCell] != 'b' && board[curCell] != 'i')
		{
			prevDirection = curDirection;									 // Save the current direction
			prevCell	  = curCell;										 // Save the current cell
			curDirection  = existentCell[prevCell].direction[prevDirection]; // Get the new direction
			curCell 	  = existentCell[prevCell].neighbour[prevDirection]; // Get the next cell with the saved direction
		}

		// Check if the current cell builds a line with the placed cell for the player
		if(curCell != NO_CELL && curCell != start && board[curCell] == curPlayer)
		{	
			// Mark all cells that need to be recoloured
			curDirection = (prevDirection + 4) % DIRECTION_COUNT; // Reverse direction to go backwards
			curCell = prevCell;						// Choose the last cell as a valid one and is not a line start/end

			while(curCell != start)
			{
				recolourList.push_back(curCell);								// Add to list for recolouring
				prevDirection = curDirection;									// Save current direction
				curDirection = existentCell[curCell].direction[curDirection];	// Get the new direction
				curCell = existentCell[curCell].neighbour[prevDirection];		// Get the next cell with the saved direction
			}
		}
	}

	return recolourList;
}

/**
 * Deletes all necessary cells from the map.
 * The offset may be included multiple times.
 *
 * @param start - The offset of the cell where the bomb will be placed
 * @param depth - The depth of the bomb for which cells get deleted
 */
inline void Map::bombCell(uint16_t start, int depth)
{
	if(start==NO_CELL || board[start]>=NO_STONE-bombExplosionRadius+depth)	// Nothing to destroy
	{
		return;
	}

	board[start] = NO_STONE - bombExplosionRadius + depth;

	if(depth > 0)
	{
		for(int dir=0; dir<DIRECTION_COUNT; ++dir)	// For every neighbour
		{
			bombCell(existentCell[start].neighbour[dir],depth-1);
		}
	}
}

/**
 * Add a new neighbour to the data structure
 *
 * @param x1   - The x coordinate of the first cell to be linked
 * @param y1   - The y coordinate of the first cell to be linked
 * @param dir1 - The direction of the link from the perspective of the first cell
 * @param x2   - The x coordinate of the second cell to be linked
 * @param y2   - The y coordinate of the first second to be linked
 * @param dir2 - The direction of the link from the perspective of the second cell
 */
void Map::addNeighbour(uint16_t x1, uint16_t y1, uint8_t dir1, uint16_t x2, uint16_t y2, uint8_t dir2)
{
	uint16_t cell1 = getOffset(x1, y1);
	uint16_t cell2 = getOffset(x2, y2);
	(existentCell[cell1]).neighbour[dir1] = cell2;
	(existentCell[cell1]).direction[dir1] = (dir2+4) % DIRECTION_COUNT;
	(existentCell[cell2]).neighbour[dir2] = cell1;
	(existentCell[cell2]).direction[dir2] = (dir1+4) % DIRECTION_COUNT;
}

////////////////////////////
////			     	////
////  PUBLIC FUNCTIONS  ////
////                    ////
////////////////////////////

/**
 * Set a flag whether override stones should be considered for the validation of playing phase move. 
 * This should be used to reduce the branching factor during move search. 
 * It is most likely that no override stones will be saved for the end of the game anyway.
 *
 * @param toConsider - True if override stones should be considered, false otherwise
 */
void setConsiderOverrideStones(bool toConsider)
{
	toConsiderOverrideStones = toConsider;
}

/**
 * Getter function for the flag that indicates if override stones are being considered.
 *
 * @return True if override stones are being considered, false otherwise
 */
bool getConsiderOverrideStones()
{
	return toConsiderOverrideStones;
}

/**
 * Returns the next player to be considered.
 *
 * @param lastPlayer
 * @return	The id of the player that should do the next move
 */
uint8_t getNextPlayer(uint8_t lastPlayer)
{
	int lastIndex = find(nextPlayers.begin(), nextPlayers.end(), lastPlayer) - nextPlayers.begin();
	return nextPlayers[(lastIndex+1)%nextPlayers.size()];
}

/**
 * Getter method for the amount of players that are considered while doing the search for the next move.
 *
 * @return The amount of players in the list of next Players
 */
uint8_t getAmountOfConsideredPlayers()
{
	return nextPlayers.size();
}

/**
 * Getter function for the width of the board.
 *
 * @return The width of the board
 */
uint16_t getWidth()
{
	return width;
}

/**
 * Getter function for the height of the board.
 *
 * @return The height of the board
 */
uint16_t getHeight()
{
	return height;
}

/**
 * Returns the offset of a cell at the coordinates (x,y).
 *
 * @param x - The x coordinate of the cell that should get returned
 * @param y - The y coordinate of the cell that should get returned
 * @return The offset of the cell for the intern arrays
 */
uint16_t getOffset(uint16_t x, uint16_t y)
{
	return offsetMap[x][y];
}

/**
 * Computes the x and y coordinate for a give offset.
 *
 * @param x - Pointer to integer that gets set to the x coordinate of the cell
 * @param y - Pointer to integer that gets set to the y coordinate of the cell
 * @param offset - The offset of the cell which coordinates are requested
 */
void reverseOffset(int* x, int* y, uint16_t offset)
{
	*x = reverseOffsetX[offset];
	*y = reverseOffsetY[offset];
}

/**
 * Getter function for the amount of cells on the board.
 *
 * @return The amount of cells on the board
 */
uint16_t getAmountOfCells()
{
	return cellcount;
}

/**
 * Getter function for the amount of players.
 *
 * @return The amount of players that started the game
 */
uint8_t getAmountOfPlayers()
{
	return amountOfPlayers;
}

/**
 * Getter function for the amount of players that have not been disqualified.
 *
 * @return The amount of players that have not been disqualified
 */
uint8_t getAmountOfActivePlayers()
{
	return amountOfActivePlayers;
}

/**
 * Getter function for disqualifying status of the player.
 * NOTE: Param player starts with player 1 as the number 1!
 *
 * @param player - The slot of the player that should get disqualified.
 */
void disqualifyPlayer(uint8_t player)
{
	amountOfActivePlayers--;
	disqualified[player] = true;
}

/**
 * Getter function for getting the disqualification status of the player.
 * NOTE: Param player starts with player 1 as the number 1!
 *
 * @param player - The player whose disqualification status should get checked
 * @return Returns true if the player has been disqualified, false otherwise
 */
bool isDisqualified(uint8_t player)
{
	return disqualified[player];
}

/**
 * Frees all memory allocated for the static part of the board.
 * This function should always be called at the end of a game to avoid memory leaks.
 */
void freeAllocatedMemory()
{
	delete[] existentCell;
	delete[] cellsOnLine;
}
