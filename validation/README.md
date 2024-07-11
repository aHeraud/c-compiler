# Validation

Each subdirectory in the validation directory contains a single validation test. To be detected, the directory name must start with at least 3 numbers (e.g. `001`). The test harness will build and run each test and report the results.
The harness will return a non-zero exit code if any test fails.

## Test Structure

Each test contains a main function in a file named `main.c`. A zero exit code from the main function indicates success, and a non-zero exit code indicates failure, unless an expected exit code is specified in a file named `exit.expected` in the test directory.
If the test directory contains a file named `stdout.expected`, then the output that the test prints to stdout will be compared to the contents of the file, and the test will fail unless they match.