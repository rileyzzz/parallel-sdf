
#include <stdio.h>


#ifndef _SDF_GEN_H_
#define _SDF_GEN_H_
#pragma once

namespace CUDA_SDF
{
	struct SDFGenerationContext
	{
		int width = 0;
		int height = 0;
		int numComponents = 1;

		unsigned char* data = nullptr;
		float* sdf_data = nullptr;
		unsigned char* out_data = nullptr;

		~SDFGenerationContext();


		void CopyImage(unsigned char* img_data);
	};

	void GenerateSDF(SDFGenerationContext& ctx);
}

#endif