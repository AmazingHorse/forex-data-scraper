# Author: Bill Heughan (@amazinghorse)
# TODO: 
# - Can we use something other than VPATHS?

# Recursive search function
rwildcard=$(wildcard $(addsuffix $2, $1)) $(foreach d,$(wildcard $(addsuffix *, $1)),$(call rwildcard,$d/,$2))

# Set compiler and flags
CXX=g++
CXXFLAGS=-Wall -Wno-switch -g

# Define directories
ROOT_DIR=lib/tws-api/source/CppClient
BIN_DIR:=obj
INC_DIR=include
SRC_DIR=src
OPEN_SSL_DIR=/usr/local/openssl
BASE_SRC_DIR=${ROOT_DIR}/Posix

# Collect all source files
TWS_SRC := $(BASE_SRC_DIR)/EClientSocketBase.cpp $(BASE_SRC_DIR)/EPosixClientSocket.cpp
SRC := $(call rwildcard,$(SRC_DIR)/,*.cpp) ${TWS_SRC} 

# Add all includes
INCLUDES=-I${ROOT_DIR}/Shared/ -I${BASE_SRC_DIR} -I${INC_DIR} -I${OPEN_SSL_DIR}/include

# Add all libraries
LIBS=-L${OPEN_SSL_DIR}/lib -lssl -lcrypto -ldl

# Combine all cflags
ALL_CFLAGS=$(CXXFLAGS) $(INCLUDES)

# Specify objects from list of source files
SRC_FILES := $(notdir $(SRC))
SRC_DIRS := $(dir $(SRC))
VPATH := $(SRC_DIRS)
OBJ := $(patsubst %.cpp,$(BIN_DIR)/%.o,$(SRC_FILES))

# Specify target
BINARY=${BIN_DIR}/ConnectionVerifyTest

$(BINARY): $(OBJ)
	$(CXX) $(ALL_CFLAGS) -o $(BINARY) $(OBJ) $(LIBS)

$(OBJ): $(BIN_DIR)/%.o: %.cpp
	$(CXX) $(ALL_CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(BIN_DIR)/*.o

