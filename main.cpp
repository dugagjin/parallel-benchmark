#include <iostream>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <numeric>

#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <list>
#pragma comment(lib, "user32.lib")

#ifdef _WIN32
#define WINPAUSE system("pause")
#endif

using namespace std;
using namespace chrono;

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

int arrayLength;
auto initializeRandomArray= [](auto length, auto maxRandomValue) -> auto* {
	arrayLength = length;
	auto array = new int[arrayLength];
	for (auto i = 0; i < arrayLength; i++) {
		array[i] = rand() % maxRandomValue + 1;
	}
	return array;
};

auto calculateTime = [](auto name, auto &function, auto &array) {
	auto begin = steady_clock::now();
	auto sum = function(array);
	auto end = steady_clock::now();
	cout << name << " function done in " << duration_cast<microseconds>(end - begin).count() << " ns.";
	cout << " The sum is: " << sum << endl;
};

class Methods {
public:
	static auto Sum() {
		vector<pair<string, function<int(int* &)>>> functions = {
			make_pair("default", [](auto &array) {
				auto sum = 0;
				for (auto i = 0; i < arrayLength; i++) {
					sum += array[i];
				}
				return sum;
			}),
			make_pair("auto parallel", [](auto &array) {
				auto sum = 0;
				#pragma loop(ivdep)
				#pragma loop(hint_parallel(4))
				for (auto i = 0; i < arrayLength; i++) {
					sum += array[i];
				}
				return sum;
			}),
			make_pair("no vector", [](auto &array) {
				auto sum = 0;
				#pragma loop(ivdep)
				#pragma loop(no_vector)
				for (auto i = 0; i < arrayLength; i++) {
					sum += array[i];
				}
				return sum;
			}),
			make_pair("openMP", [](auto &array) {
				auto sum = 0;
				#pragma omp parallel for reduction(+:sum)
				for (auto i = 0; i < arrayLength; i++) {
					sum += array[i];
				}
				return sum;
			}),
			make_pair("SIMD", [](auto array) {
				/* array needs to be 16 byte aligned, Thats why it does not work */
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
			}),
			make_pair("threads", [](auto &array) {
				auto sum = 0, numberOfThreads = 8, elementsPerThread = (arrayLength / numberOfThreads);
				auto accumulator = [](auto* array, auto max) {
					auto sum = 0;
					for (auto i = 0; i < max; i++) {
						sum += array[i];
					}
					return sum;
				};
				vector<future<int>> threads;
				for(auto i = 0; i < numberOfThreads; i++) {
					threads.push_back(async(launch::async, accumulator, array + i * elementsPerThread, elementsPerThread));
					sum += threads.at(i).get();
				}
				return sum;
			}),
		};
		return functions;
	}
};

int main() {
	printSystemInfo();	
	auto arrayOfRandomNumbers = initializeRandomArray(262'144, 1);
	for(auto &f : Methods::Sum()) {
		calculateTime(f.first, f.second, arrayOfRandomNumbers);
	}
	WINPAUSE;
	return 0;
}
