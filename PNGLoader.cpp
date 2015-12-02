#include <iostream>
#include <fstream>
#include <stdlib.h>

#define PNG_DEBUG 3
#include <png.h>

#include "PNGLoader.h"

#define ERR(x) { error = x; return false; }

class PNGLoaderImpl : public PNGLoader {

	FILE *fp;
	png_structp png_ptr;

	int  rowbytes;
	png_byte color_type;
	png_byte bit_depth;

	png_infop info_ptr;
	int number_of_passes;
	png_bytep * row_pointers;
public:
	PNGLoaderImpl(){
		fp = NULL;
		png_ptr = NULL;
		info_ptr = NULL;
		row_pointers = NULL;
		data = NULL;
		error = NULL;
	}

	virtual ~PNGLoaderImpl(){
		if( data ) delete data;
		if( row_pointers ) 
			free(row_pointers);
		if( fp ) fclose(fp);
	}


	bool _load(const char* file)
	{
        	unsigned char header[8];

	        fp = fopen(file, "rb");
        	if (!fp) ERR( "Could not open file" );

	        fread(header, 1, 8, fp);

        	if( png_sig_cmp(header, 0, 8) ) ERR( "Not a PNG" );

	        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        	if (!png_ptr) ERR("Internal Error 1");

	        info_ptr = png_create_info_struct(png_ptr);
        	if (!info_ptr) ERR("Internal Error 2");

	        if (setjmp(png_jmpbuf(png_ptr))) ERR("Internal Error 3");

        	png_init_io(png_ptr, fp);
	        png_set_sig_bytes(png_ptr, 8);

        	png_read_info(png_ptr, info_ptr);

	        width = png_get_image_width(png_ptr, info_ptr);
        	height = png_get_image_height(png_ptr, info_ptr);
	        color_type = png_get_color_type(png_ptr, info_ptr);
        	bit_depth = png_get_bit_depth(png_ptr, info_ptr);

		if( bit_depth == 16 )
	        	png_set_strip_16(png_ptr);

		if( color_type == PNG_COLOR_TYPE_PALETTE )
			png_set_palette_to_rgb(png_ptr);

		if( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) )
			png_set_tRNS_to_alpha(png_ptr);

		if( color_type == PNG_COLOR_TYPE_PALETTE
			|| color_type == PNG_COLOR_TYPE_RGB 
			)
			png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER );

	        number_of_passes = png_set_interlace_handling(png_ptr);
        	png_read_update_info(png_ptr, info_ptr);


        	if (setjmp(png_jmpbuf(png_ptr))) ERR("Error reading image");

	        row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);

	        if (bit_depth == 16)
	                rowbytes = width*8;
        	else
                	rowbytes = width*4;
		
		data = (char *) malloc( rowbytes * height );
	        for( int y=0; y<height; y++ )
			row_pointers[y] = (png_byte*) data+y*rowbytes;
		
	        png_read_image(png_ptr, row_pointers);

	}
/*
	void process_file(void)
	{
	        for (y=0; y<height; y++) {
        	        png_byte* row = row_pointers[y];
	                for (x=0; x<width; x++) {
                        png_byte* ptr = &(row[x*4]);
                        printf("Pixel at position [ %d - %d ] has RGBA values: %d - %d - %d - %d\n",
                               x, y, ptr[0], ptr[1], ptr[2], ptr[3]);

                           ptr[0] = 0;
                           ptr[1] = ptr[2];
                	}
        	}
	}
*/
};

PNGLoader *PNGLoader::load(const char *file){
	PNGLoaderImpl *png = new PNGLoaderImpl();
	if( png->_load(file) )
		return png;
	std::cerr << png->error << std::endl;
	delete png;
	return NULL;
}



