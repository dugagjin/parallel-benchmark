## Synopsis

This project try to parallelize the reduction of an array with different approaches. The project is made for Windows.

## What is already implemented

- Sequential
- Auto-parallelize
- No vector
- OpenMP
- SIMD
- Threads

## What has to be implemented

- OpenCL

## Actual issues

- Auto-parallelize from Microsoft do not work (error 1007: reduction of array to scalar)
- Threads have a argument problem
- SIMD should be for integers but does it exist ?

## To run

In developer command line:
```
cl /EHsc /O2 /Qpar main.cpp && main
```

## Author

Dugagjin Lashi

## License

MIT
