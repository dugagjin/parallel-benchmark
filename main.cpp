#include <iostream>
#include <time.h>
#include <chrono>

#define arrayLength 1000000
#define maxRandomValue 1000

using namespace std;
using namespace std::chrono;

auto initializeArray = [](){
    //srand(time(NULL));
    auto* array = new int [arrayLength];
    for (auto i = 0; i < arrayLength; i++){
        array[i] = rand() % maxRandomValue + 1;
    }
    return array;
};

auto sequentialSum = [](auto array){
    auto sum = 0;
    for (auto i = 0; i < arrayLength; i++){
        sum += array[i];
    }
    return sum;
};

auto calculateTime = [](auto function, auto array){
    steady_clock::time_point begin = steady_clock::now();
    auto sum = function(array);
    steady_clock::time_point end = steady_clock::now();
    cout << "required time: " << chrono::duration_cast<chrono::microseconds>(end - begin).count() << " µs\nsum: " <<  sum <<endl;
};

int main (){
    auto* array = initializeArray();
    calculateTime(sequentialSum, array);
    return 0;
}
