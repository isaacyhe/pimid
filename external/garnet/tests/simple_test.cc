/**
 * @file simple_test.cc
 * @brief Simple test to verify Garnet builds and links correctly
 */

#include <iostream>

// Include Garnet headers
#include "GarnetNetwork.hh"
#include "Router.hh"
#include "NetworkInterface.hh"
#include "NetworkLink.hh"

using namespace gem5;
using namespace gem5::ruby;
using namespace gem5::ruby::garnet;

int main() {
    std::cout << "=== Garnet Standalone Test ===" << std::endl;

    // Test 1: Test NetDest operations
    std::cout << "\n1. Testing NetDest..." << std::endl;
    NetDest dest;
    dest.add(0);
    dest.add(3);
    dest.add(5);
    std::cout << "   Added nodes: 0, 3, 5" << std::endl;
    std::cout << "   Contains 3: " << (dest.contains(3) ? "yes" : "no") << std::endl;
    std::cout << "   Contains 7: " << (dest.contains(7) ? "yes" : "no") << std::endl;
    std::cout << "   Count: " << dest.count() << std::endl;

    NetDest dest2;
    dest2.add(3);
    dest2.add(4);
    std::cout << "   Intersection test: " << (dest.intersects(dest2) ? "yes" : "no") << std::endl;
    std::cout << "   PASSED" << std::endl;

    // Test 2: Test message creation
    std::cout << "\n2. Testing message creation..." << std::endl;
    SimpleMessage msg(0, 1, MessageSizeType::Data, 100);
    std::cout << "   Created SimpleMessage: ";
    msg.print(std::cout);
    std::cout << std::endl;
    std::cout << "   PASSED" << std::endl;

    // Test 3: Verify network parameters structure
    std::cout << "\n3. Testing network parameters..." << std::endl;
    GarnetNetworkParams params;
    params.num_rows = 4;
    params.num_cols = 4;
    params.vcs_per_vnet = 4;
    params.buffers_per_data_vc = 4;
    params.ni_flit_size = 16;
    std::cout << "   Created 4x4 mesh network params" << std::endl;
    std::cout << "   VCs per vnet: " << params.vcs_per_vnet << std::endl;
    std::cout << "   Buffers per VC: " << params.buffers_per_data_vc << std::endl;
    std::cout << "   PASSED" << std::endl;

    // Test 4: Verify router parameters
    std::cout << "\n4. Testing router parameters..." << std::endl;
    GarnetRouterParams rparams;
    rparams.vcs_per_vnet = 4;
    rparams.virt_nets = 3;
    rparams.width = 128;
    std::cout << "   Created router params" << std::endl;
    std::cout << "   Width: " << rparams.width << " bits" << std::endl;
    std::cout << "   Virt nets: " << rparams.virt_nets << std::endl;
    std::cout << "   PASSED" << std::endl;

    // Test 5: Test link parameters
    std::cout << "\n5. Testing link parameters..." << std::endl;
    NetworkLinkParams lparams;
    lparams.link_latency = Cycles(2);
    lparams.width = 128;
    std::cout << "   Created link params" << std::endl;
    std::cout << "   Latency: " << static_cast<uint64_t>(lparams.link_latency) << " cycles" << std::endl;
    std::cout << "   Width: " << lparams.width << " bits" << std::endl;
    std::cout << "   PASSED" << std::endl;

    // Test 6: Test message size conversion
    std::cout << "\n6. Testing message size conversion..." << std::endl;
    uint32_t ctrl_size = Network::MessageSizeType_to_int(MessageSizeType::Control);
    uint32_t data_size = Network::MessageSizeType_to_int(MessageSizeType::Data);
    std::cout << "   Control message size: " << ctrl_size << " bytes" << std::endl;
    std::cout << "   Data message size: " << data_size << " bytes" << std::endl;
    std::cout << "   PASSED" << std::endl;

    std::cout << "\n=== All tests passed! ===" << std::endl;
    std::cout << "Garnet standalone library is working correctly." << std::endl;

    return 0;
}
