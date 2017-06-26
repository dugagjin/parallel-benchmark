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

auto vectorAdd = [](auto name) {
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
		const auto aligned = arrayLength - arrayLength % 4;
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


auto matrixMultiplication = [](auto name) {
	map<string, function<unique_ptr<float[]>(float* &, float* &, int)>> methods;
	methods["classic"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		auto width = (int)sqrt(arrayLength);
#pragma loop(no_vector)	
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < width; y++) {
				arrayOut[width * x + y] = 0;
				for (int z = 0; z < width; z++) {
					arrayOut[width * x + y] += array1[width * x + z] * array2[width * z + y];
				}
			}
		}
		return arrayOut;
	};
	methods["autoparallel"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		auto width = (int)sqrt(arrayLength);
#pragma loop(ivdep)
#pragma loop(hint_parallel(4))
#pragma loop(no_vector)	
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < width; y++) {
				arrayOut[width * x + y] = 0;
				for (int z = 0; z < width; z++) {
					arrayOut[width * x + y] += array1[width * x + z] * array2[width * z + y];
				}
			}
		}
		return arrayOut;
	};
	methods["autovector"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		auto width = (int)sqrt(arrayLength);
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < width; y++) {
				arrayOut[width * x + y] = 0;
				for (int z = 0; z < width; z++) {
					arrayOut[width * x + y] += array1[width * x + z] * array2[width * z + y];
				}
			}
		}
		return arrayOut;
	};
	methods["openmp"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		auto width = (int)sqrt(arrayLength);
#pragma omp parallel for
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < width; y++) {
				arrayOut[width * x + y] = 0;
				for (int z = 0; z < width; z++) {
					arrayOut[width * x + y] += array1[width * x + z] * array2[width * z + y];
				}
			}
		}
		return arrayOut;
	};
	methods["threads"] = [](auto &array1, auto &array2, auto arrayLength) {
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		auto dimension = (int)sqrt(arrayLength), numberOfThreads = 4;
		auto mul = [](auto* array1, auto* array2, auto size, auto row, auto col, auto* array3) {
			for (auto i = 0; i < size; ++i) {
				array3[row * size + col] += array1[row * size + i] * array2[i * size + col];
			}
		};
		vector<thread> threads(numberOfThreads);
		for (auto i = 0; i < numberOfThreads; ++i)
			threads[i] = thread([&, i]() {
			for (auto row = i; row < dimension; row += numberOfThreads) {
				for (auto col = 0; col < dimension; ++col) {
					mul(array1, array2, dimension, row, col, arrayOut.get());
				}
			}
		});
		for (auto &thread : threads) {
			thread.join();
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
			"   __kernel void simple_add(__global const float* A, __global const float* B, __global float* C, const int M){		"
			"		const int column  = get_global_id(0);													"
			"		const int row  = get_global_id(1);														"
			"		for (int k = 0; k < M; k++) {															"
			"			C[row * M + column] += A[row * M + k] * B[k * M + column];							"
			"		}																						"
			"	}																							";
		sources.push_back({ kernel_code.c_str(), kernel_code.length() });

		cl::Program program(context, sources);
		if (program.build({ default_device }) != CL_SUCCESS) {
			cout << " Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device) << "\n";
			cin.get();
		}

		cl::Buffer buffer_A(context, CL_MEM_READ_WRITE, sizeof(float) * arrayLength);
		cl::Buffer buffer_B(context, CL_MEM_READ_WRITE, sizeof(float) * arrayLength);
		cl::Buffer buffer_C(context, CL_MEM_READ_WRITE, sizeof(float) * arrayLength);

		cl::CommandQueue queue(context, default_device);

		int width = (int)sqrt(arrayLength);
		queue.enqueueWriteBuffer(buffer_A, CL_TRUE, 0, sizeof(float) * arrayLength, array1);
		queue.enqueueWriteBuffer(buffer_B, CL_TRUE, 0, sizeof(float) * arrayLength, array2);

		cl::Kernel kernel_add = cl::Kernel(program, "simple_add");
		kernel_add.setArg(0, buffer_A);
		kernel_add.setArg(1, buffer_B);
		kernel_add.setArg(2, buffer_C);
		kernel_add.setArg(3, width);
		queue.enqueueNDRangeKernel(kernel_add, cl::NullRange, cl::NDRange(arrayLength), cl::NullRange);
		queue.finish();

		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		queue.enqueueReadBuffer(buffer_C, CL_TRUE, 0, sizeof(float) * arrayLength, arrayOut.get());
		return arrayOut;
	};
	return methods[name];
};

auto detectGPU = []() {
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

auto detectCPU = [](auto print) {
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

auto calculateTime = [](auto name, auto &sumFunction, auto &input1, auto &input2, auto arrayLength, bool debug = false) {
	auto begin = steady_clock::now();
	auto output = sumFunction(input1, input2, arrayLength);
	auto end = steady_clock::now();
	if (debug) {
		cout << input1[1] << " + " << input2[1] << " = " << output[1] << endl;
	}
	return duration_cast<nanoseconds>(end - begin).count();
};

auto benchmark = [](auto lengths, auto implementations, auto algorithm, auto filename) {
	std::cout << "Preparing benchmark in order to generate a " << filename << " file." << endl;

	json jsontiny, jsonbig;
	for (auto l = 0; l < lengths.size(); l++) {
		auto arrayLength = lengths[l], randomValueMax = 10;
		std::cout << "Preparing for " << arrayLength << " elements ...";
		auto arrayA = initializeRandomArray(arrayLength, randomValueMax);
		auto arrayB = initializeRandomArray(arrayLength, randomValueMax);
		std::cout << " done, computation starts now ! " << endl;

		long long reference;
		for_each(implementations.begin(), implementations.end(), [&arrayA, &arrayB, &arrayLength, &jsontiny, &reference, &algorithm](auto implementation) {
			vector<long long> measurements;
			for (auto i = 0; i < 1; i++) {
				measurements.push_back(calculateTime(implementation, algorithm(implementation), arrayA, arrayB, arrayLength, true));
			}
			sort(measurements.begin(), measurements.end());
			auto median = measurements[measurements.size() / 2];
			auto average = accumulate(measurements.begin(), measurements.end(), .0) / measurements.size();
			auto GoperationsPerSecond = arrayLength / average;
			auto Gbandwith = sizeof(float) * 3 * arrayLength / average;
			auto ticks = average * detectCPU(false) * 1'000;
			if (implementation == "classic") {
				reference = median;
			}
			auto boost = (float)reference / median;
			jsontiny[implementation] = {
				{ "average", average },
				{ "median", median },
				{ "minimum", measurements.at(0) },
				{ "maximum", measurements.at(measurements.size() - 1) },
				{ "boost", boost },
				{ "operationsPerSecond", GoperationsPerSecond },
				{ "bandwith", Gbandwith },
				{ "ticks", ticks }
			};
			cout << "> " << boost << " -> " << implementation << endl;
		});

		jsonbig[to_string(lengths[l])] = jsontiny;
		std::cout << "Computation ended " << (l + 1) << "/" << lengths.size() << " for " << filename << " are done." << endl;
		delete[] arrayA;
		delete[] arrayB;
	}
	std::ofstream jsonfile(filename);
	jsonfile << std::setw(4) << jsonbig << std::endl;
};

int main() {
	detectGPU();
	detectCPU(true);

	std::cout << "This will run two benchmarks for a total of +/- 10 minutes. Do not exit." << endl;

	vector<int> lengths = { 16, 256, 1'024, 16'384, 262'144, 1'048'576, 4'194'304, 16'777'216, 134'217'728 };
	vector<string> methods = { "classic", "autovector", "simd", "autoparallel", "openmp", "threads", "openCL" };
	benchmark(lengths, methods, vectorAdd, "vector.json");

	lengths = { 16, 256, 1'024, 16'384, 262'144, 1'048'576, 4'194'304 };
	methods = { "classic", "autovector", "autoparallel", "openmp", "threads", "openCL" };
	benchmark(lengths, methods, matrixMultiplication, "matrix.json");

	std::cout << "Program ended and data saved. You can safely exit the program by pressing enter." << endl;
	std::cin.get();

	return 0;
}
