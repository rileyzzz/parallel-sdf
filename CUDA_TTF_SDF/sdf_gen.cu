
#include "cuda.h"
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "sdf_gen.cuh"

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

CUDA_SDF::SDFGenerationContext::~SDFGenerationContext()
{
	cudaFree(data);
	cudaFree(sdf_data);
	//cudaFree(out_data);
	delete[] out_data;
}

void CUDA_SDF::SDFGenerationContext::CopyImage(unsigned char* img_data)
{
	cudaFree(data);
	cudaFree(sdf_data);
	//cudaFree(out_data);
	delete[] out_data;

	size_t img_size = (size_t)width * (size_t)height * (size_t)numComponents;

	cudaMallocManaged(&data, img_size);
	cudaMallocManaged(&sdf_data, img_size * sizeof(float));
	//cudaMallocManaged(&out_data, img_size);
	out_data = new unsigned char[img_size];
	memcpy(data, img_data, img_size);
}

template <typename type>
inline __host__ __device__ type clamp(type d, type min, type max)
{
	const type t = d < min ? min : d;
	return t > max ? max : t;
}

inline __host__ __device__ float distance(int x1, int y1, int x2, int y2)
{
	float x = (float)x2 - (float)x1;
	float y = (float)y2 - (float)y1;

	return sqrt(x * x + y * y);
}

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

__global__ void sdf_calc(int width, int height, unsigned char* data, float* out)
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	int stride = blockDim.x;

#define SAMPLE(_x, _y) data[_y * stride + _x]
#define WRITE(_x, _y, val) out[_y * stride + _x] = val

	bool isBaseSample = SAMPLE(x, y) > 0x00;

	const int maxRadius = 16;

	float dist = distance(0, 0, width, height);
	bool foundSample = false;

	for (int ringSize = 1; ringSize <= maxRadius; ringSize++)
	{
		int fromX	= clamp(x - ringSize, 0, width - 1);
		int toX		= clamp(x + ringSize, 0, width - 1);
		int fromY	= clamp(y - ringSize, 0, height - 1);
		int toY		= clamp(y + ringSize, 0, height - 1);

		// ***
		// ---
		// ---
		for (int i = fromX; i <= toX; i++)
		{
			int uv_x = i;
			int uv_y = fromY;

			unsigned char sample = SAMPLE(uv_x, uv_y);
			if (sample > 0 && !isBaseSample || sample == 0 && isBaseSample)
			{
				dist = min(dist, distance(x, y, uv_x, uv_y));
				foundSample = true;
			}
		}

		// ---
		// ---
		// ***
		for (int i = fromX; i <= toX; i++)
		{
			int uv_x = i;
			int uv_y = toY;

			unsigned char sample = SAMPLE(uv_x, uv_y);
			if (sample > 0 && !isBaseSample || sample == 0 && isBaseSample)
			{
				dist = min(dist, distance(x, y, uv_x, uv_y));
				foundSample = true;
			}
		}

		// ---
		// *--
		// ---
		for (int i = fromY + 1; i < toY; i++)
		{
			int uv_x = fromX;
			int uv_y = i;

			unsigned char sample = SAMPLE(uv_x, uv_y);
			if (sample > 0 && !isBaseSample || sample == 0 && isBaseSample)
			{
				dist = min(dist, distance(x, y, uv_x, uv_y));
				foundSample = true;
			}
		}

		// ---
		// --*
		// ---
		for (int i = fromY + 1; i < toY; i++)
		{
			int uv_x = toX;
			int uv_y = i;

			unsigned char sample = SAMPLE(uv_x, uv_y);
			if (sample > 0 && !isBaseSample || sample == 0 && isBaseSample)
			{
				dist = min(dist, distance(x, y, uv_x, uv_y));
				foundSample = true;
			}
		}

		//if (foundSample)
		//{
		//	break;
		//}
	}

	//dist = 1.0f - (dist / (float)maxRadius);
	//dist = clamp(dist, 0.0f, 1.0f);
	if (!isBaseSample)
		dist = -dist;

	WRITE(x, y, dist);

#undef SAMPLE
#undef WRITE
}

void CUDA_SDF::GenerateSDF(SDFGenerationContext& ctx)
{
	printf("Generating signed distance field.\n");

	int numThreads = ctx.width * ctx.height;
	int blockSize = ctx.width;
	int numBlocks = (numThreads + blockSize - 1) / blockSize;
	//add << <numBlocks, blockSize >> > (N, x, y);

	sdf_calc << < numBlocks, blockSize >> > (ctx.width, ctx.height, ctx.data, ctx.sdf_data);

	cudaDeviceSynchronize();

	float maxDist = distance(0, 0, ctx.width, ctx.height);
	float sdf_min = maxDist;
	float sdf_max = -sdf_min;

	for (int i = 0; i < ctx.width * ctx.height; i++)
	{
		const float& val = ctx.sdf_data[i];

		//if the value is not a max distance sample
		if (val > -maxDist && val < maxDist)
		{
			sdf_min = min(sdf_min, val);
			sdf_max = max(sdf_max, val);
		}
	}

	printf("SDF min %f max %f\n", sdf_min, sdf_max);

	float sdf_range = sdf_max - sdf_min;
	for (int i = 0; i < ctx.width * ctx.height; i++)
	{
		const float& val = ctx.sdf_data[i];
		float normalized_val = (val - sdf_min) / sdf_range;
		//float normalized_val = val / sdf_range * 0.5f + 0.5f;
		normalized_val = clamp(normalized_val, 0.0f, 1.0f);
		ctx.out_data[i] = (unsigned char)(normalized_val * 255.0f);
	}
}

