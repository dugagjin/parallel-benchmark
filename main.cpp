#include <iostream>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <map>

#include <stdlib.h>
#include <windows.h>

#pragma comment(lib, "user32.lib")
#define WINPAUSE system("pause")


using namespace std;
using namespace chrono;



auto add = [](auto name) {
	map<string, function<unique_ptr<float[]>(float* &, float* &, int)>> methods;
	methods["classic"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] + array2[i];
		}
		return arrayOut;
	};
	methods["autoparallel"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		#pragma loop(ivdep)
		#pragma loop(hint_parallel(4))
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] + array2[i];
		}
		return arrayOut;
	};
	methods["autovector"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		#pragma loop(ivdep)
		#pragma loop(no_vector)
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] + array2[i];
		}
		return arrayOut;
	};
	methods["openmp"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		#pragma omp parallel for 
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] + array2[i];
		}
		return arrayOut;
	};
	methods["simd"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		const int aligned = arrayLength - arrayLength % 4;
		for (auto i = 0; i < aligned; i += 4) {
			_mm_storeu_ps(&arrayOut[i], _mm_add_ps(_mm_loadu_ps(&array1[i]), _mm_loadu_ps(&array2[i])));
		}
		for (auto i = aligned; i < arrayLength; ++i) {
			arrayOut[i] = array1[i] + array2[i];
		}
		return arrayOut;
	};
	methods["threads"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		auto numberOfThreads = 4, elementsPerThread = (arrayLength / numberOfThreads);
		auto add = [](auto* array1, auto* array2, auto* array3, auto max) {
			for (auto i = 0; i < max; i++) {
				array3[i] = array1[i] + array2[i];
			}
		};
		vector<thread*> threads;
		for (auto i = 0; i < numberOfThreads; i++) {
			threads.push_back(
				new thread(
					add,
					array1 + i * elementsPerThread,
					array2 + i * elementsPerThread,
					arrayOut.get() + i * elementsPerThread,
					elementsPerThread
				)
			);
		}
		for (auto thread : threads) {
			thread->join();
			delete thread;
		}
		return arrayOut;
	};
	return methods[name];
};

auto mul = [](auto name) {
	map<string, function<unique_ptr<float[]>(float* &, float* &, int)>> methods;
	methods["classic"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] * array2[i];
		}
		return arrayOut;
	};
	methods["autoparallel"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		#pragma loop(ivdep)
		#pragma loop(hint_parallel(4))
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] * array2[i];
		}
		return arrayOut;
	};
	methods["autovector"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		#pragma loop(ivdep)
		#pragma loop(no_vector)
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] * array2[i];
		}
		return arrayOut;
	};
	methods["openmp"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		#pragma omp parallel for 
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] * array2[i];
		}
		return arrayOut;
	};
	methods["simd"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		const int aligned = arrayLength - arrayLength % 4;
		for (auto i = 0; i < aligned; i += 4) {
			_mm_storeu_ps(&arrayOut[i], _mm_mul_ps(_mm_loadu_ps(&array1[i]), _mm_loadu_ps(&array2[i])));
		}
		for (auto i = aligned; i < arrayLength; ++i) {
			arrayOut[i] = array1[i] * array2[i];
		}
		return arrayOut;
	};
	methods["threads"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		auto numberOfThreads = 4, elementsPerThread = (arrayLength / numberOfThreads);
		auto add = [](auto* array1, auto* array2, auto* array3, auto max) {
			for (auto i = 0; i < max; i++) {
				array3[i] = array1[i] * array2[i];
			}
		};
		vector<thread*> threads;
		for (auto i = 0; i < numberOfThreads; i++) {
			threads.push_back(
				new thread(
					add,
					array1 + i * elementsPerThread,
					array2 + i * elementsPerThread,
					arrayOut.get() + i * elementsPerThread,
					elementsPerThread
				)
			);
		}
		for (auto thread : threads) {
			thread->join();
			delete thread;
		}
		return arrayOut;
	};
	return methods[name];
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
	auto array = new float[arrayLength];
	for (auto i = 0; i < arrayLength; i++) {
		array[i] = rand() % maxRandomValue + 1;
	}
	return array;
};

auto calculateTime = [](auto name, auto &sumFunction, auto &array1, auto &array2, auto arrayLength, bool debug = false) {
	auto begin = steady_clock::now();
	auto output = sumFunction(array1, array2, arrayLength);
	auto end = steady_clock::now();
	if (debug) {
		cout << name << " function done in " << duration_cast<microseconds>(end - begin).count() << " ns. " << endl;
		cout << array1[0] << " + " << array2[0] << " = " << output[0] << endl;
	}
	return duration_cast<microseconds>(end - begin).count();
};



int main() {
	auto arrayLength = 1'048'576, randomValueMax = 1'024;
	cout << "Generating array ... ";
	auto arrayA = initializeRandomArray(arrayLength, randomValueMax);
	auto arrayB = initializeRandomArray(arrayLength, randomValueMax);
	cout << " done ! " << endl;
	printSystemInfo();	
	vector<string> methods = { "classic", "autoparallel", "autovector", "openmp", "simd", "threads" };

	for_each(methods.begin(), methods.end(), [&arrayA, &arrayB, &arrayLength](auto method) {
		vector<long long> measurements;
		for (auto i = 0; i < 200; i++) {
			measurements.push_back(calculateTime(method, mul(method), arrayA, arrayB, arrayLength));
		}
		sort(measurements.begin(), measurements.end());
		auto median = measurements[measurements.size() / 2];
		long long total = 0;
		for (auto& measurement : measurements) {
			total += measurement;
		}
		auto average = total / measurements.size();

		auto standardDeviation = 0;
		for (auto &measurement : measurements){
			standardDeviation += pow(measurement - average, 2);
		}
		standardDeviation = sqrt(standardDeviation / measurements.size());
		cout << method << ":\naverage: " << average << "\t median: " << median << "\t min: " << measurements.at(0) << "\t max: " << measurements.at(measurements.size() - 1) << "\t standard deviation: " << standardDeviation << endl;
	});

	WINPAUSE;
	return 0;
}
