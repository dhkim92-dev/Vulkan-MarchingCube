CC=g++
DEPENDENCIES=scan.o marchingcube.o
TARGET=main
LDFLAGS=-LVKEngine/lib -lVKEngine -lvulkan -lglfw
INCLUDE_PATH=-IVKEngine/include
CFLAGS=--std=c++17

$(TARGET) : $(TARGET).cpp $(DEPENDENCIES)
	$(CC) $(CFLAGS) $(INCLUDE_PATH) $(LDFLAGS) -o $(TARGET).out $(TARGET).cpp $(DEPENDENCIES)

scan.o : scan.cpp
	$(CC) $(CFLAGS) $(INCLUDE_PATH) -c $^

marchingcube.o : marchingcube.cpp
	$(CC) $(CFLAGS) $(INCLUDE_PATH) -c $^

clean : 
	rm *.o
	rm *.out

