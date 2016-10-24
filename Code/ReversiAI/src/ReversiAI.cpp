// This is the main entry point of the program for the normal use.
// It parses the input parameters and executes the client or
// the local testing program accordingly.

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <limits>
#include <string.h>		// std::string
#include <sstream>		// std::istringstream
#include <fstream>		// std::ifstream

#include "map.h"
#include "client.h"
#include "algorithms.h"

using namespace std;

int play(string ip, string port, int algo);
int test(char* path);
void freeMemory();

uint8_t player=1;

const uint8_t groupID = 3;

const string version = "v6.00.01";

/**
 * This is the main entry point. 
 * It is responsible for the correct parameter parsing. 
 * For most cases the program will continue either in the test or the play function.
 */
int main(int argc, char* argv[])
{
	initTimer();

	string program = argv[0];

	char* path=(char*)"";
	string ip="localhost";	// default value
	string port="7777";		// default value
	int toExecute=1;
	int algo=ALPHABETA_MOVESORTING;
	bool mapSet=false;

	int i=1;
	while(i<argc){
		string cur = argv[i];

		if(cur.compare("-h")==0 || cur.compare("--help")==0)
		{
			i++;
			cout << program << " accepts the following options:" << endl;
			cout << "	required:" << endl;
			cout << "		-e or --execute <program index>	program to execute(default: 1)" << endl;
			cout << "						1: connecting to host" << endl;
			cout << "						2: local testing" << endl;
			cout << "	required for testing:" << endl;
			cout << "		-m or --map <mapfile>		use this map for the testing" << endl;
			cout << "	required for connecting to a host:" <<endl;
			cout << " 		-i or --ip  			ip used to connect to server (default: localhost)" << endl;
			cout << "		-p or --port			port used to connect to server (default: 7777)" << endl;
			cout << "		-a or --algorithm		search algorithm used (default: 2)" << endl;
			cout << " 						1: minimax with maxn search" << endl;
			cout << "						2: alphabeta" << endl;
			cout << "						3: alphabeta with move sorting" << endl;
			cout << "						4: alphabeta algorithm with move sorting and aspiration windows" << endl;
			cout << "	optional:" << endl;
			cout << "		-h or --help			show this blob" <<endl;
			cout << "		-v or --version			show the version number" <<endl;
			return EXIT_SUCCESS;
		}
		else if(cur.compare("-v")==0 || cur.compare("--version")==0)
		{
			cout << "ReversiAI Group 3: " << version << endl;
			return EXIT_SUCCESS;
		}
		else if(cur.compare("-e")==0 || cur.compare("--execute")==0)
		{
			i++;
			if(i<argc){
				toExecute=atoi(argv[i]);
				i++;
			}
			if(toExecute<0 || toExecute>2)
			{
				cout << "-e or --execute was called with invalid parameters."<< endl;
				return EXIT_FAILURE;
			}

		}
		else if(cur.compare("-m")==0 || cur.compare("--map")==0)
		{
			i++;
			mapSet=true;
			if(i<argc){
				path=argv[i];
				i++;
			}
		}
		else if(cur.compare("-a")==0 || cur.compare("--algorithm")==0)
		{
			i++;
			if(i<argc){
				algo=atoi(argv[i]);
				i++;
			}
			else
			{
				cout << "-a or --algorithm was called with invalid parameters."<< endl;
				return EXIT_FAILURE;
			}

			if(algo<1 || algo>4)
			{
				cout << "-a or --algorithm was called with invalid parameters."<< endl;
				return EXIT_FAILURE;
			}

		}
		else if(cur.compare("-i")==0 || cur.compare("--ip")==0)
		{
			i++;
			if(i<argc){
				ip=argv[i];
				i++;
			}
			else
			{
				cout << "-i or --ip was called with invalid parameters." << endl;
				return EXIT_FAILURE;
			}
		}
		else if(cur.compare("-p")==0 || cur.compare("--port")==0)
		{
			i++;
			if(i<argc){
				port=(argv[i]);
				i++;
			}
			else
			{
				cout << "-p or --port was called with invalid parameters." << endl;
				return EXIT_FAILURE;
			}
		}
		else
		{
			cout << "Call " << program << " with valid parameters." << endl;
			cout << "-h or --help for help" << endl;
			return EXIT_FAILURE;
		}
	}

	cout << "G3-Main: Algorithm " << algo << endl;

	switch(toExecute)
	{
		case(1):	play(ip,port,algo);
					break;
		case(2):	if(mapSet)
					{
						test(path);
					}
					break;
		default:
					break;
	}

	freeMemory();

	return EXIT_SUCCESS;
}

/**
 * This method is responsible for the normal game flow.
 * It will first setup a connection to the specified host,
 * then read all the necessary information from the host and start the game loop.
 */
int play(string ip, string port, int algo)
{
	if(setupConnection(ip, port, groupID) == false)	// Setup with the group number 3
	{
		// No Error message needed, function will output it automatically
		cout << "G3-Main: Terminating..." << endl;
		return EXIT_FAILURE;
	}

	Map map = getMap();
	map.draw();

	player = getPlayerNumber();
	cout << "G3-Main: Player number for this game is " << (int)player << endl;

	bool isPlayingPhaseRunning = true;
	bool isBombingPhaseRunning = false;

	while(isPlayingPhaseRunning)	// First stage
	{
		// Wait for the server to send a message and act accordingly
		// Repeat until the server changes the state of the game
		listenToServer();

		uint8_t messageType = getLatestMessageType();

		if(messageType == 4)	// Our turn
		{
			map.initializeNeighbourList(player);
			cout << "G3-Main: Searching valid move with " << map.getAmountOfOverrideStones(player) << " override stones" << endl;
			Move move = *getNextMove(map, getSearchDepth(), getTimeLimit() , player , algo, true);
			cout << "G3-Main: Move found: (" << move.x << ", " << move.y << ", " << (int)move.choice << ")" << endl;
			sendMove(move);
		}
		else if(messageType == 6)	// Move made by other player
		{

			Move move = getMove();
			setConsiderOverrideStones(true);
			cout << "G3-Main: Player " << (int)move.player << " placed a stone on Cell (" << move.x << "," << move.y << ")" << endl;
			if(!map.isPlayingPhaseMoveValid(getOffset(move.x,move.y),move.player,move.choice))
			{
				cout << "G3-Error: Invalid move received from server" << endl;
			}
			cout << "G3-Main: Player " << (int)move.player << " has " << map.getAmountOfOverrideStones(move.player) << " override stones and " << map.getAmountOfBombs(move.player) << " bombs left." << endl;
			cout << endl;
		}
		else if(messageType == 7)	// A player made an invalid move and got disqualified
		{
			uint8_t disqualifiedPlayerIndex = getDisqualifiedPlayer();
			cout << "G3-Main: Player " << (int)disqualifiedPlayerIndex << " got disqualified" << endl;
			if(disqualifiedPlayerIndex == player)	// If we got disqualified, end the program
			{
				isPlayingPhaseRunning = false;
			}

			disqualifyPlayer(player);
		}
		else if(messageType == 8)	// First phase has ended
		{
			cout << "G3-Main: End of playing phase reached" << endl;
			isPlayingPhaseRunning = false;
			map.initializeNeighbourList(0);
			isBombingPhaseRunning = true;
		}
		else if(messageType == 9)	// Second phase has ended
		{
			cout << "G3-Main: End of  game reached" << endl;
			isPlayingPhaseRunning = false;
		}
		else
		{
			cout << "G3-Error : Message of type "<< (int)messageType << " received, while expecting 4,6,7 or 8" << endl;
			isPlayingPhaseRunning = false;
		}
	}

	while(isBombingPhaseRunning)	// Second stage
	{
		listenToServer();

		uint8_t messageType = getLatestMessageType();

		if(messageType == 4)	// Our turn
		{
			Move move = *getNextMove(map, getSearchDepth(), getTimeLimit(), player, algo, false);
			cout << "G3-Main: Bomb placed on Cell (" << move.x << "," << move.y << ")" << endl;
			sendMove(move);
		}
		else if(messageType == 6)	// Move made by other player
		{
			Move move = getMove();
			cout << "G3-Main: Player " << (int)move.player << " placed a bomb on Cell (" << move.x << "," << move.y << ")" << endl;
			map.isBombingPhaseMoveValid(getOffset(move.x,move.y), move.player, move.choice);
			map.draw();
		}
		else if(messageType == 7)	// A player made an invalid move and got disqualified
		{
			uint8_t disqualifiedPlayerIndex = getDisqualifiedPlayer();
			cout << "G3-Main: Player " << (int)disqualifiedPlayerIndex << " got disqualified" << endl;
			if(disqualifiedPlayerIndex == player)	// If we got disqualified, end the program
			{
				isBombingPhaseRunning = false;
			}

			disqualifyPlayer(player);
		}
		else if(messageType == 9)	// Second phase has ended
		{
			cout << "G3-Main: End of  game reached" << endl;
			isBombingPhaseRunning = false;
		}
		else
		{
			cout << "G3-Error: Message of type "<< (int)messageType << " received, while expecting 4,6,7 or 9" << endl;
			isBombingPhaseRunning = false;
		}
	}

	closeConnection();

	return EXIT_SUCCESS;
}

/**
 * This is just a local playground to test different things.
 * Usually it reads a map from the specified path and executes some algorithms on them. 
 * Its use is constantly changing.
 */
int test(char* path)
{
	ifstream file(path);
	if(!file.is_open())
	{
		cout << "G3-Error: Opening map file failed!"<< endl;
		return EXIT_FAILURE;
	}
	Map map(file);
	file.close();
	map.draw();
	map.initializeNeighbourList(1);
	getNextMove(map,4,0,1,ALPHABETA_MOVESORTING,false);

	return EXIT_SUCCESS;
}

/**
 * Calls all the functions that frees the necessary space for exiting the program.
 */
void freeMemory()
{
	freeAllocatedMemory();
}
