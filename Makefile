HEADERS = cold.h interpreter.h

default: interpreter

cold.o: cold.c $(HEADERS)
	gcc -c cold.c -o cold.o

interpreter.o: interpreter.c $(HEADERS)
	gcc -c interpreter.c -o interpreter.o

interpreter: interpreter.o cold.o
	gcc interpreter.o cold.o -o interpreter

clean:
	-rm -f cold.o
	-rm -f interpreter.o
	-rm -f interpreter
