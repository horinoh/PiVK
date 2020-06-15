TARGET = VK
OBJS = Main.o

CC = g++
CFLAGS = -W -Wall -O2 -std=c++17
LDFLAGS = -lvulkan -lxcb
.SUFFIXES: .cpp .o

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(LDFLAGS) $^
.cpp.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS)
