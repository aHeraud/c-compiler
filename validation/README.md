# Validation

harness.py will run all the tests in the validation directory and sub-directories. Test are identified as files starting in 'test' and ending in '.c'.

## Test Structure
Tests should have a main function that returns 0 on success and non-zero on failure. The harness will run the test and check the return value. If the return value is non-zero, the harness will print the test name and exit with a non-zero value.
