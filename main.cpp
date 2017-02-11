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

#define arrayLength 65536
#define maxRandomValue 10
#define nameVariable(a) #a

using namespace std;
using namespace chrono;


auto getSystemInfo() -> void {
	try {
		LARGE_INTEGER lpFrequency;
		QueryPerformanceFrequency(&lpFrequency);
		SYSTEM_INFO siSysInfo;
		GetSystemInfo(&siSysInfo);
		auto CPUSpeed = float(lpFrequency.QuadPart) / 1000000;
		cout << siSysInfo.dwNumberOfProcessors << " cores at about " << CPUSpeed << " Ghz" << endl << endl;
	} catch (int e) {
		cout << "Could not get systeminfo. Exception #" << e << '\n';
	}
};

auto initializeArray() -> auto* {
	auto* array = new int[arrayLength];
	for (auto i = 0; i < arrayLength; i++) {
		array[i] = rand() % maxRandomValue + 1;
	}
	return array;
};

auto calculateTime = [](auto name, auto function, auto &array) {
	auto begin = steady_clock::now();
	auto sum = function(array);
	auto end = steady_clock::now();
	cout << name << " function done in " << duration_cast<microseconds>(end - begin).count() << " ns.";
	cout << " The sum is: " << sum << endl;
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

auto noVectorSum = [](auto &array) {
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
	const auto numberOfThreads = 2;
	const auto elementsPerThread = arrayLength / numberOfThreads;
	auto sum = 0;
	auto threadSum = [&sum](auto* arrayThread, auto max) -> void {
		for (auto i = 0; i < max; i++) {
			sum += arrayThread[i];
		}
	};
	vector<thread*> threads;
	for (auto i = 0; i < numberOfThreads; i++) {
		threads.push_back(new thread(threadSum, array + i * elementsPerThread, elementsPerThread));
	}
	for (auto t : threads) {
		t->join();
		delete t;
	}
	return sum;
};

int main() {
	getSystemInfo();
	auto* array = initializeArray();
	calculateTime(nameVariable(sequentialSum), sequentialSum, array);
	calculateTime(nameVariable(autoParallelSum), autoParallelSum, array);
	calculateTime(nameVariable(autoVectorSum), noVectorSum, array);
	calculateTime(nameVariable(openMPSum), openMPSum, array);
	calculateTime(nameVariable(multiThreadedSum), multiThreadedSum, array);
	WINPAUSE;
	return 0;
}
