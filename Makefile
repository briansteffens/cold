HEADERS = src/cold.h src/interpreter.h src/solver.h

default: solver

pre-build:
	mkdir -p obj bin

cold.o: src/cold.c $(HEADERS)
	gcc -c src/cold.c -o obj/cold.o

interpreter.o: src/interpreter.c $(HEADERS)
	gcc -c src/interpreter.c -o obj/interpreter.o

solver.o: src/solver.c $(HEADERS)
	gcc -c src/solver.c -o obj/solver.o

solver: pre-build solver.o interpreter.o cold.o
	gcc obj/solver.o obj/interpreter.o obj/cold.o -o bin/solver

clean:
	rm -rf obj bin
