# Parallel micro-benchmark

The idea is to perform summation reduction of an array using c++ with different approaches.

## Files

There is just one main file.

### What is already implemented

- sequential
- auto parallelization
- auto vectorization

### What has to be implemented

- vectorized (SIMD)
- multi-threaded
- openCL
- openMP

## How to launch (Windows)

- sequential
```
cl /EHsc main.cpp && main
```
- Microsoft auto-parallelization
```
cl /EHsc main.cpp /O2 /Qpar /Qpar-report:1 && main
```

## How to launch (Linux)

- sequential
```
g++ -O1 -std=c++14 main.cpp -o main && ./main
```
- G++ auto vectorization
```
g++ -O1 -std=c++14 -funroll-loops -ftree-vectorize -ftree-vectorizer-verbose=1 main.cpp -o main && ./main
```

## Author

Dugagjin Lashi

## License

MIT
