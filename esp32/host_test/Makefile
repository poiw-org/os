# Host-side unit tests for the portable core (no PlatformIO toolchain needed).
CXX ?= c++
SRC = ../src/Sha256.cpp ../src/Jwt.cpp ../src/Access.cpp ../src/BlacklistSet.cpp ../src/SyncParser.cpp \
      ../src/TimeUtil.cpp ../src/Journal.cpp

test:
	$(CXX) -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g -o /tmp/doora-test test.cpp $(SRC)
	/tmp/doora-test
