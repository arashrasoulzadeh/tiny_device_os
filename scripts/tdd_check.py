#!/usr/bin/env python3
"""
TDD Check Hook - Ensures tests are written before implementation.
This script runs as a pre-commit hook to enforce TDD discipline.
"""

import sys
import subprocess
import os
import argparse
from pathlib import Path


def get_staged_files():
    """Get list of staged C files."""
    result = subprocess.run(
        ['git', 'diff', '--cached', '--name-only', '--diff-filter=ACM'],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        return []
    
    files = result.stdout.strip().split('\n')
    return [f for f in files if f.endswith('.c') and not f.startswith('tests/')]


def get_corresponding_test(impl_file):
    """Find the corresponding test file for an implementation file."""
    path = Path(impl_file)
    rel_path = path.relative_to(path.anchor) if path.is_absolute() else path
    
    # Try different test locations
    test_patterns = [
        f"tests/unit/test_{path.stem}.c",
        f"tests/unit/test_{path.parent.name}_{path.stem}.c",
        f"tests/integration/test_{path.stem}.c",
    ]
    
    for pattern in test_patterns:
        if Path(pattern).exists():
            return pattern
    
    return None


def has_test_content(test_file):
    """Check if test file has actual test content."""
    try:
        with open(test_file, 'r') as f:
            content = f.read()
        
        # Check for test markers
        test_markers = [
            'RUN_TEST',
            'TEST_ASSERT',
            'void test_',
            'UNITY_BEGIN',
            'UNITY_END'
        ]
        
        for marker in test_markers:
            if marker in content:
                return True
    except Exception:
        pass
    return False


def check_tdd_compliance(staged_files):
    """Check TDD compliance for staged files."""
    violations = []
    
    for impl_file in staged_files:
        test_file = get_corresponding_test(impl_file)
        
        if not test_file:
            violations.append(f"No test file found for {impl_file}")
            continue
        
        if not has_test_content(test_file):
            violations.append(f"Test file {test_file} exists but has no test content")
    
    return violations


def main():
    parser = argparse.ArgumentParser(description='TDD compliance check')
    parser.add_argument('--staged', action='store_true', help='Check only staged files')
    parser.add_argument('--all', action='store_true', help='Check all C files')
    args = parser.parse_args()
    
    if args.staged:
        files = get_staged_files()
    elif args.all:
        result = subprocess.run(
            ['git', 'ls-files', '*.c'],
            capture_output=True, text=True
        )
        files = [f for f in result.stdout.strip().split('\n') 
                if f and not f.startswith('tests/')]
    else:
        files = get_staged_files()
    
    if not files:
        print("No C implementation files to check")
        return 0
    
    violations = check_tdd_compliance(files)
    
    if violations:
        print("TDD COMPLIANCE VIOLATIONS:")
        print("=" * 50)
        for v in violations:
            print(f"  - {v}")
        print("=" * 50)
        print("\nTDD RULE: Write failing tests FIRST (Red), then implement (Green), then refactor.")
        print("Create test file in tests/unit/ or tests/integration/ before implementing.")
        return 1
    
    print("TDD check passed - all implementations have corresponding tests")
    return 0


if __name__ == '__main__':
    sys.exit(main())