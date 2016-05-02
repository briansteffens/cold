HEADERS = src/cold.h src/interpreter.h src/solver.h src/compiler.h src/permute.h src/general.h src/combiner.h

CCFLAGS = -c
CXXFLAGS =

all: cli

debug: CCFLAGS += -g3
debug: CXXFLAGS += -rdynamic
debug: cli

pre-build:
	mkdir -p obj bin

cold.o: src/cold.c $(HEADERS)
	gcc $(CCFLAGS) src/cold.c -o obj/cold.o

general.o: src/general.c $(HEADERS)
	gcc $(CCFLAGS) src/general.c -o obj/general.o

interpreter.o: src/interpreter.c $(HEADERS)
	gcc $(CCFLAGS) src/interpreter.c -o obj/interpreter.o

solver.o: src/solver.c $(HEADERS)
	gcc $(CCFLAGS) src/solver.c -o obj/solver.o

compiler.o: src/compiler.c $(HEADERS)
	gcc $(CCFLAGS) src/compiler.c -o obj/compiler.o

permute.o: src/permute.c $(HEADERS)
	gcc $(CCFLAGS) src/permute.c -o obj/permute.o

combiner.o: src/combiner.c $(HEADERS)
	gcc $(CCFLAGS) src/combiner.c -o obj/combiner.o

cli.o: src/cli.c $(HEADERS)
	gcc $(CCFLAGS) src/cli.c -o obj/cli.o

cli: pre-build cold.o interpreter.o solver.o compiler.o permute.o general.o cli.o combiner.o
	gcc $(CXXFLAGS) obj/compiler.o obj/interpreter.o obj/cold.o obj/solver.o obj/permute.o obj/general.o obj/cli.o obj/combiner.o -lm -pthread -o bin/cold

clean:
	rm -rf obj bin
