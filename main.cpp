#include <iostream>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <map>
#include <numeric>
#include <limits>
#include <fstream>

#include <stdlib.h>
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX

#include <CL/cl.hpp>
#include "json.hpp"

using json = nlohmann::json;
using namespace std;
using namespace chrono;

auto add = [](auto name) {
	map<string, function<unique_ptr<float[]>(float* &, float* &, int)>> methods;
	methods["classic"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		#pragma loop(no_vector)	
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] + array2[i];
		}
		return arrayOut;
	};
	methods["autoparallel"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		#pragma loop(ivdep)
		#pragma loop(hint_parallel(4))
		#pragma loop(no_vector)	
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = array1[i] + array2[i];
		}
		return arrayOut;
	};
	methods["autovector"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
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
		#pragma loop(no_vector)	
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
			#pragma loop(no_vector)	
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
	methods["openCL"] = [](auto &array1, auto &array2, auto arrayLength) {
		vector<cl::Platform> all_platforms;
		cl::Platform::get(&all_platforms);
		cl::Platform default_platform = all_platforms[0];

		vector<cl::Device> all_devices;
		default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
		cl::Device default_device = all_devices[0];
		cl::Context context({ default_device });
		cl::Program::Sources sources;

		string kernel_code =
			"   void kernel simple_add(global const float* A, global const float* B, global float* C){	"
			"       C[get_global_id(0)] = A[get_global_id(0)] + B[get_global_id(0)];					"
			"   }																						";
		sources.push_back({ kernel_code.c_str(),kernel_code.length() });

		cl::Program program(context, sources);
		if (program.build({ default_device }) != CL_SUCCESS) {
			cout << " Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device) << "\n";
			exit(1);
		}

		cl::Buffer buffer_A(context, CL_MEM_READ_WRITE, sizeof(float) * arrayLength);
		cl::Buffer buffer_B(context, CL_MEM_READ_WRITE, sizeof(float) * arrayLength);
		cl::Buffer buffer_C(context, CL_MEM_READ_WRITE, sizeof(float) * arrayLength);

		cl::CommandQueue queue(context, default_device);

		queue.enqueueWriteBuffer(buffer_A, CL_TRUE, 0, sizeof(float) * arrayLength, array1);
		queue.enqueueWriteBuffer(buffer_B, CL_TRUE, 0, sizeof(float) * arrayLength, array2);

		cl::Kernel kernel_add = cl::Kernel(program, "simple_add");
		kernel_add.setArg(0, buffer_A);
		kernel_add.setArg(1, buffer_B);
		kernel_add.setArg(2, buffer_C);
		queue.enqueueNDRangeKernel(kernel_add, cl::NullRange, cl::NDRange(arrayLength), cl::NullRange);
		queue.finish();

		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		queue.enqueueReadBuffer(buffer_C, CL_TRUE, 0, sizeof(float) * arrayLength, arrayOut.get());
		return arrayOut;
	};
	return methods[name];
};

auto detectGPU = [] () {
	vector<cl::Platform> all_platforms;
	cl::Platform::get(&all_platforms);
	if (all_platforms.size() == 0) {
		cout << " No platforms found. Check OpenCL installation!\n";
		exit(1);
	}
	cl::Platform default_platform = all_platforms[0];
	vector<cl::Device> all_devices;
	default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
	if (all_devices.size() == 0) {
		cout << " No devices found. Check OpenCL installation!\n";
		exit(1);
	}
	cout << "Using openCL with GPU: " << all_devices[0].getInfo<CL_DEVICE_NAME>() << endl;
};

auto detectCPU = [] (auto print) {
	LARGE_INTEGER lpFrequency;
	SYSTEM_INFO siSysInfo;
	try {
		QueryPerformanceFrequency(&lpFrequency);
		GetSystemInfo(&siSysInfo);
		
	} catch (exception const& e) {
		cout << "Error: " << e.what() << '\n';
	}
	if (print) {
		cout << "Using CPU: " << siSysInfo.dwNumberOfProcessors << " cores at about " << float(lpFrequency.QuadPart) / 1'000'000 << " Ghz" << endl;
	}
	return float(lpFrequency.QuadPart);
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
		cout << name << " function done in " << duration_cast<nanoseconds>(end - begin).count() << " ns. " << endl;
		cout << array1[0] << " + " << array2[0] << " = " << output[0] << endl;
	}
	return duration_cast<nanoseconds>(end - begin).count();
};

int main() {
	detectGPU();
	detectCPU(true);
	vector<int> lengths = { 4, 16, 128, 1'024, 16'384, 131'072, 1'048'576, 16'777'216, 134'217'728 };
	std::cout << "Will run " << lengths.size() << " times for a total of +/- 10 minutes. Do not exit." << endl;
	json jsontiny, jsonbig;

	for (auto l = 0; l < lengths.size(); l++) {
		auto arrayLength = lengths[l], randomValueMax = 1'000;
		std::cout << "Preparing for " << arrayLength << " elements ...";
		auto arrayA = initializeRandomArray(arrayLength, randomValueMax);
		std::cout << " half way ... ";
		auto arrayB = initializeRandomArray(arrayLength, randomValueMax);
		std::cout << " done, computation starts now ! " << endl;

		vector<string> methods = { "classic", "autoparallel", "autovector", "openmp", "simd", "threads", "openCL" };
		for_each(methods.begin(), methods.end(), [&arrayA, &arrayB, &arrayLength, &jsontiny](auto method) {
			vector<long long> measurements;
			for (auto i = 0; i < 100; i++) {
				measurements.push_back(calculateTime(method, add(method), arrayA, arrayB, arrayLength));
			}
			sort(measurements.begin(), measurements.end());
			auto median = measurements[measurements.size() / 2];
			auto average = accumulate(measurements.begin(), measurements.end(), .0) / measurements.size();
			auto GoperationsPerSecond = arrayLength / average;
			auto Gbandwith = sizeof(float) * 3 * arrayLength / average;
			auto ticks = average * detectCPU(false) * 1'000;
			cout << "> ";
			cout << method << ": " << GoperationsPerSecond << " GO/s" << endl;
			jsontiny[method] = { 
				{ "average", average }, 
				{ "median", median}, 
				{ "minimum", measurements.at(0) }, 
				{ "maximum", measurements.at(measurements.size() - 1) },
				{ "operationsPerSecond", GoperationsPerSecond },
				{ "bandwith", Gbandwith },
				{ "ticks", ticks }
			};
		});

		jsonbig[to_string(lengths[l])] = jsontiny;
		std::cout << "Computation ended " << (l + 1) << "/" << lengths.size() << " are done." << endl;
		delete[] arrayA;
		delete[] arrayB;
	}

	std::ofstream o("logsfordugagjin.json");
	o << std::setw(4) << jsonbig << std::endl;
	std::cout << "Program ended and data saved. You can safely exit the program by pressing enter." << endl;

	cin.get();
	return 0;
}

