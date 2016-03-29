SOURCES=parallelized_levenshtein_metric.cpp
CC=g++
CFLAGS=-std=c++11 -pthread

all: levenshtein_distance

levenshtein_distance: $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)
clean:
	rm levenshtein_distance