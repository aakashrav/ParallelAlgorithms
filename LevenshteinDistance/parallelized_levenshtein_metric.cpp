#include <iostream>
#include <thread>
#include "string.h"
#include <algorithm>
#include <string>
#include <future>
#include <vector>
#include <time.h>
#include <stdio.h>
#include <atomic>
#include <mutex>
#include <math.h>

using namespace std;

// #define SERIAL
#define PARALLEL

#ifdef SERIAL

int
return_distance(const std::string& s1, const std::string& s2)
{
	const std::size_t len1 = s1.size(), len2 = s2.size();
	std::vector<std::vector<unsigned int>> d(len1 + 1, std::vector<unsigned int>(len2 + 1));

	d[0][0] = 0;
	for(unsigned int i = 1; i <= len1; ++i) d[i][0] = i;
	for(unsigned int i = 1; i <= len2; ++i) d[0][i] = i;

	for(unsigned int i = 1; i <= len1; ++i)
		for(unsigned int j = 1; j <= len2; ++j)
                      d[i][j] = std::min({ d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1) });
    return d[len1][len2];
}

int 
main(int argc, char * argv[])
{
	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	int result;
	result = return_distance(argv[1], argv[2]);
	std::chrono::steady_clock::time_point end= std::chrono::steady_clock::now();
	std::cout << result << " " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << std::endl;
}
#endif

#ifdef PARALLEL

std::vector<vector<long>> DISTANCE_MATRIX;

int GRANULARITY;

void
usage()
{
	std::cout << "Usage levenshtein_distance <word_one> <word_two> <num_threads_in_diagonal>" << std::endl;
}

void 
perform_matrix_computations(const std::string str, const std::string str2, std::vector<long *> data_structure, std::pair<int, int> starting_point, std::promise<int> && p)
{
	std::pair<int, int> current_point = starting_point;

	for (int i=0; i < data_structure.size(); i++)
	{

		long min_val;
		if ( (current_point.first == 0) && (current_point.second > 0) )
			min_val = current_point.second;
		else if ((current_point.first > 0) && (current_point.second == 0) )
			min_val = current_point.first;
		else
		{
			min_val = std::min({ DISTANCE_MATRIX[current_point.first - 1][current_point.second] +1, DISTANCE_MATRIX[current_point.first][current_point.second - 1] + 1, DISTANCE_MATRIX[current_point.first - 1][current_point.second - 1] + (str[current_point.first- 1] == str2[current_point.second - 1] ? 0 : 1) });
		}

		*(data_structure[i]) = min_val;
		current_point.first = current_point.first-1;
		current_point.second = current_point.second +1;
	}
	
	// Notify the futures that we are finished
	p.set_value(1);
}


int
main(int argc, char * argv[])
{
	if (argc <  4)
	{
		usage();
		return 1;
	}

	const std::string word_one = argv[1];
	const std::string word_two = argv[2];
	GRANULARITY = atoi(argv[3]);

	for (int i=0; i < word_one.size()+1; i++)
	{
		DISTANCE_MATRIX.push_back(std::vector<long>());
		for (int j=0; j < word_two.size()+1; j++)
		{
			DISTANCE_MATRIX[i].push_back(0);
		}
	}


	/* Placeholder variable */
	int result = 0;


	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	DISTANCE_MATRIX[0][0] = 0;
	atomic_thread_fence(std::memory_order_release);

	/* Create the first half */
	for (int i=1; i <= word_one.size(); i++)
	{
		/* Keep a vector of futures */
		std::vector<std::future<int>> levenshtein_futures;

		int current_row = i;
		int current_column = 0;
		std::vector<long *> diagonal_row;
		while ( (current_row >= 0) && (current_column <= word_two.size()) )
		{
			diagonal_row.push_back(&(DISTANCE_MATRIX[current_row][current_column]));
			current_row--;
			current_column++;
		}

		int GRANULARITY_SIZE = diagonal_row.size()/GRANULARITY;
		for (int current_thread=0; current_thread < GRANULARITY; current_thread++)
		{
			std::pair<int, int> matrix_position = std::make_pair<int,int>(i-(GRANULARITY_SIZE * current_thread),0 + (GRANULARITY_SIZE * current_thread));

			std::vector<long *> thread_diagonal_row;
			if (current_thread == GRANULARITY-1)
				thread_diagonal_row = std::vector<long *>(diagonal_row.begin() + (GRANULARITY_SIZE * current_thread), diagonal_row.end());
			else
				thread_diagonal_row = std::vector<long *>(diagonal_row.begin() + (GRANULARITY_SIZE * current_thread), diagonal_row.begin() + (GRANULARITY_SIZE * (current_thread+1)));

			std::promise<int> p;
			levenshtein_futures.push_back(p.get_future());

			// Start the thread
			std::thread th(perform_matrix_computations,word_one,word_two,thread_diagonal_row,matrix_position,std::move(p));
			th.detach();
		}

		// Here we only wait for one future for a specified amount of time, and move on to the next one
		// Better than waiting for a single future endlessly and only when it finishes moving on
		size_t running = levenshtein_futures.size();
		int cur = 0;
		// While we have only more than one future left
		while (running > 0)
		{
			std::future<int> &f = levenshtein_futures[cur];

			// Wait for a bit on the current future to see if its ready
			// If its still not ready then move to the next one.
			auto status = f.wait_for(std::chrono::milliseconds(10));
			if (status == std::future_status::ready)
			{	
				result += f.get();
				levenshtein_futures.erase(levenshtein_futures.begin() + cur);
				running--;
			}
			else
			{
				// Loop back to first future and check again.
				if (cur < running-1)
					cur++;
				else
					cur =0;
			}
		}

		atomic_thread_fence(std::memory_order_acquire);

	}

	// Second part of the diagonalization process
	for (int i=1; i <= word_two.size()-1; i++)
	{
		/* Keep a vector of futures */
		std::vector<std::future<int>> levenshtein_futures;

		int current_row = word_one.size();
		int current_column = i;
		std::vector<long *> diagonal_row;
		while ( (current_column <= word_two.size()) && (current_row >= 0) )
		{
			diagonal_row.push_back(&(DISTANCE_MATRIX[current_row][current_column]));
			current_row--;
			current_column++;
		}

		int GRANULARITY_SIZE = diagonal_row.size()/GRANULARITY;
		for (int current_thread=0; current_thread < GRANULARITY; current_thread++)
		{
			std::pair<int, int> matrix_position = std::make_pair<int,int>(word_one.size()-(GRANULARITY_SIZE * current_thread),i + (GRANULARITY_SIZE * current_thread));
			
			std::vector<long *> thread_diagonal_row;
			if (current_thread == GRANULARITY-1)
				thread_diagonal_row = std::vector<long *>(diagonal_row.begin() + (GRANULARITY_SIZE * current_thread), diagonal_row.end());
			else
				thread_diagonal_row = std::vector<long *>(diagonal_row.begin() + (GRANULARITY_SIZE * current_thread), diagonal_row.begin() + (GRANULARITY_SIZE * (current_thread+1)));

			std::promise<int> p;
			levenshtein_futures.push_back(p.get_future());

			// Start the thread
			std::thread th(perform_matrix_computations,word_one,word_two,thread_diagonal_row,matrix_position,std::move(p));
			th.detach();
		}

		// Here we only wait for one future for a specified amount of time, and move on to the next one
		// Better than waiting for a single future endlessly and only when it finishes moving on
		size_t running = levenshtein_futures.size();
		int cur = 0;
		// While we have only more than one future left
		while (running > 0)
		{
			std::future<int> &f = levenshtein_futures[cur];

			// Wait for a bit on the current future to see if its ready
			// If its still not ready then move to the next one.
			auto status = f.wait_for(std::chrono::milliseconds(10));
			if (status == std::future_status::ready)
			{	
				result += f.get();
				levenshtein_futures.erase(levenshtein_futures.begin() + cur);
				running--;
			}
			else
			{
				// Loop back to first future and check again.
				if (cur < running-1)
					cur++;
				else
					cur =0;
			}
		}

		atomic_thread_fence(std::memory_order_acquire);
	}

	atomic_thread_fence(std::memory_order_acquire);

	// Fill in the final value.
	DISTANCE_MATRIX[word_one.size()][word_two.size()] = 
	std::min({ DISTANCE_MATRIX[word_one.size() - 1][word_two.size()] + 1, DISTANCE_MATRIX[word_one.size()][word_two.size() - 1] + 1, DISTANCE_MATRIX[word_one.size() - 1][word_two.size() - 1] + (word_one[word_one.size() - 2] == word_two[word_two.size() - 2] ? 0 : 1) });

	atomic_thread_fence(std::memory_order_acquire);

	std::chrono::steady_clock::time_point end= std::chrono::steady_clock::now();
	std::cout << DISTANCE_MATRIX[word_one.size()][word_two.size()] << " Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << std::endl;
}

#endif