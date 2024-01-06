import os
import subprocess
import sys
import unittest


def build_test_program(compiler_bin: str, test_file: str, output_path: str) -> str:
    ir_file_path = output_path + "/program.ll"
    build_result = subprocess.run([compiler_bin, test_file, "-o", ir_file_path])
    if build_result.returncode != 0:
        raise Exception("Failed to build test program " + test_file)

    # Currently this just emits LLVM IR, so we need to assemble and link the output into a binary we can run.
    object_file_path = output_path + "/program.o"
    assemble_result = subprocess.run(["llc", ir_file_path, "-filetype=obj", "-o", object_file_path])
    if assemble_result.returncode != 0:
        raise Exception("Failed to assemble test program " + test_file)

    output_file_path = output_path + "/program"
    link_result = subprocess.run(["clang", object_file_path, "-o", output_file_path])
    if link_result.returncode != 0:
        raise Exception("Failed to link test program " + test_file)

    return output_file_path


class SanityTest(unittest.TestCase):
    def setUp(self):
        mktemp_result = subprocess.run(["mktemp", "-d"], stdout=subprocess.PIPE)
        if mktemp_result.returncode != 0:
            raise Exception("Failed to create temporary directory")
        self.tempdir = mktemp_result.stdout.decode("utf-8").strip()

    def tearDown(self):
        subprocess.run(["rm", "-rf", self.tempdir])

    def testSuccess(self):
        binary = build_test_program(compiler_path, sys.path[0] + "/sanity/should-succeed.c", self.tempdir)
        result = subprocess.run([binary])
        self.assertEqual(result.returncode, 0, "Program exited with non-zero exit code " + str(result.returncode))

    def testFailure(self):
        binary = build_test_program(compiler_path, sys.path[0] + "/sanity/should-fail.c", self.tempdir)
        result = subprocess.run([binary])
        self.assertEqual(result.returncode, 1, "Program exited unexpected exit code " + str(result.returncode))

    def runTest(self):
        self.testSuccess()
        self.tearDown()
        self.setUp()
        self.testFailure()


class ValidationTestCase(unittest.TestCase):
    def __init__(self, test_file: str):
        super().__init__()
        self.test_file = test_file

    def id(self):
        return self.test_file

    def setUp(self):
        mktemp_result = subprocess.run(["mktemp", "-d"], stdout=subprocess.PIPE)
        if mktemp_result.returncode != 0:
            raise Exception("Failed to create temporary directory")
        self.tempdir = mktemp_result.stdout.decode("utf-8").strip()

    def tearDown(self):
        subprocess.run(["rm", "-rf", self.tempdir])

    def runTest(self):
        binary = build_test_program(compiler_path, self.test_file, self.tempdir)
        result = subprocess.run([binary])
        self.assertEqual(result.returncode, 0, "Program exited with non-zero exit code " + str(result.returncode))


compiler_path = sys.path[0] + "/../bin/cc"

if __name__ == '__main__':
    script_directory = sys.path[0]
    # Discover tests
    # This will find all files in the test directory that start with "test" and end with ".c"
    test_files = []
    for root, dirs, files in os.walk(script_directory):
        for file in files:
            if file.endswith(".c") and file.startswith("test"):
                test_files.append(os.path.join(root, file))

    # Build test suite
    suite = unittest.TestSuite()
    suite.addTest(SanityTest())
    for test_file in test_files:
        suite.addTest(ValidationTestCase(test_file))

    # Run tests
    runner = unittest.TextTestRunner()
    result = runner.run(suite)
    if not result.wasSuccessful():
        sys.exit(1)
