#include <iostream>
#include <chrono>
#include <string>
#include <future>
#include <vector>

#include <stdlib.h>
#include <windows.h>
#include <map>
#pragma comment(lib, "user32.lib")

#define WINPAUSE system("pause")

using namespace std;
using namespace chrono;

auto sum = []() {
	map<string, function<int(int* &, int)>> methods; 		
	methods["classic"] = [](auto &array, auto arrayLength) {
		auto sum = 0;
		for (auto i = 0; i < arrayLength; i++) {
			sum += array[i];
		}
		return sum;
	};
	methods["autoparallel"] = [](auto &array, auto arrayLength) {
		auto sum = 0;
		#pragma loop(ivdep)
		#pragma loop(hint_parallel(4))
		for (auto i = 0; i < arrayLength; i++) {
			sum += array[i];
		}
		return sum;
	};
	methods ["novector"] = [](auto &array, auto arrayLength) {
		auto sum = 0;
		#pragma loop(ivdep)
		#pragma loop(no_vector)
		for (auto i = 0; i < arrayLength; i++) {
			sum += array[i];
		}
		return sum;
	};
	methods["openmp"] = [](auto &array, auto arrayLength) {
		auto sum = 0;
		#pragma omp parallel for reduction(+:sum)
		for (auto i = 0; i < arrayLength; i++) {
			sum += array[i];
		}
		return sum;
	};
	methods["simd"] = [](auto array, auto arrayLength) {
		/* array needs to be 16 byte aligned, Thats why it does not work */
		/* and have no idea what this means */
		const auto vk0 = _mm_set1_epi8(0);
		const auto vk1 = _mm_set1_epi16(1);
		auto vsum = _mm_set1_epi32(0);
		auto n = arrayLength / 16;
		for (auto i = 0; i < n; i += 16) {
			__m128i v = _mm_loadu_si128(reinterpret_cast<__m128i*>(&array[i]));
			auto vl = _mm_unpacklo_epi8(v, vk0);
			auto vh = _mm_unpackhi_epi8(v, vk0);
			vsum = _mm_add_epi32(vsum, _mm_madd_epi16(vl, vk1));
			vsum = _mm_add_epi32(vsum, _mm_madd_epi16(vh, vk1));
		}
		vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 8));
		vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
		return  _mm_cvtsi128_si32(vsum);
	};
	methods["threads"] = [](auto &array, auto arrayLength) {
		auto sum = 0, numberOfThreads = 8, elementsPerThread = (arrayLength / numberOfThreads);
		auto accumulator = [](auto* array, auto max) {
			auto sum = 0;
			for (auto i = 0; i < max; i++) {
				sum += array[i];
			}
			return sum;
		};
		vector<future<int>> threads;
		for (auto i = 0; i < numberOfThreads; i++) {
			threads.push_back(async(launch::async, accumulator, array + i * elementsPerThread, elementsPerThread));
			sum += threads.at(i).get();
		}
		return sum;
	};
	return methods;
};


auto printSystemInfo = [] {
	try {
		LARGE_INTEGER lpFrequency;
		QueryPerformanceFrequency(&lpFrequency);
		SYSTEM_INFO siSysInfo;
		GetSystemInfo(&siSysInfo);
		cout << siSysInfo.dwNumberOfProcessors << " cores at about " << float(lpFrequency.QuadPart) / 1'000'000 << " Ghz" << endl;
	} catch (exception const& e) {
		cout << "Error: " << e.what() << '\n';
	}
};

auto initializeRandomArray = [](auto arrayLength, auto maxRandomValue) {
	auto array = new int[arrayLength];
	for (auto i = 0; i < arrayLength; i++) {
		array[i] = rand() % maxRandomValue + 1;
	}
	return array;
};

auto calculateTime = [](auto name = "", auto &sumFunction, auto &array, auto arrayLength) {
	auto begin = steady_clock::now();
	auto sum = sumFunction(array, arrayLength);
	auto end = steady_clock::now();
	cout << name << " function done in " << duration_cast<microseconds>(end - begin).count() << " ns.";
	cout << " The sum is: " << sum << endl;
};



int main() {
	auto arrayLength = 268'435'456, randomValueMax = 268'435'456;
	auto arrayOfRandomNumbers = initializeRandomArray(arrayLength, randomValueMax);
	printSystemInfo();	
	auto methods = sum();
	for(auto &method : methods) {
		calculateTime(method.first, method.second, arrayOfRandomNumbers, arrayLength);
	}
	WINPAUSE;
	return 0;
}
