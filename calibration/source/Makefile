ALL=calibration

all: ${ALL}

CFLAGS+=-g -Wall -pedantic -std=c++11
LDFLAGS+=$(shell pkg-config --libs opencv)

calibration: calibration.cpp
	g++ ${CFLAGS} -o $@ $< ${LDFLAGS}

clean:
	rm ${ALL}

