#include <stdio.h>
#include <string.h>
#include <openjpeg-2.3/openjpeg.h>
#include <png.h>
#include <stdlib.h>

static void opj_close_from_file(void* p_user_data)
{
    FILE* p_file = (FILE*)p_user_data;
    fclose(p_file);
}

static OPJ_SIZE_T opj_read_from_file(void * p_buffer, OPJ_SIZE_T p_nb_bytes,
                                     void * p_user_data)
{
    FILE* p_file = (FILE*)p_user_data;
    OPJ_SIZE_T l_nb_read = fread(p_buffer, 1, p_nb_bytes, (FILE*)p_file);
    return l_nb_read ? l_nb_read : (OPJ_SIZE_T) - 1;
}

static OPJ_SIZE_T opj_write_from_file(void * p_buffer, OPJ_SIZE_T p_nb_bytes,
                                      void * p_user_data)
{
    FILE* p_file = (FILE*)p_user_data;
    return fwrite(p_buffer, 1, p_nb_bytes, p_file);
}

static OPJ_OFF_T opj_skip_from_file(OPJ_OFF_T p_nb_bytes, void * p_user_data)
{
    FILE* p_file = (FILE*)p_user_data;
    if (fseek(p_file, p_nb_bytes, SEEK_CUR)) {
        return -1;
    }

    return p_nb_bytes;
}

static OPJ_BOOL opj_seek_from_file(OPJ_OFF_T p_nb_bytes, void * p_user_data)
{
    FILE* p_file = (FILE*)p_user_data;
    if (fseek(p_file, p_nb_bytes, SEEK_SET)) {
        return OPJ_FALSE;
    }

    return OPJ_TRUE;
}

static OPJ_UINT64 opj_get_data_length_from_file(void * p_user_data)
{
    FILE* p_file = (FILE*)p_user_data;
    OPJ_OFF_T file_length = 0;

    fseek(p_file, 0, SEEK_END);
    file_length = (OPJ_OFF_T)ftell(p_file);
    fseek(p_file, 0, SEEK_SET);

    return (OPJ_UINT64)file_length;
}

int convert_j2k_to_png(const char *input_filename, const char *output_filename) {
    // Open the JPEG 2000 file
    FILE *infile = fopen(input_filename, "rb");
    if (!infile) {
        fprintf(stderr, "Error: Cannot open input file %s\n", input_filename);
        return 1;
    }

//     Create a stream
    opj_stream_t *stream = opj_stream_create_default_file_stream(input_filename, OPJ_STREAM_READ);
    if (!stream) {
        fclose(infile);
        fprintf(stderr, "Error: Cannot create stream\n");
        return 1;
    }

//    OPJ_SIZE_T p_size = OPJ_J2K_STREAM_CHUNK_SIZE;
//    opj_stream_t* stream = 00;
//
//    stream = opj_stream_create(p_size, 1);
//    if (!stream) {
//        fclose(infile);
//        return 1;
//    }
//
//    opj_stream_set_user_data(stream, infile, opj_close_from_file);
//    opj_stream_set_user_data_length(stream,
//                                    opj_get_data_length_from_file(infile));
//    opj_stream_set_read_function(stream, opj_read_from_file);
//    opj_stream_set_write_function(stream,
//                                  (opj_stream_write_fn) opj_write_from_file);
//    opj_stream_set_skip_function(stream, opj_skip_from_file);
//    opj_stream_set_seek_function(stream, opj_seek_from_file);


    // Create decompressor
    opj_codec_t *codec = opj_create_decompress(OPJ_CODEC_JP2);
    if (!codec) {
        opj_stream_destroy(stream);
        fprintf(stderr, "Error: Cannot create codec\n");
        return 1;
    }

    // Setup decompression parameters
    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    opj_image_t *image = NULL;

    if (!opj_setup_decoder(codec, &parameters)) {
        fprintf(stderr, "ERROR -> opj_decompress: failed to setup the decoder\n");
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        return 1;
    }

    /* Read the main header of the code stream and if necessary the JP2 boxes*/
    if (!opj_read_header(stream, codec, &image)) {
        fprintf(stderr, "ERROR -> opj_decompress: failed to read the header\n");
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        return 1;
    }

    // Decode the image
    if (!(opj_decode(codec, stream, image) &&
          opj_end_decompress(codec, stream))) {
        fprintf(stderr, "ERROR -> j2k_to_image: failed to decode image!\n");
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        return 1;
    }

    // Cleanup stream and file
    opj_stream_destroy(stream);
    opj_destroy_codec(codec);

    FILE *output_png = fopen(output_filename, "wb");
    if (!output_png) {
        fprintf(stderr, "Error opening output PNG file: %s\n", output_filename);
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        return 1;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "Error creating PNG write struct\n");
        fclose(output_png);
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        return 1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "Error creating PNG info struct\n");
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(output_png);
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        return 1;
    }

    png_init_io(png_ptr, output_png);

    png_set_IHDR(png_ptr, info_ptr, image->x1, image->y1, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    png_bytep row = (png_bytep)malloc(3 * image->x1 * sizeof(png_byte));
//    png_bytep row = (png_bytep)malloc(image->x1 * sizeof(png_byte));

//    size_t pixel_size = 6; // 12 bits = 1.5 bytes, rounded up to 2 bytes (16 bits)
//    png_bytep row = (png_bytep)malloc(image->x1 * sizeof(png_byte));

//    for (int y = 0; y < image->y1; y++) {
//        for (int x = 0; x < image->x1; x++) {
//            row[x * 3] = image->comps[0].data[y * image->x1 + x];
//            row[x * 3 + 1] = image->comps[1].data[y * image->x1 + x];
//            row[x * 3 + 2] = image->comps[2].data[y * image->x1 + x];
//        }
//        png_write_row(png_ptr, row);
//    }

// Iterate over each row of the image
//    for (int y = 0; y < image->y1; y++) {
//        // Iterate over each pixel in the row
//        for (int x = 0; x < image->x1; x++) {
//            // Get the pixel values for each component (assuming 3 components: RGB)
//            png_byte r = image->comps[0].data[y * image->comps[0].w + x]; // Red component
//            png_byte g; // Green component
//            if (image->comps[1].data) {
//                g = image->comps[1].data[y * image->comps[1].w + x]; // Green component
//            } else {
//                // Assign a default value if green component data is NULL
//                g = 0; // Default value for green
//            }
//            png_byte b; // Blue component
//
//            if (image->comps[2].data) {
//                if (image->comps[2].data[y * image->comps[2].w + x])
//                    b = image->comps[2].data[y * image->comps[2].w + x]; // Green component
//                else {
//                    b = 0;
//                }
//            } else {
//                b = 0; // Default value for green
//            }
//
//            // Store the RGB values in the row array
//            row[x * 3] = r;
//            row[x * 3 + 1] = g;
//            row[x * 3 + 2] = b;
//        }
//
//        // Write the row of pixel data to the PNG file
//        png_write_row(png_ptr, row);
//    }



//    for (int y = 0; y < image->y1; y++) {
//        // Iterate over each pixel in the row
//        for (int x = 0; x < image->x1; x++) {
//            // Get the pixel value (assuming 1 component, grayscale)
//            png_byte pixel_value = image->comps[0].data[y * image->comps[0].w + x];
//
//            // Store the pixel value in the row array (for grayscale, use the same value for R, G, and B)
//            row[x] = pixel_value;
//        }
//
//        // Write the row of pixel data to the PNG file
//        png_write_row(png_ptr, row);
//    }

    for (int y = 0; y < image->y1; y++) {
        for (int x = 0; x < image->x1; x++) {
            // Get the pixel values for each component (assuming ARGB format)
            png_byte r = image->comps[0].data[y * image->x1 + x]; // Red component
            png_byte g = image->comps[1].data[y * image->x1 + x]; // Green component
            png_byte b; // Blue component

            if (image->comps[2].data) {
                b = image->comps[2].data[y * image->x1 + x];
            } else {
                b = 0;
            }

            // Store the RGB values in the row array
            row[x * 3]     = r; // Red
            row[x * 3 + 1] = g; // Green
            row[x * 3 + 2] = b; // Blue
        }

        // Write the row of pixel data to the PNG file
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, info_ptr);

    // Cleanup
    free(row);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(output_png);
    opj_image_destroy(image);

    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s input_file output_file\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];

    if (convert_j2k_to_png(input_filename, output_filename)) {
        printf("Conversion failed.\n");
        return 1;
    }

    printf("Conversion successful.\n");
    return 0;
}

