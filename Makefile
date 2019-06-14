all: server.out client.out

client.out: client.cpp utils.cpp
	c++ -fsanitize=address -pedantic -Wall -std=c++11 client.cpp -o client.out

server.out: server.cpp utils.cpp
	c++ -fsanitize=address -pedantic -Wall -std=c++11 server.cpp -o server.out

clean:
	rm *.out