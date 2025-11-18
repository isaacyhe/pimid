#!/usr/bin/env python3
"""
Topology Verification Script

This script validates the newly created network topologies:
- HTree
- Bus
- RingBus
- HTreeBus

Checks performed:
1. Import validation
2. Class structure verification
3. Interface compliance
4. Basic instantiation test
"""

import sys
import os

# Add gem5 configs path
sys.path.insert(0, '/home/user/pimid-dev/pimid/external/gem5/configs')

def test_topology_import(topology_name):
    """Test if topology can be imported"""
    try:
        module = __import__(f'topologies.{topology_name}', fromlist=[topology_name])
        topology_class = getattr(module, topology_name)
        print(f"✓ {topology_name}: Successfully imported")
        return topology_class
    except Exception as e:
        print(f"✗ {topology_name}: Import failed - {e}")
        return None

def test_topology_structure(topology_name, topology_class):
    """Test if topology has required methods and attributes"""
    required_methods = ['makeTopology', '__init__']
    required_attrs = ['description']

    errors = []

    for method in required_methods:
        if not hasattr(topology_class, method):
            errors.append(f"Missing method: {method}")

    for attr in required_attrs:
        if not hasattr(topology_class, attr):
            errors.append(f"Missing attribute: {attr}")

    if errors:
        print(f"✗ {topology_name}: Structure validation failed")
        for error in errors:
            print(f"  - {error}")
        return False
    else:
        print(f"✓ {topology_name}: Structure validation passed")
        return True

def test_topology_inheritance(topology_name, topology_class):
    """Test if topology inherits from SimpleTopology"""
    try:
        from topologies.BaseTopology import SimpleTopology

        if issubclass(topology_class, SimpleTopology):
            print(f"✓ {topology_name}: Correctly inherits from SimpleTopology")
            return True
        else:
            print(f"✗ {topology_name}: Does not inherit from SimpleTopology")
            return False
    except Exception as e:
        print(f"✗ {topology_name}: Inheritance check failed - {e}")
        return False

def test_topology_instantiation(topology_name, topology_class):
    """Test if topology can be instantiated with mock controllers"""
    try:
        # Create mock controllers
        class MockController:
            def __init__(self, idx):
                self.idx = idx
                self.type = "L1Cache_Controller"

        controllers = [MockController(i) for i in range(4)]
        topology = topology_class(controllers)

        if hasattr(topology, 'nodes') and len(topology.nodes) == 4:
            print(f"✓ {topology_name}: Successfully instantiated with controllers")
            return True
        else:
            print(f"✗ {topology_name}: Instantiation succeeded but nodes not set correctly")
            return False
    except Exception as e:
        print(f"✗ {topology_name}: Instantiation failed - {e}")
        return False

def test_topology_description(topology_name, topology_class):
    """Test if topology has a meaningful description"""
    try:
        desc = topology_class.description
        if desc and isinstance(desc, str) and len(desc) > 0:
            print(f"✓ {topology_name}: Description = '{desc}'")
            return True
        else:
            print(f"✗ {topology_name}: Invalid or missing description")
            return False
    except Exception as e:
        print(f"✗ {topology_name}: Description check failed - {e}")
        return False

def analyze_topology_logic(topology_name):
    """Analyze the topology implementation for common issues"""
    filepath = f'/home/user/pimid-dev/pimid/external/gem5/configs/topologies/{topology_name}.py'

    try:
        with open(filepath, 'r') as f:
            content = f.read()

        issues = []

        # Check for return statement in makeTopology
        if 'return routers' not in content:
            issues.append("makeTopology may not return routers")

        # Check if network attributes are set
        if 'network.routers = routers' not in content:
            issues.append("network.routers may not be set")
        if 'network.ext_links = ext_links' not in content:
            issues.append("network.ext_links may not be set")
        if 'network.int_links' not in content:
            issues.append("network.int_links may not be set")

        # Check for link_count usage
        if 'link_count' not in content:
            issues.append("link_count not used (may cause duplicate link IDs)")

        if issues:
            print(f"⚠ {topology_name}: Potential logic issues detected:")
            for issue in issues:
                print(f"  - {issue}")
            return False
        else:
            print(f"✓ {topology_name}: Logic analysis passed")
            return True
    except Exception as e:
        print(f"✗ {topology_name}: Logic analysis failed - {e}")
        return False

def main():
    """Run all verification tests"""
    topologies = ['HTree', 'Bus', 'RingBus', 'HTreeBus']

    print("="*70)
    print("TOPOLOGY VERIFICATION SUITE")
    print("="*70)
    print()

    results = {}

    for topology_name in topologies:
        print(f"\n{'='*70}")
        print(f"Testing: {topology_name}")
        print('='*70)

        # Test 1: Import
        topology_class = test_topology_import(topology_name)
        if not topology_class:
            results[topology_name] = False
            continue

        # Test 2: Structure
        structure_ok = test_topology_structure(topology_name, topology_class)

        # Test 3: Inheritance
        inheritance_ok = test_topology_inheritance(topology_name, topology_class)

        # Test 4: Instantiation
        instantiation_ok = test_topology_instantiation(topology_name, topology_class)

        # Test 5: Description
        description_ok = test_topology_description(topology_name, topology_class)

        # Test 6: Logic analysis
        logic_ok = analyze_topology_logic(topology_name)

        # Overall result
        all_passed = all([
            structure_ok,
            inheritance_ok,
            instantiation_ok,
            description_ok,
            logic_ok
        ])

        results[topology_name] = all_passed

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
    sys.exit(main())
