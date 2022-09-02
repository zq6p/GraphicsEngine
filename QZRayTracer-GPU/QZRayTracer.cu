﻿
#include <iostream>
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "./src/core/QZRayTracer.h"
#include "./src/core/api.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "./src/core/stb_image.h"
#include "./src/core/stb_image_write.h"
#include "src/scene/example.h"

using namespace raytracer;
using namespace std;
#define MAXBOUNDTIME 50
#define MAXNUMSHAPE 1000

// GPU Mode
// limited version of checkCudaErrors from helper_cuda.h in CUDA examples
#define checkCudaErrors(val) check_cuda( (val), #val, __FILE__, __LINE__ )

void check_cuda(cudaError_t result, char const* const func, const char* const file, int const line) {
    if (result) {
        std::cerr << "CUDA error = " << static_cast<unsigned int>(result) << " at " <<
            file << ":" << line << " '" << func << "' \n";
        // Make sure we call CUDA Device Reset before exiting
        cudaDeviceReset();
        exit(99);
    }
}

__device__ Point3f Color(const Ray& r, Shape** world, curandState* local_rand_state) {
    Ray cur_ray = r;
    Point3f cur_attenuation = Point3f(1.0, 1.0, 1.0);
    for (int i = 0; i < 50; i++) {
        HitRecord rec;
        if ((*world)->Hit(cur_ray, rec)) {
            Ray scattered;
            Point3f attenuation;
            Point3f target = rec.p + Point3f(rec.normal) + RandomInUnitSphere(local_rand_state);
            if (rec.mat->Scatter(cur_ray, rec, attenuation, scattered, local_rand_state)) {
                cur_attenuation = cur_attenuation * attenuation;
                cur_ray = scattered;
            }
            else {
                return Point3f(0.0, 0.0, 0.0);
            }
        }
        else {
            Vector3f unit_direction = Normalize(cur_ray.d);
            float t = 0.5f * (unit_direction.y + 1.0f);
            Point3f c = (1.0f - t) * Point3f(1.0, 1.0, 1.0) + t * Point3f(0.5, 0.7, 1.0);
            return cur_attenuation * c;
        }
    }
        
    return Point3f(0.0, 0.0, 0.0); // exceeded recursion
}

__global__ void render_init(int max_x, int max_y, curandState* rand_state) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;
    if ((i >= max_x) || (j >= max_y)) return;
    int pixel_index = j * max_x + i;
    //Each thread gets same seed, a different sequence number, no offset
    curand_init(1999, pixel_index, 0, &rand_state[pixel_index]);
}


__global__ void render(Point3f* fb, int max_x, int max_y, int ns, Camera** cam, Shape** world, curandState* rand_state) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;
    if ((i >= max_x) || (j >= max_y)) return;
    int pixel_index = j * max_x + i;
    curandState local_rand_state = rand_state[pixel_index];
    Point3f color;
    for (int s = 0; s < ns; s++) {
        Float u = Float(i + curand_uniform(&local_rand_state)) / Float(max_x);
        Float v = Float(/*max_y -*/ j /*- 1*/ + curand_uniform(&local_rand_state)) / Float(max_y);
        Ray ray = (*cam)->GenerateRay(u, v, &local_rand_state);
        color += Color(ray, world, &local_rand_state);
    }

    rand_state[pixel_index] = local_rand_state;
    color /= Float(ns);
    color = Point3f(pow(color.x, Gamma), pow(color.y, Gamma), pow(color.z, Gamma)); // gamma矫正
    fb[pixel_index] = color;
}

int main() {
    int nx = 2400;
    int ny = 1600;
    int ns = 1;
    int tx = 32;
    int ty = 32;

    std::cerr << "Rendering a " << nx << "x" << ny << " image with " << ns << " samples per pixel ";
    std::cerr << "in " << tx << "x" << ty << " blocks.\n";

    // allocate FB
    int num_pixels = nx * ny;
    size_t fb_size = num_pixels * sizeof(Point3f);

    Point3f* fb;
    checkCudaErrors(cudaMallocManaged((void**)&fb, fb_size));

    // allocate random state
    curandState* d_rand_state;
    checkCudaErrors(cudaMalloc((void**)&d_rand_state, num_pixels * sizeof(curandState)));
    curandState* d_rand_state2;
    checkCudaErrors(cudaMalloc((void**)&d_rand_state2, 1 * sizeof(curandState)));

    // we need that 2nd random state to be initialized for the world creation
    rand_init << <1, 1 >> > (d_rand_state2);
    checkCudaErrors(cudaGetLastError());
    checkCudaErrors(cudaDeviceSynchronize());

    // make our world of hitables
    Shape** d_list;
    checkCudaErrors(cudaMalloc((void**)&d_list, MAXNUMSHAPE * sizeof(Shape*)));
    Shape** d_world;
    checkCudaErrors(cudaMalloc((void**)&d_world, sizeof(Shape*)));
    Camera** d_camera;
    checkCudaErrors(cudaMalloc((void**)&d_camera, sizeof(Camera*)));
    
    /*--------------------------更换自己的场景--------------------------*/
    ShapeTestCylinderScene <<<1, 1>>>(d_list, d_world, d_camera, nx, ny, d_rand_state2);
    //SampleScene<<<1, 1>>>(d_list, d_world, d_camera, nx, ny, d_rand_state2);
    // create_world << <1, 1 >> > (d_list, d_world, d_camera, nx, ny);
    /*------------------------------end--------------------------------*/
    
    
    checkCudaErrors(cudaGetLastError());
    checkCudaErrors(cudaDeviceSynchronize());

    clock_t start, stop;
    start = clock();
    // Render our buffer
    dim3 blocks(nx / tx + 1, ny / ty + 1);
    dim3 threads(tx, ty);
    render_init << <blocks, threads >> > (nx, ny, d_rand_state);
    checkCudaErrors(cudaGetLastError());
    checkCudaErrors(cudaDeviceSynchronize());
    render <<<blocks, threads>>> (fb, nx, ny, ns, d_camera, d_world, d_rand_state);
    checkCudaErrors(cudaGetLastError());
    checkCudaErrors(cudaDeviceSynchronize());
    stop = clock();
    double timer_seconds = ((double)(stop - start)) / CLOCKS_PER_SEC;
    std::cerr << "took " << timer_seconds << " seconds.\n";
    auto* data = (unsigned char*)malloc(nx * ny * 3);
    for (int j = ny - 1; j >= 0; j--) {
        for (int i = 0; i < nx; i++) {
            size_t pixel_index = j * nx + i;
            int ir = int(255.99 * fb[pixel_index].x);
            int ig = int(255.99 * fb[pixel_index].y);
            int ib = int(255.99 * fb[pixel_index].z);
            size_t shadingPoint = ((ny - j - 1) * nx + i) * 3;
            data[shadingPoint + 0] = ir;
            data[shadingPoint + 1] = ig;
            data[shadingPoint + 2] = ib;
        }
    }
    // 写入图像
    raytracer::stbi_write_png("./output/GPU/Cylinder-spp1.png", nx, ny, 3, data, 0);
    raytracer::stbi_image_free(data);

    // clean up
    checkCudaErrors(cudaDeviceSynchronize());
    free_world <<<1, 1 >>> (d_list, d_world, d_camera);
    checkCudaErrors(cudaGetLastError());;
    checkCudaErrors(cudaFree(d_camera));
    checkCudaErrors(cudaFree(d_list));
    checkCudaErrors(cudaFree(d_world));
    checkCudaErrors(cudaFree(d_rand_state));
    checkCudaErrors(cudaFree(d_rand_state2));
    checkCudaErrors(cudaFree(fb));

    // useful for cuda-memcheck --leak-check full
    cudaDeviceReset();
}

#pragma endregion



