// CUDA_TTF_SDF.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <stdio.h>
#include <string>
#include "sdf_gen.cuh"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _MSC_VER
#define STB_MSC_SECURE_CRT
#define __STDC_LIB_EXT1__
#endif //_MSC_VER
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <ft2build.h>

#include <freetype/freetype.h>
#include <freetype/ftoutln.h>

#ifdef _MSC_VER
#include <windows.h>
#endif //_MSC_VER

FT_Long GenerateGlyphAtlas(FT_Face face, int char_offset, int target_width, int target_height, int font_height, unsigned char* data);

#define INVALID_ARG_TEXT "Invalid number of arguments.\n"
int main(int argc, char** argv)
{

	char* font_src = nullptr;
	char* img_src = nullptr;

	bool paginate = false;

	if (argc <= 1)
	{
		printf("Invalid number of arguments.\n");
		return EXIT_FAILURE;
	}

#ifdef _MSC_VER
	char appPath[MAX_PATH + 1] = { 0 };
	GetModuleFileNameA(NULL, appPath, MAX_PATH);
#else
	char* appPath = argv[0];
#endif //_MSC_VER
	
	std::string strAppPath(appPath);
	std::string appDir = strAppPath.substr(0, strAppPath.find_last_of("\\/"));

	int target_width = 1024;
	int target_height = 1024;
	int glyph_size = 32;

	//printf("Application dir %s\n", appDir.c_str());

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-font") == 0)
		{
			if (i + 1 < argc)
				font_src = argv[i + 1];
			else
			{
				printf(INVALID_ARG_TEXT);
				return EXIT_FAILURE;
			}
		}
		else if (strcmp(argv[i], "-img") == 0)
		{
			if (i + 1 < argc)
				img_src = argv[i + 1];
			else
			{
				printf(INVALID_ARG_TEXT);
				return EXIT_FAILURE;
			}
		}
		else if (strcmp(argv[i], "-w") == 0)
		{
			if (i + 1 < argc)
				target_width = std::stoi(argv[i + 1]);
			else
			{
				printf(INVALID_ARG_TEXT);
				return EXIT_FAILURE;
			}
		}
		else if (strcmp(argv[i], "-h") == 0)
		{
			if (i + 1 < argc)
				target_height = std::stoi(argv[i + 1]);
			else
			{
				printf(INVALID_ARG_TEXT);
				return EXIT_FAILURE;
			}
		}
		else if (strcmp(argv[i], "-glyphsize") == 0)
		{
			if (i + 1 < argc)
				glyph_size = std::stoi(argv[i + 1]);
			else
			{
				printf(INVALID_ARG_TEXT);
				return EXIT_FAILURE;
			}
		}
		else if (strcmp(argv[i], "-p") == 0)
			paginate = true;
	}

	CUDA_SDF::SDFGenerationContext ctx{};

	if (font_src)
	{
		FT_Library library;
		FT_Error error;

		if (FT_Init_FreeType(&library))
		{
			printf("Error initializing freetype.\n");
			return EXIT_FAILURE;
		}

		FT_Face face;
		
		std::string fontPath(font_src);

		error = FT_New_Face(library,
			font_src,
			0,
			&face);

		if (error == FT_Err_Unknown_File_Format)
		{
			printf("Unsupported font format.\n");
			return EXIT_FAILURE;
		}
		else if (error)
		{
			printf("Error reading font file.\n");
			return EXIT_FAILURE;
		}

		ctx.width = target_width;
		ctx.height = target_height;

		size_t img_size = (size_t)ctx.width * (size_t)ctx.height;
		unsigned char* img_data = new unsigned char[img_size];

		int offset = 0;
		int page = 0;
		while (offset != -1)
		{
			printf("Generating page %d\n", page);

			memset(img_data, 0, img_size);
			offset = GenerateGlyphAtlas(face, offset, ctx.width, ctx.height, glyph_size, img_data);

			ctx.CopyImage(img_data);

			CUDA_SDF::GenerateSDF(ctx);

			std::string filePath = fontPath;
			if(filePath.find("\\") != std::string::npos || filePath.find("/") != std::string::npos)
				filePath = filePath.substr(filePath.find_last_of("\\/") + 1);
			
			//truncate extension
			if (filePath.find(".") != std::string::npos)
				filePath = filePath.substr(0, filePath.find_last_of("."));

			filePath = appDir + "\\" + filePath;

			if (paginate)
				filePath += "_page" + std::to_string(page++);
			filePath += "_sdf.png";

			printf("writing to %s\n", filePath.c_str());
			stbi_write_png(filePath.c_str(), ctx.width, ctx.height, ctx.numComponents, ctx.out_data, ctx.width * ctx.numComponents);

			if (!paginate)
				break;
		}

		delete[] img_data;


		//stbi_write_png("C:/Users/riley/source/repos/CUDA_TTF_SDF/x64/Debug/test.png", ctx.width, ctx.height, ctx.numComponents, img_data, ctx.width * ctx.numComponents);

		

		FT_Done_Face(face);
		FT_Done_FreeType(library);
	}
	else if (img_src)
	{

		unsigned char* img_data = stbi_load(img_src, &ctx.width, &ctx.height, nullptr, 1);
		if (img_data == nullptr)
		{
			printf("Failed to load image!\n");
			return EXIT_FAILURE;
		}

		ctx.CopyImage(img_data);

		stbi_image_free(img_data);

		CUDA_SDF::GenerateSDF(ctx);

		stbi_write_png("C:/Users/riley/source/repos/CUDA_TTF_SDF/x64/Debug/out.png", ctx.width, ctx.height, ctx.numComponents, ctx.out_data, ctx.width * ctx.numComponents);
	}

	//CUDA_SDF::GenerateSDF(ctx);

	//save output image
	//stbi_write_png("C:/Users/riley/source/repos/CUDA_TTF_SDF/x64/Debug/out.png", ctx.width, ctx.height, ctx.numComponents, ctx.out_data, ctx.width * ctx.numComponents);

	return EXIT_SUCCESS;
}

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

FT_Long GenerateGlyphAtlas(FT_Face face, int char_offset, int target_width, int target_height, int font_height, unsigned char* data)
{
	//const int font_height = 64;

	FT_Set_Pixel_Sizes(face, 0, font_height);

	const int margin = 8;
	const int half_margin = margin / 2;

	int current_height = half_margin;
	int row_width = half_margin;
	int max_row_height = 0;

	//add options for pagination
	printf("Generating glyph atlas for %d glyphs.\n", face->num_glyphs);
	for (FT_Long i = char_offset; i < face->num_glyphs; i++)
	{
		if (FT_Get_Char_Index(face, i) == 0)
			continue;

		// load character glyph 
		if (FT_Load_Char(face, i, FT_LOAD_RENDER))
		{
			printf("Failed to load glyph %c for font\n", i);
			continue;
		}

		int width = face->glyph->bitmap.width;
		int height = face->glyph->bitmap.rows;

		if (row_width + width + half_margin > target_width)
		{
			row_width = half_margin;
			current_height += max_row_height + half_margin;
			max_row_height = 0;
			//maximum row height overall should be the font height
			if (current_height + font_height + half_margin > target_height)
			{
				printf("Texture size exceeded.\n");
				//return the next glyph required
				return i;
			}
		}

		int offset_x = row_width;
		int offset_y = current_height;

		for (int row = 0; row < height; row++)
		{
			for (int col = 0; col < width; col++)
			{
				int x = offset_x + col;
				int y = offset_y + row;

				data[y * target_width + x] = face->glyph->bitmap.buffer[row * width + col];
			}
		}

		max_row_height = max(max_row_height, height);
		row_width += width + half_margin;

		//glBindTexture(GL_TEXTURE_2D, newChar->TexID);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width, face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
		//glObjectLabel(GL_TEXTURE, newChar->TexID, 13, "char texture");
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		//glBindTexture(GL_TEXTURE_2D, 0);


		//newChar->size = glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows);
		//newChar->bearing = glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
		//newChar->advance = face->glyph->advance.x;
	}

	return -1;
}
