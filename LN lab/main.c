#include "return_codes.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined ZLIB
#	include <zlib.h>
#elif defined LIBDEFLATE
#	error("libdeflate is not currently supported")
#	include <libdeflate.h>
#elif defined ISAL
#	error("isa-l is not currently supported")
#	include <include/igzip_lib.h>
#else
#	error("A deflate decoding library must be selected")
#endif

typedef struct png_t_tag
{
	uint32_t m_width;
	uint32_t m_height;
	uint8_t m_bit_depth;
	uint8_t m_color_type;
	uint8_t m_compression_method;
	uint8_t m_filter_method;
	uint8_t m_interlace_method;

	uint8_t* m_image_data;
} png_t;

static size_t read_from_file(void* buffer, size_t size, FILE* file)
{
	size_t bytes_read = fread(buffer, 1, size, file);
	if (bytes_read != size)
	{
		fprintf(stderr, "fread failed");
		return ERROR_INVALID_DATA;
	}
	return ERROR_SUCCESS;
}

static size_t get_file_length(int64_t* length, FILE* file)
{
	if (fseek(file, 0, SEEK_END))
	{
		fprintf(stderr, "fseek failed");
		return ERROR_UNKNOWN;
	}
	*length = ftell(file);
	if (*length == -1)
	{
		fprintf(stderr, "ftell failed");
		return ERROR_UNKNOWN;
	}
	if (fseek(file, 0, SEEK_SET))
	{
		fprintf(stderr, "fseek failed");
		return ERROR_UNKNOWN;
	}
	return ERROR_SUCCESS;
}

static int32_t reverse_byte_order_32(int32_t x)
{
	return ((x & 0x000000FF) << 0x18) | ((x & 0x0000FF00) << 0x08) | ((x & 0x00FF0000) >> 0x08) | ((x & 0xFF000000) >> 0x18);
}

typedef enum chunk_type_t_tag
{
#define BYTES_TO_INT(b0, b1, b2, b3) (b0 << 0x00) | (b1 << 0x08) | (b2 << 0x10) | (b3 << 0x18)

	IHDR = BYTES_TO_INT('I', 'H', 'D', 'R'),
	PLTE = BYTES_TO_INT('P', 'L', 'T', 'E'),
	IDAT = BYTES_TO_INT('I', 'D', 'A', 'T'),
	IEND = BYTES_TO_INT('I', 'E', 'N', 'D'),
#undef BYTES_TO_INT
} chunk_type_t;

static size_t is_valid_chunk_type(uint32_t chunk_type)
{
	for (int64_t i = 0; i < 4; i++)
	{
		uint32_t c = (chunk_type >> (0x08 * i)) & 0xFF;
		if (!(((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'))))
			return 0;
	}
	return 1;
}

static size_t is_critical_chunk(uint32_t chunk_type)
{
	return !(chunk_type & (1 << (8 * 3 + 5)));
}

typedef struct png_reader_t_tag
{
	uint8_t* m_data;
	int64_t m_length;
	int64_t m_cursor;

	uint8_t* m_compressed_data;
	int64_t m_compressed_data_length;
	uint8_t* m_filtered_data;
	int64_t m_filtered_data_length;
} png_reader_t;
static size_t read_it(uint8_t* buffer, int64_t length, png_reader_t* reader)
{
	if (reader->m_cursor + length > reader->m_length)
	{
		fprintf(stderr, "Input file ended");
		return ERROR_INVALID_DATA;
	}
	for (int64_t i = 0; i < length; i++)
	{
		buffer[i] = reader->m_data[reader->m_cursor++];
	}
	return ERROR_SUCCESS;
}

typedef struct png_chunk_t_tag
{
	uint32_t m_length;
	uint32_t m_type;
	uint8_t* m_data;
	uint32_t m_crc;
} png_chunk_t;

static size_t read_chunk(png_chunk_t* chunk, png_reader_t* reader)
{
	size_t code;

	code = read_it(chunk, 8, reader);
	if (code)
		return code;
	chunk->m_length = reverse_byte_order_32(chunk->m_length);
	if (reader->m_cursor + chunk->m_length > reader->m_length)
	{
		fprintf(stderr, "Input file ended");
		return ERROR_INVALID_DATA;
	}
	chunk->m_data = reader->m_data + reader->m_cursor;
	reader->m_cursor += chunk->m_length;
	code = read_it(&chunk->m_crc, 4, reader);
	if (code)
		return code;
	chunk->m_crc = reverse_byte_order_32(chunk->m_crc);

	switch (chunk->m_type)
	{
	case IHDR:
	case PLTE:
	case IDAT:
	case IEND:
		break;
	default:
		if (!is_valid_chunk_type(chunk->m_type))
		{
			fprintf(stderr, "Invalid chunk type");
			return ERROR_INVALID_DATA;
		}
		if (is_critical_chunk(chunk->m_type))
		{
			fprintf(stderr, "Invalid critical chunk");
			return ERROR_INVALID_DATA;
		}
		break;
	}
	return ERROR_SUCCESS;
}

static size_t inflate_png_datastream(png_reader_t* reader)
{
	size_t code;
	code = ERROR_SUCCESS;

	z_stream stream = { 0 };
	stream.next_in = reader->m_compressed_data;
	stream.avail_in = reader->m_compressed_data_length;	   // number of bytes available at next_in
	stream.next_out = reader->m_filtered_data;
	stream.avail_out = reader->m_filtered_data_length;	  // remaining free space at next_out
	if (inflateInit(&stream))
	{
		fprintf(stderr, "inflateEnd failed");
		code = ERROR_UNKNOWN;
	}
	else
	{
		if (inflate(&stream, 0) != Z_STREAM_END)
		{
			fprintf(stderr, "inflate failed");
			code = ERROR_UNKNOWN;
		}
	}
	if (inflateEnd(&stream))	// All dynamically allocated data structures for this stream are freed
	{
		fprintf(stderr, "inflateEnd failed");
		code = ERROR_UNKNOWN;
	}
	return code;
}

typedef enum filter_type_t_tag
{
	None,
	Sub,
	Up,
	Average,
	Paeth,
} filter_type_t;

static size_t on_iend(png_t* png, png_reader_t* reader)
{
	size_t code;

	int64_t bytes_per_pixel = (png->m_color_type & 0b010) + 1ll;
	int64_t byte_width = png->m_width * bytes_per_pixel;	// bytes per line

	// get filtered data from compressed data
	int64_t filtered_data_width = byte_width + 1;
	reader->m_filtered_data_length = filtered_data_width * png->m_height;
	reader->m_filtered_data = malloc(reader->m_filtered_data_length);
	if (!reader->m_filtered_data)
	{
		fprintf(stderr, "cannot allocate memory");
		code = ERROR_MEMORY;
	}
	else
	{
		code = inflate_png_datastream(reader);
	}
	if (code)
		return code;

	if (reader->m_compressed_data)
	{
		free(reader->m_compressed_data);
		reader->m_compressed_data = 0;
		reader->m_compressed_data_length = 0;
	}

	// get image data from filtered data
	png->m_image_data = malloc(png->m_width * png->m_height * bytes_per_pixel);
	if (!png->m_image_data)
	{
		fprintf(stderr, "cannot allocate memory");
		return ERROR_MEMORY;
	}
	int64_t output_length = 0;
	for (int64_t y = 0; y < png->m_height; y++)
	{
		uint8_t filter = reader->m_filtered_data[y * filtered_data_width + 0];
		for (int64_t x = 0; x < byte_width; x++)
		{
			uint8_t left = 0;
			uint8_t above = 0;
			uint8_t upper_left = 0;
			if (y > 0)
			{
				above = png->m_image_data[(y - 1) * byte_width + x];
				if (x >= bytes_per_pixel)
				{
					upper_left = png->m_image_data[(y - 1) * byte_width + (x - bytes_per_pixel)];
				}
			}
			if (x >= bytes_per_pixel)
			{
				left = png->m_image_data[y * byte_width + (x - bytes_per_pixel)];	 // Sub(x) = Raw(x) - Raw(x-bpp)
			}
			uint8_t delta;
			switch (filter)
			{
			case None:
				delta = 0;
				break;
			case Sub:
				delta = left;
				break;
			case Up:
				delta = above;
				break;
			case Average:
				delta = (left + (int64_t)above) / 2;
				break;
				int64_t distance;
			case Paeth:
				distance = (int64_t)above + (int64_t)left - (int64_t)upper_left;	// Alan Paeth method
				int64_t dist_l = llabs(distance - left);
				int64_t dist_a = llabs(distance - above);
				int64_t dist_ul = llabs(distance - upper_left);
				if ((dist_l <= dist_a) && (dist_l <= dist_ul))
					delta = left;
				else if (dist_a <= dist_ul)
					delta = above;
				else
					delta = upper_left;
				break;
			default:
				fprintf(stderr, "Invalid filter type");
				return ERROR_INVALID_DATA;
			}
			uint8_t value = (uint8_t)(delta + reader->m_filtered_data[y * filtered_data_width + (x + 1)]);
			png->m_image_data[output_length++] = value;
		}
	}
	return code;
}

static size_t parse_png_data(png_t* png, png_reader_t* reader)
{
	size_t code;
	int64_t magic;
	int64_t signature = 0x0A1A0A0D474E5089ll;
	code = read_it(&magic, 8, reader);
	if (code)
		return code;
	if (magic != signature)
	{
		fprintf(stderr, "not a PNG signature");
		return ERROR_INVALID_DATA;
	}

	png_chunk_t header_chunk;
	code = read_chunk(&header_chunk, reader);
	if (code)
		return code;

	if (header_chunk.m_type != IHDR)
	{
		fprintf(stderr, "The first chunk must be IHDR");
		return ERROR_INVALID_DATA;
	}
	if (header_chunk.m_length != 13)
	{
		fprintf(stderr, "invalid length of IHDR");
		return ERROR_INVALID_DATA;
	}

	for (int64_t i = 0; i < 13; i++)
	{
		((uint8_t*)png)[i] = header_chunk.m_data[i];
	}
	png->m_width = reverse_byte_order_32(png->m_width);
	png->m_height = reverse_byte_order_32(png->m_height);

	if (!png->m_width)
	{
		fprintf(stderr, "Image dimensions must be non zero");
		return ERROR_INVALID_DATA;
	}
	if (!png->m_height)
	{
		fprintf(stderr, "Image dimensions must be non zero");
		return ERROR_INVALID_DATA;
	}
	if (png->m_width & (1u << 31u))
	{
		fprintf(stderr, "Image dimensions must be less than (2^31)-1!");
		return ERROR_INVALID_DATA;
	}
	if (png->m_height & (1u << 31u))
	{
		fprintf(stderr, "Image dimensions must be less than (2^31)-1!");
		return ERROR_INVALID_DATA;
	}
	if ((png->m_bit_depth != 1) && (png->m_bit_depth != 2) && (png->m_bit_depth != 4) && (png->m_bit_depth != 8) &&
		(png->m_bit_depth != 16))
	{
		fprintf(stderr, "Invalid bit depth");
		return ERROR_INVALID_DATA;
	}
	if ((png->m_color_type != 0) && (png->m_color_type != 2) && (png->m_color_type != 3) && (png->m_color_type != 4) &&
		(png->m_color_type != 6))
	{
		fprintf(stderr, "Invalid color type");
		return ERROR_INVALID_DATA;
	}
	if (png->m_compression_method)
	{
		fprintf(stderr, "only compression method 0 is defined");
		return ERROR_INVALID_DATA;
	}
	if (png->m_filter_method)
	{
		fprintf(stderr, "only filter method 0 is defined");
		return ERROR_INVALID_DATA;
	}
	if ((png->m_interlace_method != 0) && (png->m_interlace_method != 1))
	{
		fprintf(stderr, "only 0 and 1 values are defined");
		return ERROR_INVALID_DATA;
	}
	if ((png->m_color_type != 0) && (png->m_color_type != 2))
	{
		fprintf(stderr, "Unsupported color type");
		return ERROR_INVALID_DATA;
	}
	if (png->m_bit_depth != 8)
	{
		fprintf(stderr, "Unsupported bit depth");
		return ERROR_INVALID_DATA;
	}
	if (png->m_interlace_method)
	{
		fprintf(stderr, "Unsupported interlace method");
		return ERROR_INVALID_DATA;
	}
	// decode
	reader->m_compressed_data = 0;
	reader->m_compressed_data_length = 0;
	reader->m_filtered_data = 0;
	reader->m_filtered_data_length = 0;
	// parse remaining chunks
	size_t running = 1;
	while (running)
	{
		png_chunk_t chunk;
		code = read_chunk(&chunk, reader);
		if (code)
			break;

		switch (chunk.m_type)
		{
		case IHDR:
			fprintf(stderr, "must be 1 IHDR chunk!");
			code = ERROR_INVALID_DATA;
			running = 0;
			break;
		case PLTE:
			fprintf(stderr, "unsupported PLTE chunk");
			code = ERROR_INVALID_DATA;
			running = 0;
			break;
			uint8_t* temp;
		case IDAT:
			temp = realloc(reader->m_compressed_data, reader->m_compressed_data_length + chunk.m_length);
			if (!temp)
			{
				fprintf(stderr, "Can't allocate more memory");
				code = ERROR_MEMORY;
				running = 0;
			}
			else
			{
				reader->m_compressed_data = temp;
				memcpy(reader->m_compressed_data + reader->m_compressed_data_length, chunk.m_data, chunk.m_length);
				reader->m_compressed_data_length += chunk.m_length;
			}
			break;
		case IEND:
			running = 0;
			if (chunk.m_length)
			{
				fprintf(stderr, "IEND chunk must have 0 data length");
				code = ERROR_INVALID_DATA;
			}
			else
			{
				code = on_iend(png, reader);
			}
			break;
		}
	}

	if (reader->m_compressed_data)
	{
		free(reader->m_compressed_data);
		reader->m_compressed_data = 0;
		reader->m_compressed_data_length = 0;
	}
	if (reader->m_filtered_data)
	{
		free(reader->m_filtered_data);
		reader->m_filtered_data = 0;
		reader->m_filtered_data_length = 0;
	}

	return code;
}

size_t png_from_file_handle(png_t* png, FILE* file)
{
	size_t code;

	png_reader_t reader;
	reader.m_cursor = 0;
	code = get_file_length(&reader.m_length, file);
	if (!code)
	{
		reader.m_data = malloc(reader.m_length);
		if (!reader.m_data)
		{
			fprintf(stderr, "Can't allocate memory");
			code = ERROR_MEMORY;
		}
		else
		{
			code = read_from_file(reader.m_data, reader.m_length, file);
			if (!code)
			{
				code = parse_png_data(png, &reader);
			}

			free(reader.m_data);
		}
	}

	return code;
}

static size_t write_decimal(FILE* file, int32_t value)
{
	if (fprintf(file, "%d ", value) < 0)
	{
		fprintf(stderr, "fprintf failed");
		return ERROR_UNKNOWN;
	}
	return ERROR_SUCCESS;
}

static size_t write_to_file(void* buffer, size_t size, FILE* file)
{
	size_t bytes_written = fwrite(buffer, 1, size, file);
	if (bytes_written != size)
	{
		fprintf(stderr, "fwrite failed");
		return ERROR_UNKNOWN;
	}
	return ERROR_SUCCESS;
}

size_t save_png_as_pnm_by_file_handle(png_t* png, FILE* file)
{
	size_t code;

	switch (png->m_color_type)
	{
	case 0:
		code = write_to_file("P5 ", 3, file);
		if (code)
			return code;
		break;
	case 2:
		code = write_to_file("P6 ", 3, file);
		if (code)
			return code;
		break;
	default:
		fprintf(stderr, "Invalid PNG color type");
		return ERROR_INVALID_PARAMETER;
	}

	code = write_decimal(file, png->m_width);
	if (code)
		return code;
	code = write_decimal(file, png->m_height);
	if (code)
		return code;
	code = write_decimal(file, 255);
	if (code)
		return code;
	size_t bytes_per_pixel = (png->m_color_type & 0b010) + 1ll;
	code = write_to_file(png->m_image_data, png->m_width * bytes_per_pixel * png->m_height, file);
	return code;
}

int main(int argc, char* argv[])
{
	size_t code;

	if (argc != 3)
	{
		fprintf(stderr, "Wrong number of arguments");
		code = ERROR_INVALID_PARAMETER;
	}
	else
	{
		FILE* input_file = fopen(argv[1], "rb");
		if (!input_file)
		{
			fprintf(stderr, "Can't open an input file");
			code = ERROR_NOT_FOUND;
		}
		else
		{
			FILE* output_file = fopen(argv[2], "wb");
			if (!output_file)
			{
				fprintf(stderr, "Can't open an output file");
				code = ERROR_NOT_FOUND;
			}
			else
			{
				png_t png;
				code = png_from_file_handle(&png, input_file);
				if (!code)
				{
					code = save_png_as_pnm_by_file_handle(&png, output_file);
				}

				if (png.m_image_data)
				{
					free(png.m_image_data);
				}
				fclose(output_file);
			}
			fclose(input_file);
		}
	}
	return code;
}
