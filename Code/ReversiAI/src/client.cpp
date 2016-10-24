/** 
* The functions provided in this file are used to communicate with a server. 
* It provides the a method for the initial setup of a TCP connection and 
* a functions that waits for a message from the server, that is then parsed.
* The other functions provide the information send by the host, to other parts of the program.
*/

#include <netdb.h>		// htons, htonl, ntohs, ntohl
#include <netinet/in.h>
#include <sys/types.h>	// need for portability reasons
#include <sys/socket.h>	// std::socket
#include <cstdlib>		// std::malloc
#include <iostream>		// std::cout
#include <string.h>		// std::string
#include <cstring>
#include <errno.h> 		// strerror
#include <unistd.h>		// close
#include <sstream>

#include "client.h"
#include "map.h"

char latestMessageType = 0;
char lastDisqulifiedPlayer = 0;
uint8_t searchDepth =0;
uint32_t timeLimit = 0;
Move lastMoveExecuted;

void sendMessage(uint8_t type, uint32_t lenght, uint8_t* message);
void readMessage(int32_t length, char* message);

int clientSocket=-1;

struct hostent* host;
struct sockaddr_in serv_addr;

struct addrinfo hints;
struct addrinfo* res;

/**
 * Setup the socket connecting to a game server with the specified ip on the specified port.
 * If no connection has been setup successfully, the result of every other function is undefined.
 * A successful connection has to be closed with the closeConnection() function at the end of the data transfer.
 *
 * @param ip		  - Contains the ip of the server
 * @param port		  - Contains the port on which the server listens
 * @param groupNumber - Contains the group number of the team
 * @return True upon successfully setting up the socket connection, false otherwise
 */
bool setupConnection(string ip, string port, uint8_t groupNumber){

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;

	int addrStatus = getaddrinfo(ip.c_str(), port.c_str(),&hints, &res);
	if( addrStatus != 0)
	{
		cout << "G3-Error: Could not get host information: " << strerror(errno) << endl;
		return false;
	}

	clientSocket = socket(AF_INET, SOCK_STREAM, 0);	// SOCK_STREAM for TCP socket
	if(clientSocket==-1)
	{
		cout << "G3-Error: Could not create socket! " << strerror(errno) << endl;
		return false;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = ((struct sockaddr_in*)(res->ai_addr))->sin_addr;
	serv_addr.sin_port = htons(atoi(port.c_str()));

	int connectionStatus = connect(clientSocket,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	if ( connectionStatus == -1)
	{
		cout << "G3-Error: Connecting to host failed: " << strerror(errno) << endl;
		return false;
	}

	cout << "G3-Client: Sending group number" << endl;
	sendMessage(1, 1, &groupNumber);

	return true;
}


/**
 * Closes the TCP connection to the server.
 */
void closeConnection(){
	close(clientSocket);
	freeaddrinfo(res);
}

/**
 * Returns the player number this AI gets assigned.
 *
 * @return  The player number this AI gets upon success, 0xFF otherwise
 */
uint8_t getPlayerNumber()
{
	// Reading the message type
	char messageType;
	readMessage(1,&messageType);
	if(messageType != 3)
	{
		cout << "G3-Error: Received message of type " << (int)(messageType) << ", while expecting 3." << endl;
		return NO_PLAYER;
	}

	// Reading the length
	char l[4];
	uint32_t length;
	readMessage(4,l);

	memcpy(&length, l, 4);
	length = ntohl(length);
	if(length != 1)
	{
		cout << "G3-Error: Received message of length  " << length << ", while expecting 1" << endl;
		return 0xff;
	}

	// Reading the player number
	char playerNumber;
	readMessage(1, &playerNumber);

	return playerNumber;
}

/**
 * Returns a pointer to the map that gets transferred through the socket.
 *
 * @return  An instance of the class map
 */
Map getMap()
{
	cout << "G3-Client: Receiving Map" <<endl;

	// Reading the message type
	char messageType = 0;
	readMessage(1, &messageType);
	if(messageType != 2)
	{
		cout << "G3-Error: Received message of type " << (int)(messageType) << ", while expecting 2." << endl;
	}

	// Reading the length
	char l[4]={0};
	uint32_t length=0;
	readMessage(4,l);
	memcpy(&length, l, 4);
	length = ntohl(length);

	// Reading the map
	char message[length+1]={0};
	readMessage(length, message);
	message[length+1]='\0';

	string s(message);
	istringstream stream(s);
	return Map(stream);
}

/**
 * Wait until the server sends a new message. If so it is saved what type of message was send 
 * and reacts appropriately by reading the data and save it in a way it can be accessed by the according function.
 */
void listenToServer()
{
	readMessage(1, &latestMessageType);

	char l[4];
	uint32_t length;
	readMessage(4,l);
	memcpy(&length, l, 4);
	length = ntohl(length);

	if(latestMessageType==4)
	{
		if(length != 5)
		{
			cout << "G3-Error: Received message of length  " << length << ", while expecting 5" << endl;
			return;
		}

		// Extract time and depth limit
		char message[5];
		readMessage(5,message);

		// Get Time Limit
		memcpy(&timeLimit, message, 4);
		timeLimit = ntohl(timeLimit);

		// Get Search Depth
		searchDepth = message[4];
	}
	else if(latestMessageType==6)
	{
		if(length != 6)
		{
			cout << "G3-Error: Received message of length  " << length << ", while expecting 6" << endl;
			return;
		}

		// Receive move from another player
		char message[6];
		readMessage(6,message);
		uint16_t x,y;
		memcpy(&x, message, 2);
		memcpy(&y, message+2, 2);
		lastMoveExecuted.x = ntohs(x);
		lastMoveExecuted.y = ntohs(y);
		lastMoveExecuted.choice= message[4];
		lastMoveExecuted.player= message[5];

	}
	else if(latestMessageType==7)
	{
		if(length != 1)
		{
			cout << "G3-Error: Received message of length  " << length << ", while expecting 1" << endl;
			return;
		}

		// Receiving the 8-bit unsigned integer indicating which player has been disqualified
		readMessage(1,&lastDisqulifiedPlayer);
	}
}

/**
 * Returns the type of the last received message from the server.
 *
 * @return 	The type of the last received message from the server
 */
uint8_t getLatestMessageType()
{
	return latestMessageType;
}

/**
 * Returns a player that got disqualified.
 * listentoServer() has to be called first and receive a message of type 7.
 *
 * @return 	A player that got disqualified
 */
uint8_t getDisqualifiedPlayer()
{
	return lastDisqulifiedPlayer;
}

/**
 * Returns the search depth for the next move.
 * listentoServer() has to be called first and receive a message of type 4.
 *
 * @return 	The search depth for the next move
 */
uint8_t getSearchDepth()
{
	return searchDepth;
}

/**
 * Returns the search depth for the next move.
 * listentoServer() has to be called first and receive a message of type 4.
 *
 * @return 	The time limit(in ms) for the next move
 */
uint32_t getTimeLimit()
{
	return timeLimit; // Set to 3 second to test
}

/**
 * Returns a struct that contains the information of a move.
 * listentoServer() has to be called first and receive a message of type 6.
 *
 * @return 	A struct that contains the information of a move
 */
Move getMove()
{
	return lastMoveExecuted;
}

/**
 * Sends a move to the sever at the (x,y) coordinate with the information for choice and bonus cells.
 *
 * @param Move - A instance of a move struct that contains the information of the coordinates and type of move
 */
void sendMove(Move move)
{
	uint8_t messageType = 5;
	uint32_t messageLenght = 5;
	uint8_t message[5];
	uint16_t x = htons(move.x);
	uint16_t y = htons(move.y);
	memcpy(message, &x, 2);
	memcpy(message+2, &y, 2);
	message[4]=move.choice;
	sendMessage(messageType, messageLenght, message);
}

/**
 * Sends a message to the server.
 *
 * @param type		- Contains the type of the message that will be send
 * @param length	- Contains the length of the message that will be send
 * @param message	- Contains the message that should be sent to the server
 */
void sendMessage(uint8_t type, uint32_t length, uint8_t* message)
{
	uint8_t buffer[5+length];

	buffer[0] = type;

	// Convert length to network byte order
	uint32_t l = htonl(length);
	memcpy(buffer+1, &l, 4);

	memcpy(buffer+5, message, length);

	unsigned int bytesSend=0;
	int sendStatus;
	while(bytesSend < length+5)
	{
		if(bytesSend!=0)
		{
			cout << "G3-Client: Only "<<bytesSend<<" could be transferred. Trying again..." << endl;
		}

		sendStatus = send(clientSocket, buffer+bytesSend, (length+5)-bytesSend , 0);
		if (sendStatus == 0)
		{
			cout << "G3-Error: No more bytes can be transferred!" << endl;
			return;
		}
		else if (sendStatus == -1)
		{
			cout << "G3-Error: " << errno << endl;
			return;
		}
		bytesSend += sendStatus;
	}
}

/**
 * Reads a message send by the server.
 *
 * @param length	- Contains the expected length of the server message
 * @param message	- A pointer to where the message should be stored. Should have a size >= length
 */
void readMessage(int32_t length, char* message){
    int bytesRead = 0;
    int readStatus;
    while (bytesRead < length)
    {
    	if(bytesRead!=0)
		{
			cout << "G3-Client: Only "<<bytesRead<<" could be read. Trying again..." << endl;
		}

        readStatus = read(clientSocket, message + bytesRead, length - bytesRead);
        if (readStatus == 0 )
        {
            cout << "G3-Error: No more bytes to read!" << endl;
            return;
        }
        else if (readStatus == -1)
        {
        	cout << "G3-Error: " << errno << endl;
        	return;
        }

        bytesRead += readStatus;
    }
}
