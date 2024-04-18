import os
import subprocess
import sys
import unittest
from typing import Optional, List

compiler_path = os.path.abspath(sys.path[0] + "/../bin/cc")


class NamedTestSuite(unittest.TestSuite):
    def __init__(self, name: str, tests=()):
        super().__init__(tests)
        self.name = name

    def __str__(self):
        return self.name


class ValidationTestCase(unittest.TestCase):
    def __init__(self, test_dir: str, name: Optional[str] = None):
        super().__init__()
        self.name = name
        self.test_dir = test_dir

    def id(self) -> str:
        return os.path.basename(self.test_dir)

    def shortDescription(self) -> Optional[str]:
        return None

    def __str__(self):
        if self.name is not None:
            return self.name
        return self.id()

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
        result = subprocess.run([binary], stdout=subprocess.PIPE, text=True)

        self.assertEqual(result.returncode, self.expected_exit_code(), "Program exited with unexpected exit code " +
                         str(result.returncode))
        self.assertEqual(result.stdout, self.expected_stdout(), "Program produced unexpected output (stdout)")

    # Builds the test program, and returns the path to the resulting binary.
    def build(self):
        test_file = self.test_dir + "/main.c"
        ir_file_path = self.tempdir + "/program.ll"
        build_result = subprocess.run([compiler_path, test_file, "-o", ir_file_path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if build_result.returncode != 0:
            raise Exception("Failed to build test program " + test_file + "\n" + build_result.stdout.decode("utf-8"))

        # Currently this just emits LLVM IR, so we need to assemble and link the output into a binary we can run.
        object_file_path = self.tempdir + "/program.o"
        assemble_result = subprocess.run(["llc", "-relocation-model=pic", ir_file_path, "-filetype=obj", "-o", object_file_path])
        if assemble_result.returncode != 0:
            raise Exception("Failed to assemble test program " + test_file)

        output_file_path = self.tempdir + "/program"
        link_result = subprocess.run(["clang", object_file_path, "-o", output_file_path])
        if link_result.returncode != 0:
            raise Exception("Failed to link test program " + test_file)

        return output_file_path

    # Gets the expected exit code of the test program.
    def expected_exit_code(self) -> int:
        expected_exit_code_file = self.test_dir + "/exit.expected"
        if not os.path.exists(expected_exit_code_file):
            return 0
        with open(expected_exit_code_file, "r") as f:
            return int(f.read())

    # Gets the expected output to stdout of the test program.
    def expected_stdout(self) -> str:
        expected_stdout_file = self.test_dir + "/stdout.expected"
        if not os.path.exists(expected_stdout_file):
            return ""
        with open(expected_stdout_file, "r") as f:
            return f.read()


def discover_suites() -> List[NamedTestSuite]:
    script_directory = sys.path[0]
    test_suites = []
    # dict of root -> test dir
    tests = {}
    # Searches for test directories in the script directory
    for root, dirs, files in os.walk(script_directory):
        for dir in dirs:
            # Tests are directories that start with 3+ numbers, e.g. `001-return-0`
            if dir[0:3].isnumeric():
                if root not in tests:
                    tests[root] = []
                tests[root].append(dir)

    for root in tests:
        suite_name = str(os.path.relpath(root, script_directory))
        if suite_name == ".":
            suite_name = "root"
        suite = NamedTestSuite(suite_name)
        for test_dir in tests[root]:
            suite.addTest(ValidationTestCase(str(os.path.join(root, test_dir))))
        test_suites.append(suite)

    return test_suites


def run():
    # Discover tests
    test_suites = discover_suites()

    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    success = True
    for suite in test_suites:
        print(f'Running suite {suite.name}...')
        tests_result = runner.run(suite)
        success &= tests_result.wasSuccessful()
        print('')

    if success:
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == '__main__':
    run()
