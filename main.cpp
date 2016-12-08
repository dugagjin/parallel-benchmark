#include <iostream>
#include <time.h>
#include <chrono>

#define arrayLength 1000000
#define maxRandomValue 1000

using namespace std;

auto initializeArray = [](){
    srand(time(NULL));
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
};

/*auto openMpSum = [](auto array){
    auto sum = 0;
    #pragma omp parallel for
    for (auto i = 0; i < arrayLength; i++){
        sum += array[i];
    }
};*/

auto calculateTime = [](auto function, auto array){
    chrono::steady_clock::time_point begin = chrono::steady_clock::now();
    function(array);
    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    cout << "time required is " << chrono::duration_cast<chrono::microseconds>(end - begin).count() << " µs" << endl;
};

int main (){
    auto* array = initializeArray();
    calculateTime(sequentialSum, array);
    //calculateTime(openMpSum, array);
    return 0;
}
