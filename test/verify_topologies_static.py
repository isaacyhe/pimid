#!/usr/bin/env python3
"""
Static Topology Verification Script

This script performs static analysis of topology files without
requiring the gem5 environment.
"""

import re
import ast
import os

def check_file_syntax(filepath, topology_name):
    """Check if file has valid Python syntax"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
        ast.parse(content)
        print(f"✓ {topology_name}: Valid Python syntax")
        return True, content
    except SyntaxError as e:
        print(f"✗ {topology_name}: Syntax error at line {e.lineno}: {e.msg}")
        return False, None

def check_class_definition(content, topology_name):
    """Check if class is properly defined"""
    class_pattern = rf'class {topology_name}\(SimpleTopology\):'
    if re.search(class_pattern, content):
        print(f"✓ {topology_name}: Class inherits from SimpleTopology")
        return True
    else:
        print(f"✗ {topology_name}: Class does not inherit from SimpleTopology")
        return False

def check_init_method(content, topology_name):
    """Check if __init__ method is properly defined"""
    init_pattern = r'def __init__\(self, controllers\):'
    if re.search(init_pattern, content):
        print(f"✓ {topology_name}: __init__ method signature correct")

        # Check if self.nodes is set
        if 'self.nodes = controllers' in content:
            print(f"✓ {topology_name}: self.nodes properly assigned")
            return True
        else:
            print(f"⚠ {topology_name}: self.nodes may not be properly assigned")
            return True
    else:
        print(f"✗ {topology_name}: __init__ method signature incorrect")
        return False

def check_make_topology_method(content, topology_name):
    """Check if makeTopology method is properly defined"""
    method_pattern = r'def makeTopology\(self, options, network, IntLink, ExtLink, Router\):'
    if re.search(method_pattern, content):
        print(f"✓ {topology_name}: makeTopology method signature correct")
        return True
    else:
        print(f"✗ {topology_name}: makeTopology method signature incorrect")
        return False

def check_network_assignments(content, topology_name):
    """Check if network attributes are properly set"""
    all_ok = True

    # Check network.routers
    if 'network.routers = routers' in content:
        print(f"✓ {topology_name}: network.routers assignment found")
    else:
        print(f"✗ {topology_name}: network.routers assignment missing")
        all_ok = False

    # Check network.ext_links
    if 'network.ext_links = ext_links' in content:
        print(f"✓ {topology_name}: network.ext_links assignment found")
    else:
        print(f"✗ {topology_name}: network.ext_links assignment missing")
        all_ok = False

    # Check network.int_links (can be int_links or [])
    if 'network.int_links = int_links' in content or 'network.int_links = []' in content:
        print(f"✓ {topology_name}: network.int_links assignment found")
    else:
        print(f"✗ {topology_name}: network.int_links assignment missing")
        all_ok = False

    # Check return statement
    if 'return routers' in content:
        print(f"✓ {topology_name}: routers return statement found")
    else:
        print(f"✗ {topology_name}: routers return statement missing")
        all_ok = False

    return all_ok

def check_description(content, topology_name):
    """Check if description attribute exists"""
    if 'description = ' in content:
        # Extract description
        match = re.search(r'description\s*=\s*["\']([^"\']+)["\']', content)
        if match:
            desc = match.group(1)
            print(f"✓ {topology_name}: description = '{desc}'")
            return True
    print(f"⚠ {topology_name}: description attribute not found")
    return False

def check_router_creation(content, topology_name):
    """Check if routers are created properly"""
    router_pattern = r'Router\(router_id=\w+,\s*latency=[\w.]+\)'
    if re.search(router_pattern, content):
        print(f"✓ {topology_name}: Router creation pattern found")
        return True
    else:
        print(f"⚠ {topology_name}: Router creation pattern not standard")
        return False

def check_link_creation(content, topology_name):
    """Check if links are created properly"""
    ext_link_found = 'ExtLink(' in content
    int_link_found = 'IntLink(' in content

    if ext_link_found:
        print(f"✓ {topology_name}: ExtLink creation found")
    else:
        print(f"⚠ {topology_name}: ExtLink creation not found")

    # Internal links are optional (bus topology doesn't need them)
    if int_link_found:
        print(f"✓ {topology_name}: IntLink creation found")
    else:
        print(f"  {topology_name}: IntLink creation not found (may be intentional for bus topology)")

    return ext_link_found

def analyze_topology_complexity(content, topology_name):
    """Analyze topology complexity"""
    router_count = content.count('Router(')
    ext_link_count = content.count('ExtLink(')
    int_link_count = content.count('IntLink(')

    print(f"  {topology_name}: Complexity analysis:")
    print(f"    - Router creations: ~{router_count}")
    print(f"    - ExtLink creations: ~{ext_link_count}")
    print(f"    - IntLink creations: ~{int_link_count}")

    return True

def verify_topology(topology_name):
    """Run all verification checks on a topology"""
    filepath = f'/home/user/pimid-dev/pimid/external/gem5/configs/topologies/{topology_name}.py'

    if not os.path.exists(filepath):
        print(f"✗ {topology_name}: File not found at {filepath}")
        return False

    print(f"\n{'='*70}")
    print(f"Verifying: {topology_name}")
    print('='*70)

    # Syntax check
    syntax_ok, content = check_file_syntax(filepath, topology_name)
    if not syntax_ok:
        return False

    # Structure checks
    class_ok = check_class_definition(content, topology_name)
    init_ok = check_init_method(content, topology_name)
    make_topology_ok = check_make_topology_method(content, topology_name)
    network_ok = check_network_assignments(content, topology_name)
    desc_ok = check_description(content, topology_name)
    router_ok = check_router_creation(content, topology_name)
    link_ok = check_link_creation(content, topology_name)
    analyze_topology_complexity(content, topology_name)

    # Overall result
    critical_checks = [syntax_ok, class_ok, init_ok, make_topology_ok, network_ok]
    all_passed = all(critical_checks)

    if all_passed:
        print(f"\n✓ {topology_name}: All critical checks passed")
    else:
        print(f"\n✗ {topology_name}: Some critical checks failed")

    return all_passed

def main():
    """Run verification on all topologies"""
    topologies = ['HTree', 'Bus', 'RingBus', 'HTreeBus']

    print("="*70)
    print("STATIC TOPOLOGY VERIFICATION")
    print("="*70)

    results = {}

    for topology_name in topologies:
        results[topology_name] = verify_topology(topology_name)

    # Summary
    print(f"\n{'='*70}")
    print("VERIFICATION SUMMARY")
    print('='*70)

    passed = sum(results.values())
    total = len(results)

    for topology_name, result in results.items():
        status = "PASS" if result else "FAIL"
        symbol = "✓" if result else "✗"
        print(f"{symbol} {topology_name}: {status}")

    print(f"\nTotal: {passed}/{total} topologies passed verification")

    if passed == total:
        print("\n✓ All topologies verified successfully!")
        return 0
    else:
        print(f"\n✗ {total - passed} topology(ies) failed verification")
        return 1

if __name__ == '__main__':
    import sys
    sys.exit(main())
