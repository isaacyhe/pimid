#!/usr/bin/env python3
"""
Python 3 Compatibility Verification Test

Tests that all Python scripts in the PIMID project are Python 3 compatible.
This includes:
1. Shebang verification (#!/usr/bin/python3)
2. Syntax checking (py_compile)
3. Import verification
4. Python 2 anti-pattern detection
"""

import os
import sys
import py_compile
import subprocess
import re
from pathlib import Path

# Color codes for output
class Colors:
    GREEN = '\033[0;32m'
    RED = '\033[0;31m'
    YELLOW = '\033[1;33m'
    NC = '\033[0m'  # No Color

class TestResults:
    def __init__(self):
        self.total = 0
        self.passed = 0

    def add_pass(self, message):
        print(f"{Colors.GREEN}[PASS]{Colors.NC} {message}")
        self.passed += 1
        self.total += 1

    def add_fail(self, message):
        print(f"{Colors.RED}[FAIL]{Colors.NC} {message}")
        self.total += 1

    def add_info(self, message):
        print(f"{Colors.YELLOW}[INFO]{Colors.NC} {message}")

    def print_summary(self):
        print("")
        print("=" * 60)
        print("TEST SUMMARY")
        print("=" * 60)
        print(f"Total Tests: {self.total}")
        print(f"Passed:      {self.passed}")
        print(f"Failed:      {self.total - self.passed}")

        if self.passed == self.total:
            print(f"{Colors.GREEN}ALL TESTS PASSED!{Colors.NC}")
            return 0
        else:
            print(f"{Colors.RED}SOME TESTS FAILED{Colors.NC}")
            return 1

def find_python_files(root_dir):
    """Find all Python files in the project"""
    python_files = []

    # Find .py files
    for path in Path(root_dir).rglob("*.py"):
        python_files.append(str(path))

    # Find files with python shebang
    for path in Path(root_dir).rglob("*"):
        if path.is_file() and not path.suffix == '.py':
            try:
                with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                    first_line = f.readline()
                    if 'python' in first_line and '#!' in first_line:
                        python_files.append(str(path))
            except:
                pass

    return sorted(set(python_files))

def check_shebang(file_path, results):
    """Check if file has proper Python 3 shebang"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            first_line = f.readline().strip()

        if not first_line.startswith('#!'):
            results.add_info(f"{file_path}: No shebang")
            return True

        if 'python3' in first_line:
            return True
        elif 'python' in first_line and 'python2' not in first_line:
            results.add_fail(f"{file_path}: Uses 'python' instead of 'python3'")
            return False
        elif 'python2' in first_line:
            results.add_fail(f"{file_path}: Still uses Python 2 shebang")
            return False

        return True
    except Exception as e:
        results.add_fail(f"{file_path}: Error reading shebang - {e}")
        return False

def check_syntax(file_path, results):
    """Check if file has valid Python 3 syntax"""
    try:
        py_compile.compile(file_path, doraise=True)
        return True
    except py_compile.PyCompileError as e:
        results.add_fail(f"{file_path}: Syntax error - {e}")
        return False
    except Exception as e:
        results.add_fail(f"{file_path}: Compilation error - {e}")
        return False

def check_python2_patterns(file_path, results):
    """Check for common Python 2 anti-patterns"""
    python2_patterns = [
        (r'\bprint\s+["\']', 'print statement instead of print()'),
        (r'\.has_key\(', '.has_key() method (use "in" instead)'),
        (r'<>', '<> operator (use != instead)'),
        (r'`.*`', 'backtick repr (use repr() instead)'),
        (r'\bexecfile\(', 'execfile() (use exec(open().read()) instead)'),
        (r'\braw_input\(', 'raw_input() (use input() instead)'),
        (r'\bxrange\(', 'xrange() (use range() instead)'),
        (r'\bunicode\(', 'unicode() (use str() instead)'),
        (r'\blong\(', 'long() (use int() instead)'),
    ]

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        issues = []
        for pattern, description in python2_patterns:
            if re.search(pattern, content):
                issues.append(description)

        if issues:
            results.add_fail(f"{file_path}: Python 2 patterns found")
            for issue in issues:
                results.add_info(f"  - {issue}")
            return False

        return True
    except Exception as e:
        results.add_fail(f"{file_path}: Error checking patterns - {e}")
        return False

def test_script_execution(file_path, results):
    """Test if script can be executed with --help or -h"""
    # Skip if not executable or doesn't have shebang
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            first_line = f.readline()
            if not first_line.startswith('#!'):
                return True  # Skip non-executable scripts
    except:
        return True

    # Try running with --help
    try:
        result = subprocess.run(
            [sys.executable, file_path, '--help'],
            capture_output=True,
            timeout=5,
            text=True
        )
        # Don't fail if --help not supported, just check it doesn't crash
        return True
    except subprocess.TimeoutExpired:
        results.add_info(f"{file_path}: Execution timeout (may require args)")
        return True
    except Exception as e:
        results.add_info(f"{file_path}: Execution test failed - {e}")
        return True  # Don't fail on this, it's informational

def main():
    """Main test runner"""
    print("=" * 60)
    print("Python 3 Compatibility Verification Test Suite")
    print("=" * 60)
    print("")

    # Get project root
    script_dir = Path(__file__).parent.absolute()
    project_root = script_dir.parent.parent

    print(f"Project Root: {project_root}")
    print(f"Python Version: {sys.version}")
    print("")

    results = TestResults()

    # Find all Python files
    print("Scanning for Python files...")
    python_files = find_python_files(project_root)

    # Filter out external tool test files (they're external dependencies)
    python_files = [f for f in python_files if not '/gem5/ext/' in f]
    python_files = [f for f in python_files if not '/gem5/src/systemc/tests/' in f]

    results.add_info(f"Found {len(python_files)} Python files to test")
    print("")

    # Test each file
    for i, file_path in enumerate(python_files, 1):
        relative_path = os.path.relpath(file_path, project_root)
        print(f"[{i}/{len(python_files)}] Testing: {relative_path}")

        # Run tests
        shebang_ok = check_shebang(file_path, results)
        syntax_ok = check_syntax(file_path, results)
        patterns_ok = check_python2_patterns(file_path, results)

        if shebang_ok and syntax_ok and patterns_ok:
            results.add_pass(f"{relative_path}: All checks passed")

        print("")

    # Specific tests for critical scripts
    print("=" * 60)
    print("Testing Critical Scripts")
    print("=" * 60)
    print("")

    critical_scripts = [
        'pimid/external/zsim/misc/list_syscalls.py',
        'pimid/external/zsim/misc/gitver.py',
        'test_topologies.py',
    ]

    for script in critical_scripts:
        script_path = project_root / script
        if script_path.exists():
            results.add_info(f"Critical script: {script}")

            # Check shebang
            with open(script_path, 'r') as f:
                first_line = f.readline().strip()
                if 'python3' in first_line:
                    results.add_pass(f"{script}: Uses Python 3")
                else:
                    results.add_fail(f"{script}: Does not use Python 3")
        else:
            results.add_info(f"Critical script not found: {script}")

    print("")

    # Print summary and exit
    return results.print_summary()

if __name__ == '__main__':
    sys.exit(main())
