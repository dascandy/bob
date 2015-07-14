#!/bin/sh
mkdir bin
mkdir obj
mkdir dep
g++ -pthread -MMD -O3 -Wall -Wextra -fno-omit-frame-pointer -std=c++11 -g -Iinclude -c -o obj/bob.o  src/bob.cpp
g++ -pthread -MMD -O3 -Wall -Wextra -fno-omit-frame-pointer -std=c++11 -g -Iinclude -c -o obj/Rule.o  src/Rule.cpp
g++ -pthread -MMD -O3 -Wall -Wextra -fno-omit-frame-pointer -std=c++11 -g -Iinclude -c -o obj/File.o src/File.cpp
g++ -pthread -MMD -O3 -Wall -Wextra -fno-omit-frame-pointer -std=c++11 -g -Iinclude -c -o obj/Replace.o src/Replace.cpp
g++ -pthread -MMD -O3 -Wall -Wextra -fno-omit-frame-pointer -std=c++11 -g -Iinclude -c -o obj/RuleInstance.o src/RuleInstance.cpp
g++ -pthread -MMD -O3 -Wall -Wextra -fno-omit-frame-pointer -std=c++11 -g -Iinclude -c -o obj/String.o src/String.cpp
g++ -pthread -o bin/bob obj/bob.o obj/Rule.o obj/File.o obj/Replace.o obj/RuleInstance.o obj/String.o -lboost_filesystem -lboost_system -lre2

