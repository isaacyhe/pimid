/*
 * TreeRouter -- up/down deadlock-free router for a CUSTOM DRAM tree.
 *
 * The detailed DRAM NoC is a rooted tree (ROOT = router id 0 at the top;
 * per channel a fan-out subtree ROOT->channel->bankgroup->bank->subarray-leaf;
 * endpoints/NIs hang off the leaves). Shortest-path (TABLE/BFS) routing on a
 * tree is naturally up-to-common-ancestor-then-down (up*down*): a packet only
 * ever climbs toward the LCA and then descends. That turn set is acyclic, so
 * the tree is deadlock-free IF the up-phase and down-phase never compete for
 * the same virtual channels.
 *
 * With a single shared VC pool (the default Router/SwitchAllocator) they DO
 * compete: the VC index is carried hop-to-hop (outvc at router R becomes invc
 * at R+1), and a single-channel tree (e.g. DDR3: 1 channel, all 128 endpoints
 * funnel through ONE channel router + ROOT) lets up-bound flits and down-bound
 * flits hold-and-wait on each other's VCs across the ROOT turnaround. That
 * closes a cyclic channel-dependency -> deadlock (only a handful of packets
 * deliver, then zero forward progress). The 32-VC stopgap merely makes the
 * cycle statistically unreachable; it does not remove it.
 *
 * TreeRouter removes the cycle by VC-class separation (Dally/Seitz up/down):
 *
 *   Within each vnet, the lower half of VCs = class 0 (UP phase),
 *   the upper half = class 1 (DOWN phase).
 *
 *   - A packet injected by an NI starts in class 0 (UP phase).
 *   - While it traverses UP links (toward ROOT) it stays in class 0.
 *   - The first DOWN link it takes promotes it to class 1; once in class 1
 *     it stays there for every remaining (DOWN) hop.
 *   - A class-1 (DOWN-phase) packet may NEVER take an UP link. Shortest-path
 *     tree routing never asks it to, so this is an invariant, not a restriction.
 *
 * Because UP-phase and DOWN-phase flits draw from disjoint VC sets, no up-bound
 * flit can ever wait on a VC held by a down-bound flit (or vice versa) at the
 * same link, so the channel-dependency graph is acyclic. The tree is then
 * provably deadlock-free with as few as 2 VCs per vnet (1 UP + 1 DOWN).
 *
 * Up/Down classification is by OUTPORT DIRECTION NAME (set by the CUSTOM tree
 * builder): "Up" = toward ROOT, "Down" = toward leaves, "Local" = ext link to
 * an NI (terminal delivery; only reached when this router is the destination,
 * which the routing path resolves before VC class matters).
 */

#ifndef __MEM_RUBY_NETWORK_GARNET_0_TREEROUTER_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_TREEROUTER_HH__

#include <vector>

#include "Router.hh"

namespace gem5 {
namespace ruby {
namespace garnet {

class TreeRouter : public Router {
  public:
    using Params = GarnetRouterParams;
    TreeRouter(const Params &p);
    ~TreeRouter() = default;

    void wakeup() override;
    void init() override;
    void resetStats();

  private:
    // SA-I: per-input VC selection (standard round-robin)
    void arbitrate_inports();

    // SA-II: per-output port arbitration with up/down-aware VC allocation
    void arbitrate_outports();

    bool send_allowed(int inport, int invc, int outport, int outvc);
    int vc_allocate(int outport, int inport, int invc);
    int get_vnet(int invc);
    void clear_request_vector();
    void check_for_wakeup();

    // Up/Down helpers
    bool isDownOutport(int outport) const;  // outport dirn == "Down"
    int vcClassOf(int vc) const;            // 0 = UP phase, 1 = DOWN phase
    void getClassRange(int vnet, int vc_class,   // [lo, hi)
                       int &lo, int &hi) const;
    // Required VC class for a flit leaving via `outport`, given its
    // current invc and the inport it arrived on.
    int targetClass(int inport, int invc, int outport) const;

    // Per-input request state (SA-I -> SA-II)
    std::vector<int> m_port_requests;
    std::vector<int> m_vc_winners;
    std::vector<int> m_round_robin_invc;
    std::vector<int> m_round_robin_inport;

    int m_num_inports;
    int m_num_outports;
    int m_num_vcs;
    int m_vc_per_vnet;

    double m_input_arbiter_activity;
    double m_output_arbiter_activity;
};

} // namespace garnet
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_GARNET_0_TREEROUTER_HH__
