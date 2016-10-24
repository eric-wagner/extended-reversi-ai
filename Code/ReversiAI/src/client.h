#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>
#include <string>
#include "map.h"

using namespace std;

bool setupConnection(string ip, string port, uint8_t groupNumber);
void closeConnection();

uint8_t getPlayerNumber();
Map getMap();

void listenToServer();
uint8_t getLatestMessageType();
Move getMove();
uint8_t getDisqualifiedPlayer();
uint8_t getSearchDepth();
uint32_t getTimeLimit();

void sendMove(Move move);


#endif /* CLIENT_H_ */
