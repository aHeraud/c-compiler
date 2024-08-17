# Validation

Each subdirectory in the validation directory is a test suite, each subfolder in the test suites contain a single validation test. To be detected, the directory name must start with at least 3 numbers (e.g. `001`). The test harness will build and run each test and report the results.
The harness will return a non-zero exit code if any test fails.

## Test Structure

Each test contains a main function in a file named `main.c`. By default, a zero exit code from the main function indicates success, and a non-zero exit code indicates failure, additionally, the test expects that nothing will be printed to stdout. 

Additional files can be added to the test directory to modify the behavior of the test:
* exit.expected   - Specify a different expected exit code
* stdout.expected - Expected stdout of the test 
* stdin           - The stdin stream of the test will read from this file if it is present
