HEADERS = src/cold.h src/interpreter.h src/solver.h src/compiler.h

default: solver

pre-build:
	mkdir -p obj bin

cold.o: src/cold.c $(HEADERS)
	gcc -c src/cold.c -o obj/cold.o

interpreter.o: src/interpreter.c $(HEADERS)
	gcc -c src/interpreter.c -o obj/interpreter.o

solver.o: src/solver.c $(HEADERS)
	gcc -c src/solver.c -o obj/solver.o

compiler.o: src/compiler.c $(HEADERS)
	gcc -c src/compiler.c -o obj/compiler.o

solver: pre-build solver.o interpreter.o cold.o
	gcc obj/solver.o obj/interpreter.o obj/cold.o -o bin/solver

coldc: pre-build cold.o interpreter.o compiler.o
	gcc obj/compiler.o obj/cold.o obj/interpreter.o -o bin/coldc

clean:
	rm -rf obj bin
