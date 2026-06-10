/* volrend.c -- Volume Rendering via ray-marching through 3D voxel grid
 * SPLASH-3 style: pthreads parallel. Generate synthetic 3D volume (sphere
 * density falloff), cast rays from camera, march along each ray sampling
 * density with trilinear interpolation, front-to-back compositing. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    64
#define DEFAULT_THREADS 4
#define STEP_SIZE       0.5   /* ray march step in voxel units */
#define OPACITY_SCALE   0.04  /* density -> opacity multiplier */
#define MAX_OPACITY     0.98  /* early ray termination threshold */

static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

static uint32_t bench_rand(uint32_t* s) {
    *s = *s * 1103515245 + 12345;
    return (*s >> 16) & 0x7FFF;
}

/* Shared state */
static double* volume;     /* NxNxN density grid */
static double* image;      /* img_w x img_h x 4 (RGBA) */
static int vol_n, img_w, img_h;
static int nthreads;

/* Access volume with bounds check */
static double vol_at(int x, int y, int z) {
    if (x < 0 || x >= vol_n || y < 0 || y >= vol_n || z < 0 || z >= vol_n)
        return 0.0;
    return volume[x * vol_n * vol_n + y * vol_n + z];
}

/* Trilinear interpolation */
static double trilinear(double fx, double fy, double fz) {
    int x0 = (int)fx, y0 = (int)fy, z0 = (int)fz;
    double xf = fx - x0, yf = fy - y0, zf = fz - z0;
    double c000 = vol_at(x0,     y0,     z0);
    double c100 = vol_at(x0 + 1, y0,     z0);
    double c010 = vol_at(x0,     y0 + 1, z0);
    double c110 = vol_at(x0 + 1, y0 + 1, z0);
    double c001 = vol_at(x0,     y0,     z0 + 1);
    double c101 = vol_at(x0 + 1, y0,     z0 + 1);
    double c011 = vol_at(x0,     y0 + 1, z0 + 1);
    double c111 = vol_at(x0 + 1, y0 + 1, z0 + 1);
    double c00 = c000 * (1 - xf) + c100 * xf;
    double c10 = c010 * (1 - xf) + c110 * xf;
    double c01 = c001 * (1 - xf) + c101 * xf;
    double c11 = c011 * (1 - xf) + c111 * xf;
    double c0 = c00 * (1 - yf) + c10 * yf;
    double c1 = c01 * (1 - yf) + c11 * yf;
    return c0 * (1 - zf) + c1 * zf;
}

/* Transfer function: density -> color (simple gradient) */
static void transfer_function(double density, double* r, double* g, double* b) {
    /* Low density: blue, mid: green, high: red/white */
    if (density < 0.33) {
        *r = 0.1;
        *g = 0.2 + density * 2.0;
        *b = 0.5 + density;
    } else if (density < 0.66) {
        double t = (density - 0.33) * 3.0;
        *r = 0.2 + t * 0.6;
        *g = 0.8 - t * 0.3;
        *b = 0.5 - t * 0.4;
    } else {
        double t = (density - 0.66) * 3.0;
        *r = 0.8 + t * 0.2;
        *g = 0.5 + t * 0.3;
        *b = 0.1 + t * 0.2;
    }
}

typedef struct { int tid; } ThreadArg;

static void* worker(void* arg) {
    int tid = ((ThreadArg*)arg)->tid;
    int row_lo = (img_h * tid) / nthreads;
    int row_hi = (img_h * (tid + 1)) / nthreads;

    double half_n = vol_n * 0.5;
    /* Camera: looking along +z, centered on volume */
    double cam_z = -vol_n * 1.5;
    double screen_dist = vol_n * 1.0;

    for (int py = row_lo; py < row_hi; py++) {
        for (int px = 0; px < img_w; px++) {
            /* Ray origin and direction */
            double sx = (px - img_w * 0.5);
            double sy = (py - img_h * 0.5);
            double dx = sx, dy = sy, dz = screen_dist;
            double dlen = sqrt(dx * dx + dy * dy + dz * dz);
            dx /= dlen; dy /= dlen; dz /= dlen;

            double ox = half_n, oy = half_n, oz = cam_z;

            /* Find entry/exit with volume bounding box [0, vol_n-1]^3 */
            double tmin = 0, tmax = 1e30;
            double inv[3] = {1.0 / (dx + 1e-30), 1.0 / (dy + 1e-30), 1.0 / (dz + 1e-30)};
            double orig[3] = {ox, oy, oz};
            for (int a = 0; a < 3; a++) {
                double t1 = (0 - orig[a]) * inv[a];
                double t2 = ((vol_n - 1) - orig[a]) * inv[a];
                if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
            }

            int pidx = (py * img_w + px) * 4;
            if (tmin >= tmax || tmax < 0) {
                image[pidx + 0] = 0; image[pidx + 1] = 0;
                image[pidx + 2] = 0; image[pidx + 3] = 0;
                continue;
            }
            if (tmin < 0) tmin = 0;

            /* Front-to-back compositing */
            double acc_r = 0, acc_g = 0, acc_b = 0, acc_a = 0;
            double t = tmin;
            while (t < tmax && acc_a < MAX_OPACITY) {
                double fx = ox + t * dx;
                double fy = oy + t * dy;
                double fz = oz + t * dz;
                double density = trilinear(fx, fy, fz);
                if (density > 0.01) {
                    double cr, cg, cb;
                    transfer_function(density, &cr, &cg, &cb);
                    double alpha = density * OPACITY_SCALE;
                    if (alpha > 1.0) alpha = 1.0;
                    /* Front-to-back: C_out = C_in + (1 - a_in) * alpha * color */
                    double one_minus_a = 1.0 - acc_a;
                    acc_r += one_minus_a * alpha * cr;
                    acc_g += one_minus_a * alpha * cg;
                    acc_b += one_minus_a * alpha * cb;
                    acc_a += one_minus_a * alpha;
                }
                t += STEP_SIZE;
            }
            image[pidx + 0] = acc_r;
            image[pidx + 1] = acc_g;
            image[pidx + 2] = acc_b;
            image[pidx + 3] = acc_a;
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    int size = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    nthreads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);

    vol_n = size;
    img_w = size;
    img_h = size;

    volume = (double*)malloc((size_t)vol_n * vol_n * vol_n * sizeof(double));
    image = (double*)malloc((size_t)img_w * img_h * 4 * sizeof(double));
    if (!volume || !image) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Generate synthetic volume: sphere density falloff + noise */
    uint32_t seed = 42;
    double center = vol_n * 0.5;
    double max_r = vol_n * 0.4;
    for (int x = 0; x < vol_n; x++) {
        for (int y = 0; y < vol_n; y++) {
            for (int z = 0; z < vol_n; z++) {
                double dx = x - center;
                double dy = y - center;
                double dz = z - center;
                double r = sqrt(dx * dx + dy * dy + dz * dz);
                double base = (r < max_r) ? 1.0 - r / max_r : 0.0;
                /* Add some noise for more interesting rendering */
                double noise = 0.1 * (bench_rand(&seed) / 32768.0 - 0.5);
                double val = base + noise;
                if (val < 0.0) val = 0.0;
                if (val > 1.0) val = 1.0;
                volume[x * vol_n * vol_n + y * vol_n + z] = val;
            }
        }
    }

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

    /* Checksum: sum of all pixel RGBA values */
    double checksum = 0.0;
    for (int i = 0; i < img_w * img_h * 4; i++)
        checksum += image[i];
    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    free(threads); free(args);
    free(volume); free(image);
    return 0;
}
