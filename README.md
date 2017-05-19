## Synopsis

This project try to parallelize the reduction of an array with different approaches. The project is made for Windows.

## What is already implemented

- Sequential
- Auto-parallelize
- No vector
- OpenMP
- Threads (with futures)

## What has to be implemented

- OpenCL
- CUDA
- SIMD (written but has a bug)

## Remarks

- Auto-parallelize from Microsoft does not work - error 1007: reduction of array to scalar - so it's normal.

## To run

In developer command line:
```
cl /EHsc /O2 /Qpar main.cpp && main
```

## Author

Dugagjin Lashi

## License

MIT
