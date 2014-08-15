CXX=g++
CXXFLAGS=-Wall -Wno-switch -g
ROOT_DIR=lib/tws-api/source/CppClient
OBJ_DIR=obj
INC_DIR=include
SRC_DIR=src
OPEN_SSL_DIR=/usr/local/openssl
BASE_SRC_DIR=${ROOT_DIR}/Posix
INCLUDES=-I${ROOT_DIR}/Shared/ -I${BASE_SRC_DIR} -I${INC_DIR} -I${OPEN_SSL_DIR}/include
TARGET=ConnectionVerifyTest

$(TARGET):
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o ${OBJ_DIR}/EClientSocketBase.o -c $(BASE_SRC_DIR)/EClientSocketBase.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o ${OBJ_DIR}/EPosixClientSocket.o -c $(BASE_SRC_DIR)/EPosixClientSocket.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o ${OBJ_DIR}/ConnectionVerifyTest.o -c ConnectionVerifyTest.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o ${OBJ_DIR}/Main.o -c Main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ EClientSocketBase.o EPosixClientSocket.o ConnectionVerifyTest.o Main.o -L${OPEN_SSL_DIR}/lib -lssl -lcrypto -ldl

clean:
	rm -f $(TARGET) *.o

