#include <iostream>
#include <chrono>
#include <string>
#include <future>
#include <vector>
#include <map>
#include <numeric>
#include <limits>

#include <stdlib.h>
#include <windows.h>

#pragma comment(lib, "user32.lib")
#define WINPAUSE system("pause")

#include <CL/cl.hpp>

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
	methods["openCL"] = [](auto &array1, auto &array2, auto arrayLength) {
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
	methods["cuda"] = [](auto &array1, auto &array2, auto arrayLength) {
		float* h_A = (float*)malloc(size);
		float* h_B = (float*)malloc(size);
		float* h_C = (float*)malloc(size);

		// declare device vectors in the device (GPU) memory
		float *d_A, *d_B, *d_C;

		// initialize input vectors
		for (int i = 0; i<n_el; i++) {
			h_A[i] = sin(i);
			h_B[i] = cos(i);
		}

		// allocate device vectors in the device (GPU) memory
		cudaMalloc(&d_A, size);
		cudaMalloc(&d_B, size);
		cudaMalloc(&d_C, size);

		// copy input vectors from the host (CPU) memory to the device (GPU) memory
		cudaMemcpy(d_A, h_A, size, cudaMemcpyHostToDevice);
		cudaMemcpy(d_B, h_B, size, cudaMemcpyHostToDevice);

		// call kernel function
		sum(d_A, d_B, d_C, n_el);

		// copy the output (results) vector from the device (GPU) memory to the host (CPU) memory
		cudaMemcpy(h_C, d_C, size, cudaMemcpyDeviceToHost);
		// free device memory
		cudaFree(d_A);
		cudaFree(d_B);
		cudaFree(d_C);

		// compute the cumulative error
		double err = 0;
		for (int i = 0; i<n_el; i++) {
			double diff = double((h_A[i] + h_B[i]) - h_C[i]);
			err += diff*diff;
			// print results for manual checking.
			std::cout << "A+B: " << h_A[i] + h_B[i] << "\n";
			std::cout << "C: " << h_C[i] << "\n";
		}
		err = sqrt(err);
		std::cout << "err: " << err << "\n";

		// free host memory
		delete[] h_A;
		delete[] h_B;
		delete[] h_C;

		//return cudaDeviceSynchronize();
		unique_ptr<float[]> arrayOut(new float[arrayLength]);
		for (auto i = 0; i < arrayLength; i++) {
			arrayOut[i] = 1;
		}
		return arrayOut;
	}
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
	return all_devices[0].getInfo<CL_DEVICE_NAME>();
};

auto printSystemInfo = [] {
	try {
		LARGE_INTEGER lpFrequency;
		QueryPerformanceFrequency(&lpFrequency);
		SYSTEM_INFO siSysInfo;
		GetSystemInfo(&siSysInfo);
		cout << "Using CPU: " << siSysInfo.dwNumberOfProcessors << " cores at about " << float(lpFrequency.QuadPart) / 1'000'000 << " Ghz" << endl;
		cout << "Using openCL with GPU: " << detectGPU() << endl;
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
	auto arrayLength = 1048576, randomValueMax = 1'000;
	cout << "Generating array with " << arrayLength << " elements ...";
	auto arrayA = initializeRandomArray(arrayLength, randomValueMax);
	cout << " half way ... ";
	auto arrayB = initializeRandomArray(arrayLength, randomValueMax);
	cout << " done ! " << endl;
	printSystemInfo();
	vector<string> methods = { "classic", "autoparallel", "autovector", "openmp", "simd", "threads", "openCL" };

	for_each(methods.begin(), methods.end(), [&arrayA, &arrayB, &arrayLength](auto method) {
		vector<long long> measurements;
		for (auto i = 0; i < 1; i++) {
			measurements.push_back(calculateTime(method, add(method), arrayA, arrayB, arrayLength));
		}
		sort(measurements.begin(), measurements.end());
		auto median = measurements[measurements.size() / 2];
		auto average = accumulate(measurements.begin(), measurements.end(), .0) / measurements.size();
		cout << method << ":\naverage: " << average << "\t median: " << median << "\t min: " << measurements.at(0) << "\t max: " << measurements.at(measurements.size() - 1) << endl;
	});
	WINPAUSE;
	return 0;
}

