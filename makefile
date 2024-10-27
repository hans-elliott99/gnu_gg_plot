
SRC=main.cpp


main:
	g++ -std=c++17 -pipe -g -Wall -Wextra -Wpedantic -o main $(SRC) 

clean:
	rm main