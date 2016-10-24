5.6.2016

How To Use The Reversi Ai
-------------------------

After compiling the project with the makefile you
should find a ReversiAi.exe in the ./compiled folder.
The arguments important arguments are:

-i or --ip <ip adress> 	To set the hosts ip address
-p or --port <port>	To set the port on which the host is listening
-h or --help 		To open an description of the arguments

The default ip is "localhost" and the default port is "7777".

We have also an argument to set which search algorithm should be used, but as of know only minimax is implemented. So choosing alphabeta will not work!