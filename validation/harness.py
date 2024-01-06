import os
import subprocess
import sys
import unittest

compiler_path = os.path.abspath(sys.path[0] + "/../bin/cc")


class ValidationTestCase(unittest.TestCase):
    def __init__(self, test_dir: str):
        super().__init__()
        self.test_dir = test_dir

    def id(self):
        return self.test_dir

    def setUp(self):
        # Create a temporary build directory for the test
        mktemp_result = subprocess.run(["mktemp", "-d"], stdout=subprocess.PIPE)
        if mktemp_result.returncode != 0:
            raise Exception("Failed to create temporary directory")
        self.tempdir = mktemp_result.stdout.decode("utf-8").strip()

    def tearDown(self):
        # Delete the temporary build directory
        subprocess.run(["rm", "-rf", self.tempdir])

    def runTest(self):
        binary = self.build()
        result = subprocess.run([binary])
        expected_exit_code = self.expected_exit_code()
        self.assertEqual(result.returncode, expected_exit_code, "Program exited with unexpected exit code " + str(result.returncode))

    # Builds the test program, and returns the path to the resulting binary.
    def build(self):
        test_file = self.test_dir + "/main.c"
        ir_file_path = self.tempdir + "/program.ll"
        build_result = subprocess.run([compiler_path, test_file, "-o", ir_file_path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if build_result.returncode != 0:
            raise Exception("Failed to build test program " + test_file + "\n" + build_result.stdout.decode("utf-8"))

        # Currently this just emits LLVM IR, so we need to assemble and link the output into a binary we can run.
        object_file_path = self.tempdir + "/program.o"
        assemble_result = subprocess.run(["llc", ir_file_path, "-filetype=obj", "-o", object_file_path])
        if assemble_result.returncode != 0:
            raise Exception("Failed to assemble test program " + test_file)

        output_file_path = self.tempdir + "/program"
        link_result = subprocess.run(["clang", object_file_path, "-o", output_file_path])
        if link_result.returncode != 0:
            raise Exception("Failed to link test program " + test_file)

        return output_file_path

    # Gets the expected exit code of the test program.
    def expected_exit_code(self):
        expected_exit_code_file = self.test_dir + "/exit.expected"
        if not os.path.exists(expected_exit_code_file):
            return 0
        with open(expected_exit_code_file, "r") as f:
            return int(f.read())


def discover_tests():
    # This will find all the subdirectories of the validation test root directory that start with 3 numbers (e.g. 001).
    script_directory = sys.path[0]
    test_directories = []
    for root, dirs, files in os.walk(script_directory):
        for test_dir in dirs:
            if test_dir[0:3].isnumeric():
                test_directories.append(os.path.join(root, test_dir))
    test_directories.sort()

    # Build test suite
    suite = unittest.TestSuite()
    for test_dir in test_directories:
        suite.addTest(ValidationTestCase(str(test_dir)))

    return suite


if __name__ == '__main__':
    # Discover tests
    test_suite = discover_tests()

    # Run tests
    runner = unittest.TextTestRunner()
    tests_result = runner.run(test_suite)
    if not tests_result.wasSuccessful():
        sys.exit(1)
