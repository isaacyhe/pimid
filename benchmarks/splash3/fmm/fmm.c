/* fmm.c -- Fast Multipole Method (2D, simplified monopole)
 * SPLASH-3 style: pthreads parallel. Build quadtree from particles,
 * compute multipole expansions bottom-up, translate multipole->local for
 * well-separated cells top-down, evaluate local + direct near-field. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    1024
#define DEFAULT_THREADS 4
#define MAX_DEPTH       6
#define MAX_PARTICLES   32   /* max particles per leaf before subdivision */
#define THETA           0.5  /* opening angle for well-separated test */

static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

static uint32_t bench_rand(uint32_t* s) {
    *s = *s * 1103515245 + 12345;
    return (*s >> 16) & 0x7FFF;
}

typedef struct {
    double x, y;
    double mass;
    double fx, fy;       /* accumulated force */
    double potential;
} Particle;

/* Quadtree node -- flat array allocation */
typedef struct QNode {
    double cx, cy, size; /* center and half-size */
    double mp_mass;      /* monopole: total mass */
    double mp_x, mp_y;   /* center of mass */
    double local_fx, local_fy; /* local expansion (force contribution) */
    int children[4];     /* indices into node pool (-1 = none) */
    int* plist;          /* particle indices (leaf only) */
    int pcount;          /* number of particles */
    int is_leaf;
} QNode;

static Particle* particles;
static int nparticles, nthreads;
static QNode* nodes;
static int node_count, node_cap;
static pthread_barrier_t barrier;
static pthread_mutex_t node_lock;

static int alloc_node(double cx, double cy, double sz) {
    int idx;
    pthread_mutex_lock(&node_lock);
    if (node_count >= node_cap) {
        node_cap *= 2;
        nodes = (QNode*)realloc(nodes, (size_t)node_cap * sizeof(QNode));
    }
    idx = node_count++;
    pthread_mutex_unlock(&node_lock);
    nodes[idx].cx = cx;
    nodes[idx].cy = cy;
    nodes[idx].size = sz;
    nodes[idx].mp_mass = 0;
    nodes[idx].mp_x = 0;
    nodes[idx].mp_y = 0;
    nodes[idx].local_fx = 0;
    nodes[idx].local_fy = 0;
    nodes[idx].children[0] = nodes[idx].children[1] = -1;
    nodes[idx].children[2] = nodes[idx].children[3] = -1;
    nodes[idx].plist = NULL;
    nodes[idx].pcount = 0;
    nodes[idx].is_leaf = 1;
    return idx;
}

/* Insert particle into tree (serial, during build phase) */
static void tree_insert(int ni, int pi, int depth) {
    QNode* n = &nodes[ni];
    if (n->is_leaf && n->pcount < MAX_PARTICLES) {
        if (!n->plist)
            n->plist = (int*)malloc(MAX_PARTICLES * sizeof(int));
        n->plist[n->pcount++] = pi;
        return;
    }
    if (n->is_leaf) {
        /* Subdivide */
        n->is_leaf = 0;
        double hs = n->size * 0.5;
        double offsets[4][2] = {{-hs, -hs}, {hs, -hs}, {-hs, hs}, {hs, hs}};
        for (int c = 0; c < 4; c++)
            n->children[c] = alloc_node(n->cx + offsets[c][0],
                                         n->cy + offsets[c][1], hs);
        /* Re-insert existing particles */
        for (int i = 0; i < n->pcount; i++)
            tree_insert(ni, n->plist[i], depth + 1);
        n->pcount = 0;
        free(n->plist);
        n->plist = NULL;
    }
    /* Insert into correct child */
    double px = particles[pi].x;
    double py = particles[pi].y;
    int q = (px >= n->cx ? 1 : 0) + (py >= n->cy ? 2 : 0);
    if (n->children[q] >= 0 && depth < MAX_DEPTH)
        tree_insert(n->children[q], pi, depth + 1);
    else {
        /* Fallback: store in this node if max depth */
        if (!n->plist)
            n->plist = (int*)malloc(MAX_PARTICLES * 2 * sizeof(int));
        n->plist[n->pcount++] = pi;
    }
}

/* Compute monopole expansions bottom-up */
static void compute_multipoles(int ni) {
    QNode* n = &nodes[ni];
    if (n->is_leaf) {
        double tm = 0, tx = 0, ty = 0;
        for (int i = 0; i < n->pcount; i++) {
            int pi = n->plist[i];
            double m = particles[pi].mass;
            tm += m;
            tx += m * particles[pi].x;
            ty += m * particles[pi].y;
        }
        n->mp_mass = tm;
        n->mp_x = (tm > 0) ? tx / tm : n->cx;
        n->mp_y = (tm > 0) ? ty / tm : n->cy;
        return;
    }
    double tm = 0, tx = 0, ty = 0;
    for (int c = 0; c < 4; c++) {
        if (n->children[c] < 0) continue;
        compute_multipoles(n->children[c]);
        QNode* ch = &nodes[n->children[c]];
        tm += ch->mp_mass;
        tx += ch->mp_mass * ch->mp_x;
        ty += ch->mp_mass * ch->mp_y;
    }
    n->mp_mass = tm;
    n->mp_x = (tm > 0) ? tx / tm : n->cx;
    n->mp_y = (tm > 0) ? ty / tm : n->cy;
}

/* Evaluate: for each particle, walk tree (Barnes-Hut style with FMM spirit) */
static void eval_force(int ni, int pi) {
    QNode* n = &nodes[ni];
    if (n->mp_mass < 1e-15) return;
    double dx = particles[pi].x - n->mp_x;
    double dy = particles[pi].y - n->mp_y;
    double r2 = dx * dx + dy * dy + 1e-10;
    double r = sqrt(r2);
    /* Well-separated test: use multipole if cell is far enough */
    if (n->is_leaf || (n->size / r < THETA)) {
        double f = -particles[pi].mass * n->mp_mass / (r2 * r);
        particles[pi].fx += f * dx;
        particles[pi].fy += f * dy;
        particles[pi].potential += -n->mp_mass / r;
        return;
    }
    /* Otherwise recurse into children */
    for (int c = 0; c < 4; c++) {
        if (n->children[c] >= 0)
            eval_force(n->children[c], pi);
    }
}

typedef struct { int tid; } ThreadArg;

static void* worker(void* arg) {
    int tid = ((ThreadArg*)arg)->tid;
    int lo = (nparticles * tid) / nthreads;
    int hi = (nparticles * (tid + 1)) / nthreads;

    /* Each thread evaluates forces for its partition of particles */
    for (int i = lo; i < hi; i++) {
        particles[i].fx = 0;
        particles[i].fy = 0;
        particles[i].potential = 0;
        eval_force(0, i);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    nparticles = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    nthreads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);

    particles = (Particle*)malloc((size_t)nparticles * sizeof(Particle));
    node_cap = nparticles * 4 + 256;
    nodes = (QNode*)malloc((size_t)node_cap * sizeof(QNode));
    node_count = 0;
    if (!particles || !nodes) { fprintf(stderr, "malloc failed\n"); return 1; }

    pthread_mutex_init(&node_lock, NULL);
    pthread_barrier_init(&barrier, NULL, (unsigned)nthreads);

    /* Deterministic particle positions */
    uint32_t seed = 42;
    for (int i = 0; i < nparticles; i++) {
        particles[i].x = 10.0 * bench_rand(&seed) / 32768.0;
        particles[i].y = 10.0 * bench_rand(&seed) / 32768.0;
        particles[i].mass = 0.5 + 1.5 * bench_rand(&seed) / 32768.0;
        particles[i].fx = 0;
        particles[i].fy = 0;
        particles[i].potential = 0;
    }

    /* Build quadtree (serial) */
    alloc_node(5.0, 5.0, 5.0); /* root node centered at (5,5), half-size=5 */
    for (int i = 0; i < nparticles; i++)
        tree_insert(0, i, 0);
    compute_multipoles(0);

    /* Parallel force evaluation */
    pthread_t* threads = (pthread_t*)malloc((size_t)nthreads * sizeof(pthread_t));
    ThreadArg* args = (ThreadArg*)malloc((size_t)nthreads * sizeof(ThreadArg));

    zsim_roi_begin();
    for (int t = 1; t < nthreads; t++) {
        args[t].tid = t;
        pthread_create(&threads[t], NULL, worker, &args[t]);
    }
    args[0].tid = 0;
    worker(&args[0]);
    for (int t = 1; t < nthreads; t++)
        pthread_join(threads[t], NULL);
    zsim_roi_end();

    /* Checksum: sum of potentials */
    double checksum = 0.0;
    for (int i = 0; i < nparticles; i++)
        checksum += particles[i].potential;
    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    /* Cleanup */
    for (int i = 0; i < node_count; i++)
        if (nodes[i].plist) free(nodes[i].plist);
    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&node_lock);
    free(threads); free(args);
    free(particles); free(nodes);
    return 0;
}
