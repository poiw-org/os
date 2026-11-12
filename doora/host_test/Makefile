# Host-side unit tests for the portable core (no PlatformIO toolchain needed).
CXX ?= c++
CC ?= cc
FLAGS = -Wall -Wextra -Werror -fsanitize=address,undefined -g
# micro-ecc, built the way the firmware builds it (vendor/micro-ecc/ecc_p256.c).
ECC = /tmp/doora-ecc.o
SRC = ../src/Sha256.cpp ../src/Rsa.cpp ../src/Jwt.cpp ../src/Access.cpp ../src/BlacklistSet.cpp ../src/SyncParser.cpp \
      ../src/TimeUtil.cpp ../src/Journal.cpp ../src/Ndef.cpp ../src/Pn532Frame.cpp ../src/WsFrame.cpp \
      ../src/P256.cpp ../src/BleProto.cpp $(ECC)

test:
	$(CC) -std=c11 $(FLAGS) -Wno-implicit-fallthrough -I../vendor/micro-ecc -c -o $(ECC) ../vendor/micro-ecc/ecc_p256.c
	$(CXX) -std=c++17 $(FLAGS) -I../vendor/micro-ecc -o /tmp/doora-test test.cpp $(SRC)
	/tmp/doora-test
	$(CXX) -std=c++17 $(FLAGS) -I../vendor/micro-ecc -o /tmp/doora-ble-test ble_test.cpp $(SRC) ../src/BridgeLink.cpp
	/tmp/doora-ble-test
	$(CXX) -std=c++17 $(FLAGS) -Istubs -o /tmp/doora-pn532-test pn532_test.cpp \
	    ../src/Pn532.cpp ../src/Pn532Frame.cpp ../src/Reader.cpp ../src/Ndef.cpp
	/tmp/doora-pn532-test
	$(CXX) -std=c++17 $(FLAGS) -I../vendor/micro-ecc -Istubs -o /tmp/doora-persist-test persist_test.cpp \
	    ../src/Persist.cpp ../src/Journal.cpp ../src/Access.cpp ../src/TimeUtil.cpp ../src/BlacklistSet.cpp ../src/Sha256.cpp \
	    ../src/P256.cpp ../src/BleProto.cpp $(ECC)
	/tmp/doora-persist-test
