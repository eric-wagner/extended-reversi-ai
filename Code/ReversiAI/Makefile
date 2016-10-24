CC = g++

#compiler flags:
#  	-g    				adds debugging information to the executable file
#  	-Wall 				turns on most, but not all, compiler warnings
#	-Og					enables optimizations that do not interface with debugging
#	-fmessage-lenth=0	each error message will appear in a single line
# 
CXXFLAGS = -std=c++11 -fprofile-use=./profiling -O3 -flto -Wall -fmessage-length=0

DIR=compiled

OBJS_AI =      	 $(DIR)/ReversiAI.o $(DIR)/map.o $(DIR)/client.o $(DIR)/algorithms.o
OBJS_BENCHMARK = $(DIR)/Benchmark.o $(DIR)/map.o $(DIR)/client.o $(DIR)/algorithmsWithTimer.o

TARGET =    $(DIR)/ReversiAI.exe $(DIR)/Benchmark.exe $(DIR)/Test.exe

all:  $(TARGET) 

$(DIR)/ReversiAI.exe:	$(OBJS_AI)
	$(CC) $(CXXFLAGS) -o $(DIR)/ReversiAI.exe $(OBJS_AI) $(LIBS)
	
$(DIR)/Benchmark.exe: $(OBJS_BENCHMARK)
	$(CC) $(CXXFLAGS) -o $(DIR)/Benchmark.exe $(OBJS_BENCHMARK) $(LIBS)
	
$(DIR)/ReversiAI.o: src/ReversiAI.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@
	
$(DIR)/Benchmark.o : src/Benchmark.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@
		
$(DIR)/AiOpti.o : src/AiOpti.cpp src/weights.h
	$(CC) $(CXXFLAGS) -c $< -o $@

$(DIR)/map.o: src/map.cpp src/map.h src/weights.h
	$(CC) $(CXXFLAGS) -c $< -o $@
	
$(DIR)/algorithms.o: src/algorithms.cpp src/algorithms.h src/map.h
	$(CC) $(CXXFLAGS) -c $< -o $@
	
$(DIR)/algorithmsWithTimer.o: src/algorithms.cpp src/algorithms.h src/map.h
	$(CC) $(CXXFLAGS) -D BENCHMARK -c $< -o $@	
	
$(DIR)/client.o: src/client.cpp src/client.h src/map.h
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS_AI) $(OBJS_BENCHMARK) $(TARGET)
