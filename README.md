


# Project 3: Memory Allocator

See: https://www.cs.usfca.edu/~mmalensek/cs326/assignments/project-3.html 

To compile and use the allocator:

```bash
make
LD_PRELOAD=$(pwd)/allocator.so ls /
```

(in this example, the command `ls /` is run with the custom memory allocator instead of the default).

## About
Custom memory allocator. Malloc isn't provided by the OS, nor is it a system call; it is a library function that uses system calls to allocate and deallocate memory.
```
## Included Files

* **allocator.c**: memory allocator implementation.
* **allocator.h**: allocator.c header file, contains function prototypes and globals for memory allocator implementation
* **logger.h**: Text-based UI functionality. 
* **test.c**: test file.
* **Makefile**: included to compile and run the program.
```

## Testing

To execute the test cases, use `make test`. To pull in updated test cases, run `make testupdate`. You can also run a specific test case instead of all of them:

```
# Run all test cases:
make test

# Run a specific test case:
make test run=4

# Run a few specific test cases (4, 8, and 12 in this case):
make test run='4 8 12'
```
