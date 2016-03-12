#include <iostream>
#include <thread>
#include "string.h"
#include <algorithm>
#include <string>
#include <future>
#include <vector>
#include <time.h>
#include <stdio.h>

using namespace std;

// #define SERIAL
#define PARALLEL

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

#ifdef SERIAL
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

void 
levenshtein_promise_handler(const std::string & str, const std::string & str2, std::promise<int> p)
{
	int distance;
	distance = return_distance(str, str2);
	p.set_value(distance);
}

/* Set the granularity of the parallel program - how many threads do we have for processing the data*/
const int GRANULARITY = 2;

int
main(int argc, char * argv[])
{
	const std::string word_one = argv[1];
	const std::string word_two = argv[2];
	const int maximum_length = std::max(word_one.size(), word_two.size());

	/* Find the length of the substrings of the words that correspond to our particular granularity setting*/
	int TASK_SUBSTRING_LENGTH = maximum_length/GRANULARITY;

	std::vector<std::string> word_one_substrings;
	std::vector<std::string> word_two_substrings;

	/* Generate substrings of the two words to fit the particular granularity constraints */
	for (int i=0; i < GRANULARITY; i++)
	{
		std::string word_substring;

		/* Handle cases where we go above the word's maximum indice */
		if (i * TASK_SUBSTRING_LENGTH > word_one.size()-1)
			word_substring = "";
		else if ((i+1) * (TASK_SUBSTRING_LENGTH) <= word_one.size() - 1)
            word_substring = word_one.substr(i * TASK_SUBSTRING_LENGTH,(i+1) * TASK_SUBSTRING_LENGTH);
        else
        	word_substring = word_one.substr(i * TASK_SUBSTRING_LENGTH,word_one.size()-1);

        word_one_substrings.push_back(word_substring);
	}

	for (int i=0; i < GRANULARITY; i++)
	{
		std::string word_substring;

		/* Handle cases where we go above the word's maximum indice */
		if (i * TASK_SUBSTRING_LENGTH > word_two.size()-1)
			word_substring = "";
		else if ((i+1) * (TASK_SUBSTRING_LENGTH) <= word_two.size() - 1)
            word_substring = word_two.substr(i * TASK_SUBSTRING_LENGTH,(i+1) * TASK_SUBSTRING_LENGTH);
        else
        	word_substring = word_two.substr(i * TASK_SUBSTRING_LENGTH,word_two.size()-1);

        word_two_substrings.push_back(word_substring);
	}

	/* Store the final result in this variable */
	int result = 0;


	/* Keep a vector of futures */
	std::vector<std::future<int>> levenshtein_futures;

	/* Generate GRANULARITY number of futures and start the threads */
	for (int i=0; i < GRANULARITY; i++)
	{
		/*
		You should get future from the promise before the move, otherwise it 
		would not be accessible from the main thread after the move.
		*/
		std::promise<int> p;
		levenshtein_futures.push_back(p.get_future());

		/* Create the thread for particular substrings of the words*/
		std::cout << "ABOUT TO CREATE THREAD" << std::endl;
		std::thread th(levenshtein_promise_handler, word_one_substrings[i], word_two_substrings[i], std::move(p));
		std::cout << "CREATED THREAD" << std::endl;
	}


	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	// Here we only wait for one future for a specified amount of time, and move on to the next one
	// Better than waiting for a single future endlessly and only when it finishes moving on
	size_t running = GRANULARITY;
	int cur = 0;
	// While we have only more than one future left
	while (running > 1)
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

	std::chrono::steady_clock::time_point end= std::chrono::steady_clock::now();

	std::cout << result << " Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << std::endl;
}
#endif