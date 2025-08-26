CXX = g++
CXXFLAGS = -Wall -g

TARGET = main
SRCS = Car.cpp JumpUpState.cpp MoveBackwardState.cpp MoveForwardState.cpp SwithcOnBoostState.cpp SwitchOffBoostState.cpp main.cpp 
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET) run

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)


