#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <png.h>
#include<string.h>

#include"defs.h"
#include"alloc.h"

// Can we have more boilerplate please 🥺
// This is ChatGPT generated, I'm not reading that

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} MemoryReader;

void read_png_from_memory(png_structp png_ptr, png_bytep out_bytes, png_size_t byte_count) {
    MemoryReader *reader = (MemoryReader *)png_get_io_ptr(png_ptr);
    if (reader->offset + byte_count <= reader->size) {
        memcpy(out_bytes, reader->data + reader->offset, byte_count);
        reader->offset += byte_count;
    } else {
        png_error(png_ptr, "Read error in memory stream");
    }
}

Bitmap decode_png_rgba(const unsigned char *png_data, int png_size) {
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return {};

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return {};
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return {};
    }

    MemoryReader reader = { png_data, (size_t)png_size, 0 };
    png_set_read_fn(png_ptr, &reader, read_png_from_memory);

    png_read_info(png_ptr, info_ptr);

    auto width = png_get_image_width(png_ptr, info_ptr);
    auto height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY) {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }

    png_read_update_info(png_ptr, info_ptr);

    size_t image_size = width * height * 4;
    uint8_t *image_data = (uint8_t *)alloc(image_size, 2);

    size_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    auto ptmp = tmp;
    auto const readImg = [&](uint8_t *data) {
        png_bytep *row_pointers = talloc<png_bytep>(height);
        for (int y = 0; y < height; y++) {
            row_pointers[y] = data + y * row_bytes;
        }
        png_read_image(png_ptr, row_pointers);
    };

    if(row_bytes != width * 4) {
        // GOD I LOVE THIS LIBRARY, BEST API EVER!
        if(color_type == PNG_COLOR_TYPE_PALETTE && row_bytes == width * 3) {
            auto image2 = (uint8_t*)alloc(row_bytes * height);
            readImg(image2);
            auto imgRow = width * 4;
            auto img2Row = row_bytes;
            for(auto y = 0; y < height; y++) {
                for(auto x = 0; x < width; x++) {
                    auto const ib = y * imgRow + x * 4;
                    auto const i2b = y * img2Row + x * 3;
                    image_data[ib + 0] = image2[i2b + 0];
                    image_data[ib + 1] = image2[i2b + 1];
                    image_data[ib + 2] = image2[i2b + 2];
                    image_data[ib + 3] = 0xff;
                }
            }
        }
        else {
            printf("libpng doing nonsence again...\n");
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            return {};
        }
    }
    else {
        readImg(image_data);
    }

    png_read_end(png_ptr, NULL);

    tmp = ptmp;

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return { image_data, (int)width, (int)height };
}

