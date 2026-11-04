# Host-side unit tests for the portable core (no PlatformIO toolchain needed).
CXX ?= c++
SRC = ../src/Sha256.cpp ../src/Rsa.cpp ../src/Jwt.cpp ../src/Access.cpp ../src/BlacklistSet.cpp ../src/SyncParser.cpp \
      ../src/TimeUtil.cpp ../src/Journal.cpp ../src/Ndef.cpp ../src/Pn532Frame.cpp ../src/WsFrame.cpp

test:
	$(CXX) -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -o /tmp/doora-test test.cpp $(SRC)
	/tmp/doora-test
	$(CXX) -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Istubs -o /tmp/doora-pn532-test pn532_test.cpp \
	    ../src/Pn532.cpp ../src/Pn532Frame.cpp ../src/Reader.cpp ../src/Ndef.cpp
	/tmp/doora-pn532-test
	$(CXX) -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Istubs -o /tmp/doora-persist-test persist_test.cpp \
	    ../src/Persist.cpp ../src/Journal.cpp ../src/Access.cpp ../src/TimeUtil.cpp ../src/BlacklistSet.cpp ../src/Sha256.cpp
	/tmp/doora-persist-test
