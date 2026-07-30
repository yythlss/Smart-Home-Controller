import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BOARD_DIR = ROOT / "main" / "boards" / "bread-compact-wifi"
TEST_SOURCE = ROOT / "tests" / "native" / "smart_home_policy_test.cpp"


def find_cpp_compiler():
    configured = os.environ.get("CXX")
    candidates = [configured] if configured else []
    candidates.extend(["c++", "g++", "clang++"])
    for candidate in candidates:
        compiler = shutil.which(candidate)
        if compiler:
            return compiler
    return None


class SmartHomePolicyNativeTest(unittest.TestCase):
    def test_policy_contract(self):
        compiler = find_cpp_compiler()
        if compiler is None:
            self.skipTest("No host C++ compiler is available")

        with tempfile.TemporaryDirectory() as temp_dir:
            executable = Path(temp_dir) / (
                "smart_home_policy_test.exe" if os.name == "nt" else "smart_home_policy_test"
            )
            compile_result = subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(BOARD_DIR),
                    str(TEST_SOURCE),
                    str(BOARD_DIR / "smart_home_policy.cc"),
                    "-o",
                    str(executable),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(
                compile_result.returncode,
                0,
                compile_result.stdout + compile_result.stderr,
            )

            run_result = subprocess.run(
                [str(executable)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(
                run_result.returncode,
                0,
                run_result.stdout + run_result.stderr,
            )


if __name__ == "__main__":
    unittest.main()
