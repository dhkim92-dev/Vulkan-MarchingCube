CC=g++
DEPENDENCIES=scan.o marchingcube.o
TARGET=main
LDFLAGS=-LVKEngine/lib -lvulkan -lVKEngine -lglfw
INCLUDE_PATH=-IVKEngine/include
CFLAGS=--std=c++17

$(TARGET) : $(TARGET).cpp $(DEPENDENCIES)
	$(CC) $(CFLAGS) -o $(TARGET).out $(TARGET).cpp $(DEPENDENCIES) $(INCLUDE_PATH) $(LDFLAGS)

scan.o : scan.cpp
	$(CC) $(CFLAGS) $(INCLUDE_PATH) -c $^

marchingcube.o : marchingcube.cpp
	$(CC) $(CFLAGS) $(INCLUDE_PATH) -c $^

clean : 
	rm *.o
	rm *.out

