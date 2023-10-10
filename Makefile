CXX = gcc
CXXFLAGS = -g -Wall
OBJECTS = elevator.o
DEPENDS = ${OBJECTS:.o=.d}
EXEC = simulation

.PHONY: all clean run

all: ${EXEC}

${EXEC}: ${OBJECTS}
	${CXX} ${CXXFLAGS} $^ -o $@

clean:
	rm -f *.o *.d ${EXEC}

run: ${EXEC}
	./${EXEC}
