/* sparse_htree.h -- SHARED, deterministic sparse placement-driven DRAM H-tree.
 *
 * SINGLE SOURCE OF TRUTH (invariant #2): the topology emitter (src/main.cpp) and
 * the PE-MI routing (pe_memory_interface.h) both call buildSparseHTree() and
 * SparseHTree::endpointForUnit(), so endpoint ids can never drift between the
 * emitted Garnet topology and the runtime routing.
 *
 * MODEL: only PE-hosting branches are materialized, down to the placement level.
 *  - Each PE home unit -> a leaf ENDPOINT; endpoint e == PE e.
 *  - For every router that has EMPTY (no-PE) children, ONE abstract ENDPOINT is
 *    attached at that router -- the maximal-empty-subtree root, on the genuinely
 *    shared parent link (invariant #4: abstract = terminus, no deeper). A non-PE
 *    unit routes to the abstract endpoint of its DEEPEST LIVE ancestor, so near
 *    (same bank as a PE) vs far (other channel) is tiered by real tree distance.
 *  - Below the tree = the aggregate DRAM model (tech base latency + M/D/1 +
 *    channel-BW), charged by the caller. No per-unit nodes, no compute.
 *
 * Node count ~ PEs x tree-depth, not unit count -> sparse sweeps stay tiny.
 */
#ifndef PIMID_SPARSE_HTREE_H_
#define PIMID_SPARSE_HTREE_H_

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <set>

namespace pimid_htree {

// Decompose a flat unit id (at placement level `peLevel`) into hierarchy coords,
// TOP->LEAF (path[0]=channel ... path.back()=the peLevel unit). Matches
// hierarchy_util.h::unitToHierPos. fan[k] = children per parent for k->k+1:
//   0->1 subarrays/bank, 1->2 banks/bg, 2->3 bg/chip, 3->4 chips/rank,
//   4->5 ranks/channel; above channel = ROOT.
inline std::vector<long> decompose(uint64_t unit, int peLevel,
                                   int SA, int BpBG, int BGpC, int CpR, int RpCh) {
    long fan[5] = { SA > 0 ? SA : 1, BpBG > 0 ? BpBG : 1, BGpC > 0 ? BGpC : 1,
                    CpR > 0 ? CpR : 1, RpCh > 0 ? RpCh : 1 };
    std::vector<long> fineToCoarse;
    long id = (long)unit;
    int lvl0 = peLevel < 0 ? 0 : peLevel;
    for (int lvl = lvl0; lvl < 5; ++lvl) {
        long fn = fan[lvl] > 0 ? fan[lvl] : 1;
        fineToCoarse.push_back(id % fn);
        id /= fn;
    }
    fineToCoarse.push_back(id);                 // channel (top)
    return std::vector<long>(fineToCoarse.rbegin(), fineToCoarse.rend());  // top->leaf
}

struct Link { int a, b, w, lat; };

struct SparseHTree {
    int numRouters = 1;           // ROOT=0 + internal routers
    int numPEs = 0;               // PE endpoints [0, numPEs)
    int numAbstract = 0;          // abstract endpoints [numPEs, numPEs+numAbstract)
    std::vector<Link> intLinks;   // router<->router (both directions emitted by caller)
    std::vector<Link> extLinks;   // endpoint->router (a=endpoint, b=router, w, lat)

    // dims for endpointForUnit
    int peLevel = 1, SA = 1, BpBG = 1, BGpC = 1, CpR = 1, RpCh = 1;
    std::map<std::string,int> routerOf;   // coord-prefix key -> router id
    std::map<int,int> abstractOf;         // router id -> abstract endpoint id (if it has empties)
    std::map<int,int> peOfLeaf;           // leaf router id -> PE index

    int totalEndpoints() const { return numPEs + numAbstract; }

    // Endpoint a target unit routes to: PE index if the unit hosts a PE; else the
    // abstract endpoint of the unit's deepest LIVE ancestor. -1 only if the map is
    // empty (no PEs). Both sides call THIS -> ids agree.
    int endpointForUnit(uint64_t unit) const {
        std::vector<long> path = decompose(unit, peLevel, SA, BpBG, BGpC, CpR, RpCh);
        std::string key;
        int parent = 0;
        for (size_t i = 0; i < path.size(); ++i) {
            key += "/" + std::to_string(path[i]);
            auto it = routerOf.find(key);
            if (it == routerOf.end()) {
                // diverges into an empty region here -> parent's abstract endpoint
                auto a = abstractOf.find(parent);
                return (a != abstractOf.end()) ? a->second : -1;
            }
            parent = it->second;
        }
        auto p = peOfLeaf.find(parent);   // full path live -> a PE leaf
        return (p != peOfLeaf.end()) ? p->second : -1;
    }
};

// children per parent at router level L (6=ROOT/system down to 1=bank; leaf L=peLevel has none)
inline int childFanout(int L, int N, int SA, int BpBG, int BGpC, int CpR, int RpCh) {
    switch (L) {
        case 6: return N > 0 ? N : 1;          // ROOT -> channels
        case 5: return RpCh > 0 ? RpCh : 1;    // channel -> ranks
        case 4: return CpR > 0 ? CpR : 1;      // rank -> chips
        case 3: return BGpC > 0 ? BGpC : 1;    // chip -> bank-groups
        case 2: return BpBG > 0 ? BpBG : 1;    // bank-group -> banks
        case 1: return SA > 0 ? SA : 1;        // bank -> subarrays
        default: return 1;
    }
}

// Build the sparse tree from the PE home units. layerW/layerLat index the 4 link
// layers (0=leaf/widest ... 3=channel/narrowest); a link d hops above the leaf
// uses layer min(d,3).
inline SparseHTree buildSparseHTree(const std::vector<uint64_t>& peHomes,
                                    int peLevel, int N,
                                    int SA, int BpBG, int BGpC, int CpR, int RpCh,
                                    const int layerW[4], const int layerLat[4]) {
    SparseHTree t;
    t.peLevel = peLevel < 0 ? 0 : peLevel;
    t.SA = SA; t.BpBG = BpBG; t.BGpC = BGpC; t.CpR = CpR; t.RpCh = RpCh;
    t.routerOf[""] = 0;
    int nextRouter = 1;
    // per-router: (level, set of live child coords)
    std::map<int, std::pair<int, std::set<long>>> info;
    info[0] = { 6, {} };                       // ROOT at level 6 (system)

    for (size_t p = 0; p < peHomes.size(); ++p) {
        std::vector<long> path = decompose(peHomes[p], t.peLevel, SA, BpBG, BGpC, CpR, RpCh);
        int Lp = (int)path.size();
        std::string key;
        int parent = 0;
        for (int i = 0; i < Lp; ++i) {
            info[parent].second.insert(path[i]);      // parent now has this live child
            key += "/" + std::to_string(path[i]);
            auto it = t.routerOf.find(key);
            int rid;
            if (it == t.routerOf.end()) {
                rid = nextRouter++;
                t.routerOf[key] = rid;
                int parentLevel = info[parent].first;
                info[rid] = { parentLevel - 1, {} };
                int dfl = (Lp - 1) - i;               // hops above leaf
                int li = dfl < 3 ? dfl : 3;
                t.intLinks.push_back({ parent, rid, layerW[li], layerLat[li] });
            } else {
                rid = it->second;
            }
            parent = rid;
        }
        t.peOfLeaf[parent] = (int)p;
        t.extLinks.push_back({ (int)p, parent, layerW[0], layerLat[0] });  // PE at its unit -> L0
    }
    t.numPEs = (int)peHomes.size();
    t.numRouters = nextRouter;

    // Abstract endpoints: a router at level L (> peLevel) whose live-child count is
    // below its fan-out has empty children -> one abstract endpoint, hung on it via
    // the child link layer. Deterministic order = router id.
    int nextEndpoint = t.numPEs;
    for (auto& kv : info) {
        int rid = kv.first;
        int level = kv.second.first;
        int live = (int)kv.second.second.size();
        if (level <= t.peLevel) continue;             // leaf level: below = Ramulator
        int fanout = childFanout(level, N, SA, BpBG, BGpC, CpR, RpCh);
        if (live < fanout) {                          // has empty children
            int abst = nextEndpoint++;
            t.abstractOf[rid] = abst;
            int childLevel = level - 1;               // the empty children's level
            int dfl = childLevel - t.peLevel;         // hops above leaf
            int li = (dfl < 0 ? 0 : (dfl < 3 ? dfl : 3));
            t.extLinks.push_back({ abst, rid, layerW[li], layerLat[li] });
        }
    }
    t.numAbstract = nextEndpoint - t.numPEs;
    return t;
}

} // namespace pimid_htree

#endif // PIMID_SPARSE_HTREE_H_
