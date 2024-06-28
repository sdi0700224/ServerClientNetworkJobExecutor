# Compiler and Linker
CXX = g++
CC = gcc

# Directories
INCLUDE_DIR = include
SRC_DIR = src
TESTS_DIR = tests
BUILD_DIR = build
BIN_DIR = bin

# Runtime Files
RUN_FILES = jobExecutorServer.txt
TEMP_FILES = chr19.fa

# Executables
EXEC_JOB_COMMANDER = $(BIN_DIR)/jobCommander
EXEC_JOB_EXECUTOR_SERVER = $(BIN_DIR)/jobExecutorServer
EXEC_PROG_DELAY = $(BIN_DIR)/progDelay

# Flags, Libraries and Includes
CXXFLAGS ?= -std=c++17 -Wall -Werror -I$(INCLUDE_DIR)
CFLAGS ?= -Wall -Werror
LDFLAGS ?=
LDLIBS ?= -lpthread -lm

# Source files for each executable
SOURCES_JOB_COMMANDER := $(SRC_DIR)/JobCommander.cpp $(SRC_DIR)/Commander.cpp $(SRC_DIR)/SocketManager.cpp 
SOURCES_JOB_EXECUTOR_SERVER := $(SRC_DIR)/JobExecutorServer.cpp $(SRC_DIR)/Server.cpp $(SRC_DIR)/SocketManager.cpp
SOURCES_PROG_DELAY := $(TESTS_DIR)/progDelay.c

# Object files for each executable
OBJECTS_JOB_COMMANDER := $(SOURCES_JOB_COMMANDER:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
OBJECTS_JOB_EXECUTOR_SERVER := $(SOURCES_JOB_EXECUTOR_SERVER:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
OBJECTS_PROG_DELAY := $(SOURCES_PROG_DELAY:$(TESTS_DIR)/%.c=$(BUILD_DIR)/%.o)

# Dependency files for each executable
DEPS := $(OBJECTS_JOB_COMMANDER:.o=.d) $(OBJECTS_JOB_EXECUTOR_SERVER:.o=.d)

# Default target
all: $(EXEC_JOB_COMMANDER) $(EXEC_JOB_EXECUTOR_SERVER) $(EXEC_PROG_DELAY)

# Build rules for JobCommander
$(EXEC_JOB_COMMANDER): $(OBJECTS_JOB_COMMANDER) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Build rules for JobExecutorServer
$(EXEC_JOB_EXECUTOR_SERVER): $(OBJECTS_JOB_EXECUTOR_SERVER) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Build rules for progDelay
$(EXEC_PROG_DELAY): $(OBJECTS_PROG_DELAY) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Generic rule for building C++ objects
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

# Generic rule for building C objects
$(BUILD_DIR)/%.o: $(TESTS_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# Create build and bin directories if they don't exist
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

# Include dependency files
-include $(DEPS)

# Clean
clean:
	rm -f $(EXEC_JOB_COMMANDER) $(EXEC_JOB_EXECUTOR_SERVER) $(EXEC_PROG_DELAY)
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d
	rm -f $(RUN_FILES) $(TEMP_FILES)
	rm -f $(BIN_DIR)/*

.PHONY: all clean
