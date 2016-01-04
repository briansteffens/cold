HEADERS = src/cold.h src/interpreter.h src/solver.h src/compiler.h

default: cli

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

cli.o: src/cli.c $(HEADERS)
	gcc -c src/cli.c -o obj/cli.o

cli: pre-build cold.o interpreter.o solver.o compiler.o cli.o
	gcc obj/compiler.o obj/interpreter.o obj/cold.o obj/solver.o obj/cli.o -o bin/cold

clean:
	rm -rf obj bin
