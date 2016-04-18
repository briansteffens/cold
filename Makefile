HEADERS = src/cold.h src/interpreter.h src/solver.h src/compiler.h src/generator.h

default: cli

pre-build:
	mkdir -p obj bin

cold.o: src/cold.c $(HEADERS)
	gcc -c -g3 src/cold.c -o obj/cold.o

interpreter.o: src/interpreter.c $(HEADERS)
	gcc -c -g3 src/interpreter.c -o obj/interpreter.o

solver.o: src/solver.c $(HEADERS)
	gcc -c -g3 src/solver.c -o obj/solver.o

compiler.o: src/compiler.c $(HEADERS)
	gcc -c -g3 src/compiler.c -o obj/compiler.o

generator.o: src/generator.c $(HEADERS)
	gcc -c -g3 src/generator.c -o obj/generator.o

cli.o: src/cli.c $(HEADERS)
	gcc -c -g3 src/cli.c -o obj/cli.o

cli: pre-build cold.o interpreter.o solver.o compiler.o generator.o cli.o
	gcc obj/compiler.o obj/interpreter.o obj/cold.o obj/solver.o obj/generator.o obj/cli.o -lm -o bin/cold

clean:
	rm -rf obj bin
