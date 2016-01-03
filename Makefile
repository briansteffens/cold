HEADERS = cold.h interpreter.h solver.h

default: solver

cold.o: cold.c $(HEADERS)
	gcc -c cold.c -o cold.o

interpreter.o: interpreter.c $(HEADERS)
	gcc -c interpreter.c -o interpreter.o

solver.o: solver.c $(HEADERS)
	gcc -c solver.c -o solver.o

interpreter: interpreter.o cold.o
	gcc interpreter.o cold.o -o interpreter

solver: solver.o interpreter.o cold.o
	gcc solver.o interpreter.o cold.o -o solver

clean:
	-rm -f cold.o
	-rm -f interpreter.o
	-rm -f solver.o
	-rm -f interpreter
	-rm -f solver
