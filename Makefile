# Smart University Examination Timetable Generator
#   make            build ./timetable from src/ + include/
#   make test       build and run the self-tests
#   make single     regenerate main_single.cpp and build ./timetable_single
#   make run        build and start the menu
#   make web        build the WebAssembly version into web/ (needs Docker)
#   make clean      remove build output

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
INCLUDES := -Iinclude

SRC      := $(wildcard src/*.cpp)
OBJ      := $(SRC:src/%.cpp=build/%.o)
DEP      := $(OBJ:.o=.d)
TARGET   := timetable

.PHONY: all run test single web clean

# WebAssembly build for the browser (web/index.html, hosted on Vercel).
# Emscripten runs from its official Docker image, so nothing else is installed.
EMSDK_IMAGE := emscripten/emsdk:3.1.74
WEBFLAGS := -std=c++17 -O2 -Iinclude -fexceptions -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 \
            -sMODULARIZE -sEXPORT_NAME=createTimetable -sENVIRONMENT=web,worker -sEXIT_RUNTIME=1 \
            -sALLOW_MEMORY_GROWTH -sEXPORTED_RUNTIME_METHODS=FS \
            '-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$$stringToNewUTF8'

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

web: $(SRC) $(wildcard include/*.h)
	docker run --rm -u $$(id -u):$$(id -g) -v "$$PWD":/src -w /src $(EMSDK_IMAGE) \
		em++ $(WEBFLAGS) $(SRC) -o web/timetable.js

clean:
	rm -rf build $(TARGET) timetable_single

-include $(DEP)
