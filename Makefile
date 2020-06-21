TARGET = VK
OBJS = Main.o
VS = VS.spv
FS = FS.spv

CC = g++
CFLAGS = -W -Wall -Wno-psabi -O2 -std=c++17 -I./glm
LDFLAGS = -lvulkan -lxcb

GLSL = glslangValidator

.SUFFIXES: .cpp .o

.PHONY: all
all: $(TARGET) $(VS) $(FS)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(LDFLAGS) $^
.cpp.o:
	$(CC) $(CFLAGS) -c $<

$(VS): VS.vert
	$(GLSL) -V $< -o VS.spv
$(FS): FS.frag
	$(GLSL) -V $< -o FS.spv

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS) $(VS) $(FS)
