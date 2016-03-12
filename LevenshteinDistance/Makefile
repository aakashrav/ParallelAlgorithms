# SOURCES=parallelized_levenshtein_metric.cpp
# OBJECTS=$(SOURCES:.cpp=.out)
# CC=g++
# CFLAGS=-std=c++11 -pthread

# all: levenshtein_distance

# .cpp.out:
# 	$(CC) $(CFLAGS) $<

# levenshtein_distance: $(OBJECTS)
# 	$(CC) -o $@ a.out

all: parallelized_levenshtein_metric.cpp
	g++ -std=c++11 -pthread -o main parallelized_levenshtein_metric.cpp

clean:
	rm a.out main