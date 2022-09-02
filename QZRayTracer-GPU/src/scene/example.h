#ifndef QZRT_SCENE_EXAMPLE_H
#define QZRT_SCENE_EXAMPLE_H

#include "../core/QZRayTracer.h"
#include "../core/geometry.h"
#include "../core/api.h"
#include "cuda_runtime.h"
#include "device_launch_parameters.h"

namespace raytracer {
#define RND (curand_uniform(&local_rand_state))

	__global__ void rand_init(curandState* rand_state) {
		if (threadIdx.x == 0 && blockIdx.x == 0) {
			curand_init(1999, 0, 0, rand_state);
		}
	}

	__global__ void free_world(Shape** d_list, Shape** d_world, Camera** d_camera) {
		for (int i = 0; i < ((ShapeList*)d_world)->numShapes; i++) {
			delete d_list[i]->material;
			delete d_list[i];
		}
		delete* d_world;
		delete* d_camera;
	}

	__global__ void create_world(Shape** d_list, Shape** d_world, Camera** d_camera, int nx, int ny) {
		if (threadIdx.x == 0 && blockIdx.x == 0) {
			int curNum = 0;
			d_list[0] = new Sphere(Point3f(0, 0, -1), 0.5,
				new Lambertian(Point3f(0.1, 0.2, 0.5)));
			curNum++;
			d_list[1] = new Sphere(Point3f(0, -100.5, -1), 100,
				new Lambertian(Point3f(0.8, 0.8, 0.0)));
			d_list[2] = new Sphere(Point3f(1, 0, -1), 0.5,
				new Metal(Point3f(0.8, 0.6, 0.2), 0.0));
			d_list[3] = new Sphere(Point3f(-1, 0, -1), 0.5,
				new Dielectric(1.5));
			d_list[4] = new Sphere(Point3f(-1, 0, -1), -0.45,
				new Dielectric(1.5));
			*d_world = new ShapeList(d_list, 5);
			Point3f lookfrom(-2, 2, 1);
			Point3f lookat(0, 0, -1);
			float dist_to_focus = (lookfrom - lookat).Length();
			float aperture = 0;
			*d_camera = new Camera(lookfrom,
				lookat,
				Vector3f(0, 1, 0),
				20.0,
				float(nx) / float(ny),
				aperture,
				dist_to_focus);
		}
	}

	

	__global__ void SampleScene(Shape** shapes, Shape** world, Camera** camera, int width, int height, curandState* rand_state) {
		if (threadIdx.x == 0 && blockIdx.x == 0) {
			curandState local_rand_state = *rand_state;
			Point3f lookFrom = Point3f(13, 2, 3);
			Point3f lookAt = Point3f(0, 0, 0);
			Vector3f lookUp = Vector3f(0, 1, 0);
			Float aperture = 0.0;
			Float fov = 20.0;
			Float focusDis = 10.0;
			Float screenWidth = width;
			Float screenHeight = height;
			Float aspect = screenWidth / screenHeight;
			*camera = new Camera(lookFrom, lookAt, lookUp, fov, aspect, aperture, focusDis);

			int curNum = 0; // ��¼������Shape����
			shapes[curNum++] = new Sphere(Point3f(0, -1000, 0), 1000, new Lambertian(Point3f(0.5, 0.5, 0.5)));


			for (int a = -11; a < 11; a++) {
				for (int b = -11; b < 11; b++) {
					Float chooseMat = RND;
					Point3f center = Point3f(a + 0.9 * RND, 0.2, b + 0.9 * RND);
					if ((center - Point3f(4, 0.2, 0)).Length() > 0.9) {
						if (chooseMat < 0.7) { // ѡ�����������
							shapes[curNum++] = new Sphere(
									center,
									0.2,
									new Lambertian(Point3f(RND * RND, RND * RND, RND * RND)));
						}
						else if (chooseMat < 0.85) { // ѡ���������
							shapes[curNum++] = new Sphere(
									center,
									0.2,
									new Metal(Point3f(0.5 * (1 + RND), 0.5 * (1 + RND), 0.5 * (1 + RND)), RND));
						}
						else if (chooseMat < 0.95) { // ѡ��������
							shapes[curNum++] = new Sphere(
									center,
									0.2,
									new Dielectric(1 + RND));
						}
						else { // ѡ���пղ��������
							shapes[curNum++] = new Sphere(
									center,
									0.2,
									new Dielectric(1.5));
							shapes[curNum++] = new Sphere(
									center,
									-0.15,
									new Dielectric(1.5));
						}
					}
				}

			}

			shapes[curNum++] = new Sphere(Point3f(0, 1, 0), 1.0, new Dielectric(1.5));
			shapes[curNum++] = new Sphere(Point3f(-4, 1, 0), 1.0, new Lambertian(Point3f(0.4, 0.2, 0.1)));
			shapes[curNum++] = new Sphere(Point3f(4, 1, 0), 1.0, new Metal(Point3f(0.7, 0.6, 0.5), 0.0));
			*rand_state = local_rand_state;
			*world = new ShapeList(shapes, curNum);
		}
	}

	__global__ void ShapeTestCylinderScene(Shape** shapes, Shape** world, Camera** camera, int width, int height, curandState* rand_state) {
		if (threadIdx.x == 0 && blockIdx.x == 0) {
			curandState local_rand_state = *rand_state;
			// ��������
			Point3f lookFrom = Point3f(-1, 3, 6);
			Point3f lookAt = Point3f(0, 0, -1);
			Vector3f lookUp = Vector3f(0, 1, 0);
			Float aperture = 0.0;
			Float fov = 80.0;
			Float focusDis = (lookFrom - lookAt).Length();
			Float screenWidth = width;
			Float screenHeight = height;
			Float aspect = screenWidth / screenHeight;
			*camera = new Camera(lookFrom, lookAt, lookUp, fov, aspect, aperture, focusDis);

			// �����е���������

			int curNum = 0; // ��¼������Shape����
			shapes[curNum++] = new Sphere(Point3f(0, -1000, 0), 1000, new Lambertian(Point3f(0.980392, 0.694118, 0.627451)));
			shapes[curNum++] = new Cylinder(Point3f(0, 0.1, 0), 4.0, 0.0, 0.2, new Lambertian(Point3f(0.882353, 0.439216, 0.333333)));
			shapes[curNum++] = new Cylinder(Point3f(0, 0.25, 0), 3.0, 0.0, 0.1, new Metal(Point3f(0.423529, 0.360784, 0.905882), 0));
			shapes[curNum++] = new Cylinder(Point3f(2.5, 1.7, 2.5), 0.2, 0.0, 3.0, new Metal(Point3f(0.0352941, 0.517647, 0.890196), 0.6));
			shapes[curNum++] = new Cylinder(Point3f(2.5, 1.7, -2.5), 0.2, 0.0, 3.0, new Lambertian(Point3f(0, 0.807843, 0.788235)));
			shapes[curNum++] = new Cylinder(Point3f(-2.5, 1.7, 2.5), 0.2, 0.0, 3.0, new Dielectric(1.5));
			shapes[curNum++] = new Cylinder(Point3f(-2.5, 1.7, -2.5), 0.2, 0.0, 3.0, new Metal(Point3f(0.992157, 0.47451, 0.658824), 0.3));
			shapes[curNum++] = new Cylinder(Point3f(0, 3.3, 0), 4.0, 0.0, 0.2, new Metal(Point3f(0.839216, 0.188235, 0.192157), 0.5));

			// Բ���ϵ���
			for (int a = -3; a < 3; a++) {
				for (int b = -3; b < 3; b++) {
					Float chooseMat = RND;
					Point3f center = Point3f(a + 0.9 * RND, 0.3 + (0.1 + 0.2 * RND), b + 0.9 * RND);
					if ((center - Point3f(0, center.y, 0)).Length() > 0.6 + (center.y - 0.35) && (center - Point3f(0, center.y, 0)).Length() <= 3.0 - (center.y - 0.35)) {
						if (chooseMat < 0.7) { // ѡ�����������
							shapes[curNum++] = new Sphere(
								center,
								center.y - 0.3,
								new Lambertian(Point3f(RND * RND, RND * RND, RND * RND)));
						}
						else if (chooseMat < 0.85) { // ѡ���������
							shapes[curNum++] = new Sphere(
								center,
								center.y - 0.3,
								new Metal(Point3f(0.5 * (1 + RND), 0.5 * (1 + RND), 0.5 * (1 + RND)), RND));
						}
						else if (chooseMat < 0.95) { // ѡ��������
							shapes[curNum++] = new Sphere(
								center,
								center.y - 0.3,
								new Dielectric(1 + RND));
						}
						else { // ѡ���пղ��������
							shapes[curNum++] = new Sphere(
								center,
								center.y - 0.3,
								new Dielectric(1.5));
							shapes[curNum++] = new Sphere(
								center,
								0.4 - center.y,
								new Dielectric(1.5));
						}
					}
				}
			}



			// Բ���ϵ�Բ��

			shapes[curNum++] = new Cylinder(Point3f(0, 0.375, 0), 0.6, 0.0, 0.15, new Lambertian(Point3f(1.0, 1.0, 1.0)));
			shapes[curNum++] = new Cylinder(Point3f(0, 0.525, 0), 0.5, 0.0, 0.15, new Lambertian(Point3f(0.1, 0.1, 0.1)));
			shapes[curNum++] = new Cylinder(Point3f(0, 0.675, 0), 0.4, 0.0, 0.15, new Lambertian(Point3f(0.9, 0.9, 0.9)));
			shapes[curNum++] = new Cylinder(Point3f(0, 0.825, 0), 0.3, 0.0, 0.15, new Metal(Point3f(0.827451, 0.329412, 0), 0.3));
			shapes[curNum++] = new Cylinder(Point3f(0, 1.2625, 0), 0.2, 0.0, 0.575, new Dielectric(1.5));
			shapes[curNum++] = new Sphere(Point3f(0, 1.75, 0), 0.20, new Metal(Point3f(0.752941, 0.223529, 0.168627), 0));
			shapes[curNum++] = new Cylinder(Point3f(0, 2.2375, 0), 0.2, 0.0, 0.575, new Dielectric(1.5));
			shapes[curNum++] = new Cylinder(Point3f(0, 2.6, 0), 0.3, 0.0, 0.15, new Metal(Point3f(0.827451, 0.329412, 0), 0.3));
			shapes[curNum++] = new Cylinder(Point3f(0, 2.75, 0), 0.4, 0.0, 0.15, new Lambertian(Point3f(0.9, 0.9, 0.9)));
			shapes[curNum++] = new Cylinder(Point3f(0, 2.9, 0), 0.5, 0.0, 0.15, new Lambertian(Point3f(0.1, 0.1, 0.1)));
			shapes[curNum++] = new Cylinder(Point3f(0, 3.05, 0), 0.6, 0.0, 0.15, new Lambertian(Point3f(1.0, 1.0, 1.0)));

			// Բ���µ���
			for (Float a = -4; a < 4; a += 0.5) {
				for (Float b = -4; b < 4; b++) {
					Float chooseMat = RND;
					Point3f center = Point3f(a + 0.9 * RND, 0.2 + (0.1 + 0.25 * RND), b + 0.9 * RND);
					if ((center - Point3f(0, center.y, 0)).Length() <= 4.0 - (center.y - 0.2) &&
						(center - Point3f(0, center.y, 0)).Length() >= 3.0 + (center.y - 0.2) &&
						(center - Point3f(2.5, center.y, 2.5)).Length() >= center.y  &&
						(center - Point3f(-2.5, center.y, 2.5)).Length() >= center.y  &&
						(center - Point3f(2.5, center.y, -2.5)).Length() >= center.y  &&
						(center - Point3f(-2.5, center.y, -2.5)).Length() >= center.y ) {
						if (chooseMat < 0.7) { // ѡ�����������
							shapes[curNum++] = new Sphere(
								center,
								center.y - 0.2,
								new Lambertian(Point3f(RND * RND, RND * RND, RND * RND)));
						}
						else if (chooseMat < 0.85) { // ѡ���������
							shapes[curNum++] = new Sphere(
								center,
								center.y - 0.2,
								new Metal(Point3f(0.5 * (1 + RND), 0.5 * (1 + RND), 0.5 * (1 + RND)), RND));
						}
						else if (chooseMat < 0.95) { // ѡ��������
							shapes[curNum++] = new Sphere(
								center,
								center.y - 0.2,
								new Dielectric(1 + RND));
						}
						else { // ѡ���пղ��������
							shapes[curNum++] = new Sphere(
								center,
								center.y - 0.2,
								new Dielectric(1.5));
							shapes[curNum++] = new Sphere(
								center,
								0.25 - center.y,
								new Dielectric(1.5));
						}
					}
				}

			}
			*rand_state = local_rand_state;
			*world = new ShapeList(shapes, curNum);
		}
	}
	
}


#endif // QZRT_SCENE_EXAMPLE_H