CXX = g++
CXXFLAGS = -O3 -Wall -std=c++17
X11_FLAGS = $(shell pkg-config --cflags x11 xft)
X11_LIBS = $(shell pkg-config --libs x11 xft)

PREFIX ?= /usr

all: check_deps svarkawm

check_deps:
	@pkg-config --exists x11 xft || (echo "Error: x11 or xft not found. Install them via emerge."; exit 1)

svarkawm: main.o svarkawm.o
	$(CXX) $(CXXFLAGS) main.o svarkawm.o -o svarkawm $(X11_LIBS)

main.o: main.cpp svarkawm.h
	$(CXX) $(CXXFLAGS) $(X11_FLAGS) -c main.cpp

svarkawm.o: svarkawm.cpp svarkawm.h
	$(CXX) $(CXXFLAGS) $(X11_FLAGS) -c svarkawm.cpp

clean:
	rm -f svarkawm *.o

install: svarkawm
	install -Dm755 svarkawm $(DESTDIR)$(PREFIX)/bin/svarkawm

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/svarkawm

.PHONY: all clean check_deps install uninstall