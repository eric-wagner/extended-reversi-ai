/** 
* This program will execute the benchmarks for the AI.
* It will be controlled via the console input parameters.
* All benchmarks will be done from the perspective of player 1, 
* as we can be sure that he is present in every game.
*
* THIS PROGRAM HAS TO BE COMPILED WITH THE BENCHMARK FLAG DEFINED!
* Otherwise the default timers will be activated and might close the program automatically 
* and some of the variables used as output not declared.
*/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <limits.h>
#include <string.h>
#include <fstream>
#include <ctime>	// clock()
#include <cstdlib>	// _sleep
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "map.h"
#include "algorithms.h"

using namespace std;

#define PLAYING_PHASE_EVALUATION 1
#define BOMBING_PHASE_EVALUATION 2
#define END_OF_GAME_EVALUATION 3

Map map;

void benchmarkEvaluationFunction(Map& map, int index, long timeLimit);
void benchmarkSearchAlgorithm(Map& map, int index ,int depth);

int main(int argc, char* argv[])
{
	string program = argv[0];

	long timeLimit=10000;
	int depthLimit = 4;
	string path="";
	int testIndex=-1;

	if(argc==1)
	{
		cout << "Call " << program << " with the necessary parameters." << endl;
		cout << "-h or --help for help" << endl;
		return EXIT_FAILURE;
	}

	int i=1;
	while(i<argc){
		string cur = argv[i];

		if(cur.compare("-h")==0 || cur.compare("--help")==0)
		{
			i++;
			cout << program << " accepts the following options:" << endl;
			cout << "	required:" << endl;
			cout << "		-m or --map <mapfile>		use this map for the testing" << endl;
			cout << "		-b or --benchmark <test index>	1: playing evaluation function" << endl;
			cout << "						2: bombing evaluation function" << endl;
			cout << "						2: end of game evaluation function" << endl;
			cout << "						4: minimax algorithm " << endl;
			cout << "						5: alphabeta algorithm" << endl;
			cout << "						6: alphabeta algorithm with move sorting" << endl;
			cout << "						7: alphabeta algorithm with move sorting and aspirational windows" << endl;
			cout << "	optional:" << endl;
			cout << "		-h or --help			show this blob" <<endl;
			cout << " 		-t or --timeLimit 		use this time limit for testing(in ms)" << endl;
			cout << "						default is 10000ms" << endl;
			cout << " 		-d or --depth			use this depth limit for testing" << endl;
			cout << "						default is 4" << endl;
		}
		else if(cur.compare("-m")==0 || cur.compare("--map")==0)
		{
			i++;
			if(i<argc)
			{
				path=argv[i];
				i++;
			}
			if(path.compare("")==0)
			{
				cout << "-m or --map was called with an invalid parameter." <<endl;
				return EXIT_FAILURE;
			}
		}
		else if(cur.compare("-b")==0 || cur.compare("--benchmark")==0)
		{
			i++;
			if(i<argc){
				testIndex=atoi(argv[i]);
				i++;
			}
			if(testIndex<1 || testIndex>6)
			{
				cout << "-b or --benchmark was called with invalid parameters."<< endl;
				return EXIT_FAILURE;
			}

		}
		else if(cur.compare("-t")==0 || cur.compare("--timeLimit")==0)
		{
			i++;
			if(i<argc){
				timeLimit=atoi(argv[i]);
				i++;
			}
			if(timeLimit<=0)
			{
				cout << "-t or --timeLimit was called with invalid parameters." << endl;
				return EXIT_FAILURE;
			}
		}
		else if(cur.compare("-d")==0 || cur.compare("--depth")==0)
		{
			i++;
			if(i<argc){
				depthLimit=atoi(argv[i]);
				i++;
			}
			if(depthLimit<=0)
			{
				cout << "-d or --depth was called with invalid parameters." << endl;
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

	// Read the specified map and start the benchmark test that got requested by the user.
	ifstream file(path.c_str());
	if(!file.is_open())
	{
		cout << "Opening map file failed!"<< endl;
		return EXIT_FAILURE;
	}
	Map map(file);
	file.close();

	if(testIndex<=2)
	{
		cout << "Evaluating the following map with a time limit of " << timeLimit << " ms." << endl;
	}
	else
	{
		cout << "Searching a move on the follwing map with a depth limit of "<< depthLimit << "." << endl;
	}
	map.draw();


	switch(testIndex)
	{
		case(1):	benchmarkEvaluationFunction(map, PLAYING_PHASE_EVALUATION, timeLimit);
					break;
		case(2):	benchmarkEvaluationFunction(map, BOMBING_PHASE_EVALUATION, timeLimit);
					break;
		case(3):	benchmarkEvaluationFunction(map, END_OF_GAME_EVALUATION, timeLimit);
					break;
		case(4):	benchmarkSearchAlgorithm(map, MINIMAX, depthLimit);
					break;
		case(5):	benchmarkSearchAlgorithm(map, ALPHABETA, depthLimit);
					break;
		case(6): 	benchmarkSearchAlgorithm(map, ALPHABETA_MOVESORTING, depthLimit);
					break;
		case(7): 	benchmarkSearchAlgorithm(map, ASPIRATIONAL_WINDOW ,depthLimit);
					break;
		default:
					break;
	}

	freeAllocatedMemory();

	return EXIT_SUCCESS;
}

void benchmarkEvaluationFunction(Map& map, int index, long timeLimit)
{
	long startTime; // Milliseconds
	struct timespec spec;

	clock_gettime(CLOCK_REALTIME, &spec);

	double timePassed = 0;
	int evaluations = 0;

	startTime = spec.tv_sec*1000 + (spec.tv_nsec/1000000); // Convert nanoseconds to milliseconds

	while(timePassed<timeLimit)
	{
		if(index==PLAYING_PHASE_EVALUATION)
		{
			map.evaluateForPlayingPhase(1);
		}
		else if(index==BOMBING_PHASE_EVALUATION)
		{
			map.evaluateForBombingPhase(1);
		}
		else if(index==END_OF_GAME_EVALUATION)
		{
			map.evaluateForEndOfGame(1);
		}

		evaluations++;

		clock_gettime(CLOCK_REALTIME, &spec);
		timePassed = ((spec.tv_sec*1000)+(spec.tv_nsec/1000000))-startTime;
	}

	cout << evaluations << " evaluations done in " <<timePassed << " ms !" << endl;
}

void benchmarkSearchAlgorithm(Map& map, int index ,int depth)
{
	long startTime; // Milliseconds
	struct timespec spec;

	clock_gettime(CLOCK_REALTIME, &spec);

	startTime = spec.tv_sec*1000 + (spec.tv_nsec/1000000); // Convert nanoseconds to milliseconds

	map.initializeNeighbourList(1);

	if(index==MINIMAX)
	{
		getNextMove(map, depth, 0, 1, MINIMAX, true);
	}
	else if(index==ALPHABETA)
	{
		getNextMove(map, depth, 0, 1, ALPHABETA, true);
	}
	else if(index==ALPHABETA_MOVESORTING)
	{
		getNextMove(map, depth, 0, 1, ALPHABETA_MOVESORTING, true);
	}
	else if(index==ASPIRATIONAL_WINDOW)
	{
		getNextMove(map, depth, 0, 1, ASPIRATIONAL_WINDOW, true);
	}

	clock_gettime(CLOCK_REALTIME, &spec);
	long timePassed = ((spec.tv_sec*1000)+(spec.tv_nsec/1000000))-startTime;

	long nodes = getNumberOfAnalyzedNodes();

	double average = -1;
	if(timePassed > 0)
	{
		average = nodes * 1/(timePassed/1000.0);
	}

	long timeSpendEvaluating = getTimeSpendEvaluating();
	double percentageEvaluating = timeSpendEvaluating* 1.0/timePassed * 100;

	cout << "Node analysed: 				" << nodes << endl;
	cout << "Time taken: 				" << timePassed << " ms"<< endl;
	cout << "Average nodes per second:		" << average << " evaluations per second" << endl;
	cout << "Time spend evaluating leafs:		" << timeSpendEvaluating << " ms" << endl;
	cout << "Percentage of time spend evaluating:	" << percentageEvaluating << " %" << endl;
}
