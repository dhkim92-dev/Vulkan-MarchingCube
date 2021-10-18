CC=g++
DEPENDENCIES=scan.o marchingcube.o
TARGET=main
LDFLAGS=-LVKEngine/lib -lVKEngine -lvulkan -lglfw
INCLUDE_PATH=-IVKEngine/include
CFLAGS=--std=c++17

$(TARGET) : $(TARGET).cpp $(DEPENDENCIES)
	$(CC) $(CFLAGS) $(INCLUDE_PATH) $(LDFLAGS) -o $(TARGET).out $(TARGET).cpp $(DEPENDENCIES)

scan.o :
	$(CC) $(CFLAGS) $(INCLUDE_PATH) -c scan.cpp

marchingcube.o :
	$(CC) $(CFLAGS) $(INCLUDE_PATH) -c marchingcube.cpp

clean : 
	rm *.o
	rm *.out

