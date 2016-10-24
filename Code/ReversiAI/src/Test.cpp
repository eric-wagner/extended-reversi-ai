// This is the entry point for the test program
// The test coverage is pretty low though, as the move sorting
// algorithm are not really testable(there are hardly any clear best moves).

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <limits>
#include <string>
#include <fstream>
#include <ctime>	// clock()
#include <cstdlib>	// _sleep
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "map.h"
#include "algorithms.h"

using namespace std;

int main(int argc, char* argv[])
{
	int errorCount = 0;
	bool error=false;

	cout << "Starting Tests..." << endl << endl;

	/*
	 * TEST 1
	 * Checks if isPlayingPhaseMoveValid works correctly
	 */
	cout << "Executing Test 1" << endl;
	ifstream file1("testdata/test1.txt");
	if(!file1.is_open())
	{
		cout << "Loading Map failed!"<< endl;
		return EXIT_FAILURE;
	}
	Map map1(file1);
	file1.close();

	Map mapCopy1;

	for(int y=0; y<5; y++)
	{
		for(int x=0; x<5; x++)
		{
			mapCopy1.copy(map1);

			if(mapCopy1.isPlayingPhaseMoveValid(getOffset(x,y),1,0))
			{
				if(x==y)
				{
					cout << " Move ("<<x<<","<<y<<") should not be valid!" << endl;
					error=true;
					errorCount++;
				}
			}
			else
			{
				if(x!=y)
				{
					cout << " Move ("<<x<<","<<y<<") should be valid!" << endl;
					error=true;
					errorCount++;
				}
			}
		}
	}

	freeAllocatedMemory();
	if(!error)
	{
		cout << "Test 1 passed!!!" << endl;
	}
	//END OF TEST 1
	cout << endl << endl;
	
	/*
	 * TEST 2
	 * Checks if isBombingPhaseMoveValid works correctly
	 */
	cout << "Executing Test 3" << endl;
	ifstream file3("testdata/test3.txt");
	if(!file3.is_open())
	{
		cout << "Loading Map failed!"<< endl;
		return EXIT_FAILURE;
	}
	Map map3(file3);
	file3.close();
	Map mapCopy3;

	for(int y=0; y<5; y++)
	{
		for(int x=0; x<5; x++)
		{
			mapCopy3.copy(map3);
			if(mapCopy3.isBombingPhaseMoveValid(getOffset(x,y),1,0))
			{
				if(x==y)
				{
					cout << " Move ("<<x<<","<<y<<") should not be valid!" << endl;
					error=true;
					errorCount++;
				}
			}
			else
			{
				if(x!=y)
				{
					cout << " Move ("<<x<<","<<y<<") should be valid!" << endl;
					error=true;
					errorCount++;
				}
			}
		}
	}
	freeAllocatedMemory();
	if(!error)
	{
		cout << "Test 3 passed!!!" << endl;
	}
	//END OF TEST 2
	cout << endl << endl;

	/*
	 * TEST 3
	 * Checks if bomb drop works correctly
	 */
	cout << "Executing Test 4" << endl;
	ifstream file4("testdata/test4.txt");
	if(!file4.is_open())
	{
		cout << "Loading Map failed!"<< endl;
		return EXIT_FAILURE;
	}
	Map map4(file4);
	file4.close();

	map4.isBombingPhaseMoveValid(getOffset(5,5),1,0);
	for(int y=0; y<11; y++)
	{
		for(int x=0; x<11; x++)
		{
			if((x>3 && x<7) || (y>2 && y<8))
			{
				if(getOffset(x,y)!=NO_STONE && map4.getState(getOffset(x,y))!=NO_CELL)
				{
				cout << "Cell ("<<x<<","<<y<<") should not exist anymore!" << endl;
				error=true;
				errorCount++;
				}
			}
			else
			{
				if(map4.getState(getOffset(x,y))==NO_CELL)
				{
				cout << "Cell ("<<x<<","<<y<<") should still exist!" << endl;
				error=true;
				errorCount++;
				}
			}
		}
	}

	freeAllocatedMemory();

	cout<<endl;
	if(!error)	// no errors found
	{
		cout << "All tests passed successfully!" << endl;
		cout << "Changes can be committed to master branch" << endl;

	}
	else				// print number of errors
	{
		cout << errorCount << " errors detected!" << endl;
		cout << "Do NOT commit to master branch!" << endl;
	}

	cout << "Terminating..." << endl;

	return EXIT_SUCCESS;
}


