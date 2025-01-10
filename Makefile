
scrapercom: scrapercommunicator.o shared_stuff.o socket_process.o fifo_process.o 
	g++ scrapercommunicator.o shared_stuff.o socket_process.o fifo_process.o -o scrapercom -lncurses

scrapercommunicator.o: scrapercommunicator.cpp socket_process.o fifo_process.o
	g++ scrapercommunicator.cpp -Wall -g -c -o scrapercommunicator.o

shared_stuff.o: shared_stuff.cpp 
	g++ shared_stuff.cpp -g -c -o shared_stuff.o

socket_process.o: socket_process.cpp 
	g++ socket_process.cpp -g -c -o socket_process.o

fifo_process.o: fifo_process.cpp 
	g++ fifo_process.cpp -g -c -o fifo_process.o

clean:
	rm *.o
	rm *.out