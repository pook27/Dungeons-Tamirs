# 1. OS Detection System
ifeq ($(OS),Windows_NT)
    # Windows Configurations
    EXE = .exe
    # Raylib on Windows usually requires these core Win32 system libraries
    LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
    # Command to run the executable
    RUN_CMD = .\$(TARGET)
else
    # Linux Configurations
    EXE = 
    LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    RUN_CMD = ./$(TARGET)
endif

# 2. Variables
CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = main$(EXE)
SRCS = main.c assets.c world.c entities.c upgrades.c ui.c
HDRS = $(wildcard *.h)

# 3. Phony Targets (Commands that aren't physical files)
.PHONY: all run clean

# Default target: builds the program
all: $(TARGET)

# Links the object files/source files into the final executable.
# Depends on HDRS too (constants.h, game_types.h, ...) so editing a header alone still triggers a rebuild -
# without this, changing e.g. DASH_DAMAGE in constants.h and running `make run` silently reruns the stale binary.
$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) $(LIBS) -o $(TARGET)

# Builds and runs the application in one step
run: $(TARGET)
	$(RUN_CMD)

# Cleans up the build files depending on the OS environment
clean:
ifeq ($(OS),Windows_NT)
	del /Q $(TARGET)
else
	rm -f $(TARGET)
endif
