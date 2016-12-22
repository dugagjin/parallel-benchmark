#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <chrono>

#ifdef _WIN32
	#define WINPAUSE system("pause")
#endif

#define arrayLength 1000000
#define maxRandomValue 1000

using namespace std;
using namespace chrono;

auto initializeArray = []() {
	auto* array = new int[arrayLength];
	for (auto i = 0; i < arrayLength; i++) {
		array[i] = rand() % maxRandomValue + 1;
	}
	return array;
};

auto sequentialSum = [](auto array) {
	auto sum = 0;
	for (auto i = 0; i < arrayLength; i++) {
		sum += array[i];
	}
	return sum;
};

auto calculateTime = [](auto function, auto array) {
	auto begin = steady_clock::now();
	auto sum = function(array);
	auto end = steady_clock::now();
	cout << "required time: " << duration_cast<microseconds>(end - begin).count() << " us\nsum: " << sum << endl;
};

int main() {
	auto* array = initializeArray();
	calculateTime(sequentialSum, array);
	WINPAUSE;
	return 0;
}
