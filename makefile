CC=g++
DEPENDENCIES=scan.o
TARGET=main
LDFLAGS=-LVKEngine/lib -lVKEngine -lvulkan -lglfw
INCLUDE_PATH=-IVKEngine/include
CFLAGS=--std=c++17

$(TARGET) : $(TARGET).cpp $(DEPENDENCIES)
	$(CC) $(CFLAGS) $(INCLUDE_PATH) $(LDFLAGS) -o $(TARGET).out $(TARGET).cpp $(DEPENDENCIES)

scan.o : scan.cpp
	$(CC) $(CFLAGS) $(INCLUDE_PATH) -c scan.cpp

clean : 
	rm *.out
	rm *.o

