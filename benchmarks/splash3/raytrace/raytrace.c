/* raytrace.c -- Ray-Sphere Intersection with Phong shading
 * SPLASH-3 style: pthreads parallel. Generate random spheres in a scene,
 * cast primary rays from camera through image grid, find closest hit,
 * compute Phong (ambient + diffuse + specular) shading. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    256
#define DEFAULT_THREADS 4
#define AMBIENT         0.1
#define SPECULAR_EXP    32.0

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
    double cx, cy, cz;  /* center */
    double radius;
    double r, g, b;      /* color */
    double spec;          /* specular coefficient */
} Sphere;

typedef struct {
    double ox, oy, oz;   /* origin */
    double dx, dy, dz;   /* direction (normalized) */
} Ray;

/* Shared state */
static Sphere* spheres;
static int nspheres;
static int img_w, img_h;
static double* framebuf;  /* RGB per pixel */
static int nthreads;

/* Light position */
static const double light_x = 5.0, light_y = 10.0, light_z = -5.0;

/* Camera at (0, 0, -10) looking toward +z */
static const double cam_x = 0.0, cam_y = 0.0, cam_z = -10.0;
static const double screen_z = 0.0;
static const double screen_half = 5.0;

/* Ray-sphere intersection: returns t >= 0 or -1 */
static double intersect(const Ray* ray, const Sphere* sp) {
    double ex = ray->ox - sp->cx;
    double ey = ray->oy - sp->cy;
    double ez = ray->oz - sp->cz;
    double a = ray->dx * ray->dx + ray->dy * ray->dy + ray->dz * ray->dz;
    double b = 2.0 * (ex * ray->dx + ey * ray->dy + ez * ray->dz);
    double c = ex * ex + ey * ey + ez * ez - sp->radius * sp->radius;
    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return -1.0;
    double sq = sqrt(disc);
    double t1 = (-b - sq) / (2.0 * a);
    double t2 = (-b + sq) / (2.0 * a);
    if (t1 > 1e-6) return t1;
    if (t2 > 1e-6) return t2;
    return -1.0;
}

static double vec_len(double x, double y, double z) {
    return sqrt(x * x + y * y + z * z);
}

typedef struct { int tid; } ThreadArg;

static void* worker(void* arg) {
    int tid = ((ThreadArg*)arg)->tid;
    /* Partition image rows */
    int row_lo = (img_h * tid) / nthreads;
    int row_hi = (img_h * (tid + 1)) / nthreads;

    for (int py = row_lo; py < row_hi; py++) {
        for (int px = 0; px < img_w; px++) {
            /* Map pixel to screen coords */
            double sx = -screen_half + 2.0 * screen_half * px / (img_w - 1);
            double sy =  screen_half - 2.0 * screen_half * py / (img_h - 1);
            Ray ray;
            ray.ox = cam_x; ray.oy = cam_y; ray.oz = cam_z;
            double rdx = sx - cam_x;
            double rdy = sy - cam_y;
            double rdz = screen_z - cam_z;
            double rlen = vec_len(rdx, rdy, rdz);
            ray.dx = rdx / rlen;
            ray.dy = rdy / rlen;
            ray.dz = rdz / rlen;

            /* Find closest sphere hit */
            double best_t = 1e30;
            int best_s = -1;
            for (int s = 0; s < nspheres; s++) {
                double t = intersect(&ray, &spheres[s]);
                if (t >= 0 && t < best_t) {
                    best_t = t;
                    best_s = s;
                }
            }

            int pidx = (py * img_w + px) * 3;
            if (best_s < 0) {
                /* Background: dark gray */
                framebuf[pidx + 0] = 0.05;
                framebuf[pidx + 1] = 0.05;
                framebuf[pidx + 2] = 0.07;
                continue;
            }

            /* Hit point and normal */
            double hx = ray.ox + best_t * ray.dx;
            double hy = ray.oy + best_t * ray.dy;
            double hz = ray.oz + best_t * ray.dz;
            double nx = hx - spheres[best_s].cx;
            double ny = hy - spheres[best_s].cy;
            double nz = hz - spheres[best_s].cz;
            double nlen = vec_len(nx, ny, nz);
            nx /= nlen; ny /= nlen; nz /= nlen;

            /* Light direction */
            double lx = light_x - hx;
            double ly = light_y - hy;
            double lz = light_z - hz;
            double llen = vec_len(lx, ly, lz);
            lx /= llen; ly /= llen; lz /= llen;

            /* Diffuse */
            double ndotl = nx * lx + ny * ly + nz * lz;
            if (ndotl < 0.0) ndotl = 0.0;

            /* Specular (Phong reflection) */
            double rx = 2.0 * ndotl * nx - lx;
            double ry = 2.0 * ndotl * ny - ly;
            double rz = 2.0 * ndotl * nz - lz;
            double vx = -ray.dx, vy = -ray.dy, vz = -ray.dz;
            double rdotv = rx * vx + ry * vy + rz * vz;
            if (rdotv < 0.0) rdotv = 0.0;
            double spec = spheres[best_s].spec * pow(rdotv, SPECULAR_EXP);

            framebuf[pidx + 0] = AMBIENT * spheres[best_s].r + ndotl * spheres[best_s].r + spec;
            framebuf[pidx + 1] = AMBIENT * spheres[best_s].g + ndotl * spheres[best_s].g + spec;
            framebuf[pidx + 2] = AMBIENT * spheres[best_s].b + ndotl * spheres[best_s].b + spec;
            /* Clamp */
            for (int c = 0; c < 3; c++)
                if (framebuf[pidx + c] > 1.0) framebuf[pidx + c] = 1.0;
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    int size = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    nthreads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);

    img_w = size;
    img_h = size;
    nspheres = (int)sqrt((double)size);
    if (nspheres < 4) nspheres = 4;

    spheres = (Sphere*)malloc((size_t)nspheres * sizeof(Sphere));
    framebuf = (double*)malloc((size_t)img_w * img_h * 3 * sizeof(double));
    if (!spheres || !framebuf) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Deterministic sphere generation */
    uint32_t seed = 42;
    for (int i = 0; i < nspheres; i++) {
        spheres[i].cx = -4.0 + 8.0 * bench_rand(&seed) / 32768.0;
        spheres[i].cy = -4.0 + 8.0 * bench_rand(&seed) / 32768.0;
        spheres[i].cz = 2.0 + 6.0 * bench_rand(&seed) / 32768.0;
        spheres[i].radius = 0.3 + 1.2 * bench_rand(&seed) / 32768.0;
        spheres[i].r = 0.2 + 0.8 * bench_rand(&seed) / 32768.0;
        spheres[i].g = 0.2 + 0.8 * bench_rand(&seed) / 32768.0;
        spheres[i].b = 0.2 + 0.8 * bench_rand(&seed) / 32768.0;
        spheres[i].spec = 0.3 + 0.7 * bench_rand(&seed) / 32768.0;
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

    /* Checksum: sum of all pixel RGB values */
    double checksum = 0.0;
    for (int i = 0; i < img_w * img_h * 3; i++)
        checksum += framebuf[i];
    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    free(threads); free(args);
    free(spheres); free(framebuf);
    return 0;
}
