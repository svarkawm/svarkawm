CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude $(shell pkg-config --cflags x11 xft xrender xext) -MMD
LIBS = $(shell pkg-config --libs x11 xft xrender xext) -lpthread

SRC_DIR = src
OBJ_DIR = build
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
DEPENDS = $(OBJECTS:.o=.d)
TARGET = $(OBJ_DIR)/svarkawm

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

-include $(DEPENDS)

clean:
	rm -f $(OBJ_DIR)/*

install: all
	install -Dm755 $(TARGET) /usr/local/bin/svarkawm

uninstall:
	rm -f /usr/local/bin/svarkawm

.PHONY: all clean install uninstall