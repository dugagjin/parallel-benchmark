#include <iostream>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <cassert>

#include <stdlib.h>
#include <time.h>
#include <windows.h>
#pragma comment(lib, "user32.lib")

#ifdef _WIN32
	#define WINPAUSE system("pause")
#endif

#define arrayLength 10000000
#define maxRandomValue 10

using namespace std;
using namespace chrono;

auto initializeArray = []() {
	auto* array = new int[arrayLength];
	for (auto i = 0; i < arrayLength; i++) {
		array[i] = rand() % maxRandomValue + 1;
	}
	return array;
};

auto sequentialSum = [](auto &array) {
	auto sum = 0;
	for (auto i = 0; i < arrayLength; i++) {
		sum += array[i];
	}
	return sum;
};

auto autoParallelSum = [](auto &array) {
	auto sum = 0;
	#pragma loop(ivdep)
	#pragma loop(hint_parallel(4))
	for (auto i = 0; i < arrayLength; i++) {
		sum += array[i];
	}
	return sum;
};

auto autoVectorSum = [](auto &array) {
	auto sum = 0;
	#pragma loop(ivdep)
	#pragma loop(no_vector)
	for (auto i = 0; i < arrayLength; i++) {
		sum += array[i];
	}
	return sum;
};

auto openMPSum = [](auto &array) {
	auto sum = 0;
	#pragma omp parallel for reduction(+:sum)
	for (auto i = 0; i < arrayLength; i++) {
		sum += array[i];
	}
	return sum;
};

auto SIMDSum = [](auto &array) {
	auto sum = 0.0f;
	/*
	auto n = 16;
	auto vsum = _mm_set1_ps(0.0f);
	assert((n & 3) == 0);
	assert(((uintptr_t)(array) & 15) == 0);
	for (auto i = 0; i < n; i += 4){
		__m128 v = _mm_load_ps(&array[i]);
		vsum = _mm_add_ps(vsum, v);
	}
	vsum = _mm_hadd_ps(vsum, vsum);
	vsum = _mm_hadd_ps(vsum, vsum);
	_mm_store_ss(&sum, vsum);
	*/

	return static_cast<int>(sum);
	
};

auto multiThreadedSum = [](auto &array) {
	auto sum_left = 0;
	auto sum_right = 0;

	auto threadSum = [](auto &arrayThread, auto min, auto max, auto &sumThread) {
		for (min; min < max; min++) {
			sumThread += arrayThread[min];
		}
	};
	/*
	thread t1(sequentialSum, array, 0, arrayLength / 2, ref(sum_left));
	thread t2(sequentialSum, array, arrayLength / 2, arrayLength, ref(sum_right));

	t1.join();
	t2.join();
	*/
	auto sum = sum_left + sum_right;
	return sum;
};


auto calculateTime = [](auto function, auto &array) {
	auto begin = steady_clock::now();
	auto sum = function(array);
	auto end = steady_clock::now();
	cout << "required time: " << duration_cast<microseconds>(end - begin).count() << " us "; 
	cout << " | sum: " << sum << endl;
};

auto getSystemInfo = [](){
	try {
		LARGE_INTEGER lpFrequency;
		QueryPerformanceFrequency(&lpFrequency);
		auto CPUSpeed = float(lpFrequency.QuadPart) / 1000000;
		SYSTEM_INFO siSysInfo;
		GetSystemInfo(&siSysInfo);
		cout << siSysInfo.dwNumberOfProcessors << " cores at " << CPUSpeed << " Ghz" << endl << endl;
	} catch (int e){
		cout << "Could not get systeminfo. Exception #" << e << '\n';
	}
};

int main() {
	getSystemInfo();
	auto* array = initializeArray();
	calculateTime(sequentialSum, array);
	calculateTime(autoParallelSum, array);
	calculateTime(autoVectorSum, array);
	calculateTime(openMPSum, array);
	calculateTime(SIMDSum, array);
	WINPAUSE;
	return 0;
}
