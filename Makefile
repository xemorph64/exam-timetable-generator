# Smart University Examination Timetable Generator
#   make            build ./timetable from src/ + include/
#   make test       build and run the self-tests
#   make single     regenerate main_single.cpp and build ./timetable_single
#   make run        build and start the menu
#   make clean      remove build output

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
INCLUDES := -Iinclude

SRC      := $(wildcard src/*.cpp)
OBJ      := $(SRC:src/%.cpp=build/%.o)
DEP      := $(OBJ:.o=.d)
TARGET   := timetable

.PHONY: all run test single clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

build:
	mkdir -p build

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	./$(TARGET) --test

main_single.cpp: $(SRC) $(wildcard include/*.h) tools/make_single.sh
	sh tools/make_single.sh > $@

single: main_single.cpp
	$(CXX) $(CXXFLAGS) main_single.cpp -o timetable_single

clean:
	rm -rf build $(TARGET) timetable_single

-include $(DEP)
