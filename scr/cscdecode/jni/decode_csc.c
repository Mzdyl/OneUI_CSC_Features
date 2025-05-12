#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define MAX_BUFFER_SIZE 4096
#define INITIAL_COMPRESSION_BUFFER_SIZE (1024 * 128) // Initial buffer for compressed data

// 定义盐值数组（salts）和位移数组（shifts）
int salts[] = {65, -59, 33, -34, 107, 28, -107, 55, 78, 17, -81, 6, -80, -121, -35, -23, 72, 122, -63, -43, 68, 119, -78, -111, -60, 31, 60, 57, 92, -88, -100, -69, -106, 91, 69, 93, 110, 23, 93, 53, -44, -51, 64, -80, 46, 2, -4, 12, -45, 80, -44, -35, -111, -28, -66, -116, 39, 2, -27, -45, -52, 125, 39, 66, -90, 63, -105, -67, 84, -57, -4, -4, 101, -90, 81, 10, -33, 1, 67, -57, -71, 18, -74, 102, 96, -89, 64, -17, 54, -94, -84, -66, 14, 119, 121, 2, -78, -79, 89, 63, 93, 109, -78, -51, 66, -36, 32, 86, 3, -58, -15, 92, 58, 2, -89, -80, -13, -1, 122, -4, 48, 63, -44, 59, 100, -42, -45, 59, -7, -17, -54, 34, -54, 71, -64, -26, -87, -80, -17, -44, -38, -112, 70, 10, -106, 95, -24, -4, -118, 45, -85, -13, 85, 25, -102, -119, 13, -37, 116, 46, -69, 59, 42, -90, -38, -105, 101, -119, -36, 97, -3, -62, -91, -97, -125, 17, 14, 106, -72, -119, 99, 111, 20, 18, -27, 113, 64, -24, 74, -60, -100, 26, 56, -44, -70, 12, -51, -100, -32, -11, 26, 48, -117, 98, -93, 51, -25, -79, -31, 97, 87, -105, -64, 7, -13, -101, 33, -122, 5, -104, 89, -44, -117, 63, -80, -6, -71, -110, -29, -105, 116, 107, -93, 91, -41, -13, 20, -115, -78, 43, 79, -122, 6, 102, -32, 52, -118, -51, 72, -104, 41, -38, 124, 72, -126, -35};
int shifts[] = {1, 1, 0, 2, 2, 4, 5, 0, 4, 7, 1, 6, 5, 3, 3, 1, 2, 5, 0, 6, 2, 2, 4, 2, 2, 3, 0, 2, 1, 2, 4, 3, 4, 0, 0, 0, 3, 5, 3, 1, 6, 5, 6, 1, 1, 1, 0, 0, 3, 2, 7, 7, 5, 6, 7, 3, 5, 1, 0, 7, 6, 3, 6, 5, 4, 5, 3, 5, 1, 3, 3, 1, 5, 4, 1, 0, 0, 2, 6, 6, 6, 6, 4, 0, 1, 1, 0, 5, 5, 4, 2, 4, 6, 1, 7, 1, 2, 1, 1, 6, 5, 4, 7, 6, 5, 1, 6, 7, 0, 2, 6, 3, 1, 7, 1, 1, 7, 4, 0, 4, 2, 5, 3, 1, 1, 5, 6, 0, 3, 5, 3, 6, 5, 7, 2, 5, 6, 6, 2, 2, 3, 6, 0, 4, 3, 2, 0, 2, 2, 3, 5, 3, 3, 2, 5, 5, 5, 1, 3, 1, 1, 1, 4, 5, 1, 6, 2, 4, 7, 1, 4, 6, 0, 6, 4, 3, 2, 6, 1, 6, 3, 2, 1, 6, 7, 3, 2, 1, 1, 5, 6, 7, 2, 2, 2, 7, 4, 6, 7, 5, 3, 1, 4, 2, 7, 1, 6, 2, 4, 1, 5, 6, 5, 4, 5, 0, 1, 1, 6, 3, 7, 2, 0, 2, 5, 0, 1, 3, 3, 2, 6, 7, 7, 2, 5, 6, 0, 4, 1, 2, 5, 3, 7, 6, 5, 2, 5, 2, 0, 1, 3, 1, 4, 3, 4, 2};

size_t num_salts = sizeof(salts) / sizeof(salts[0]);
size_t num_shifts = sizeof(shifts) / sizeof(shifts[0]);

// 将文件内容读取为字节数组
unsigned char* file_to_byte_array(const char* file, size_t* size) {
    FILE* f = fopen(file, "rb");
    if (!f) {
        perror("无法打开文件");
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    unsigned char* buffer = (unsigned char*)malloc(*size);
    if (!buffer) {
        perror("内存分配失败");
        fclose(f);
        return NULL;
    }
    
    if (fread(buffer, 1, *size, f) != *size) {
        perror("读取文件失败");
        fclose(f);
        free(buffer);
        return NULL;
    }

    fclose(f);
    return buffer;
}

// 将字节数组写入文件
void byte_array_to_file(const unsigned char* byte_array, size_t size, const char* output_file) {
    FILE* f = fopen(output_file, "wb");
    if (!f) {
        perror("无法打开输出文件");
        return;
    }
    
    if (fwrite(byte_array, 1, size, f) != size) {
        perror("写入文件失败");
    }

    fclose(f);
}

// 内存中解压 GZIP 数据
int decompress_gzip_in_memory(const unsigned char* compressed_data, size_t compressed_size, 
                              const char* output_file) {
    z_stream strm;
    unsigned char out_buffer[MAX_BUFFER_SIZE];
    int ret;
    FILE* outfile;
    
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = compressed_size;
    strm.next_in = (Bytef*)compressed_data; // Cast to Bytef*
    
    ret = inflateInit2(&strm, 31); // 15 (window bits) + 16 (gzip wrapper)
    if (ret != Z_OK) {
        fprintf(stderr, "初始化解压失败: %d (%s)\n", ret, strm.msg ? strm.msg : "no message");
        return -1;
    }
    
    outfile = fopen(output_file, "wb");
    if (!outfile) {
        perror("无法打开输出文件进行解压");
        inflateEnd(&strm);
        return -1;
    }
    
    do {
        strm.avail_out = MAX_BUFFER_SIZE;
        strm.next_out = out_buffer;
        ret = inflate(&strm, Z_NO_FLUSH);
        
        switch (ret) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                fprintf(stderr, "解压数据错误: %d (%s)\n", ret, strm.msg ? strm.msg : "no message");
                inflateEnd(&strm);
                fclose(outfile);
                return -1;
        }
        
        unsigned int have = MAX_BUFFER_SIZE - strm.avail_out;
        if (fwrite(out_buffer, 1, have, outfile) != have || ferror(outfile)) {
            perror("写入解压数据失败");
            inflateEnd(&strm);
            fclose(outfile);
            return -1;
        }
    } while (strm.avail_out == 0 && ret != Z_STREAM_END); // Loop until stream end or no more output progress
    
    inflateEnd(&strm);
    fclose(outfile);
    
    return (ret == Z_STREAM_END) ? 0 : -1;
}


// 内存中压缩 GZIP 数据
int compress_gzip_in_memory(const unsigned char* uncompressed_data, size_t uncompressed_size,
                            unsigned char** compressed_data_out, size_t* compressed_size_out) {
    z_stream strm;
    int ret;
    unsigned char* out_dynamic_buffer = NULL;
    size_t out_buffer_capacity = 0;
    size_t total_out = 0;
    unsigned char chunk_buffer[MAX_BUFFER_SIZE]; // Temporary chunk for zlib output

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        fprintf(stderr, "初始化压缩失败: %d (%s)\n", ret, strm.msg ? strm.msg : "no message");
        return -1;
    }

    strm.avail_in = uncompressed_size;
    strm.next_in = (Bytef*)uncompressed_data;

    out_buffer_capacity = INITIAL_COMPRESSION_BUFFER_SIZE;
    if (uncompressed_size > 0) { // Avoid malloc(0) issues if uncompressed_size is 0
         out_dynamic_buffer = (unsigned char*)malloc(out_buffer_capacity);
         if (!out_dynamic_buffer) {
            perror("压缩输出缓冲区内存分配失败");
            deflateEnd(&strm);
            return -1;
        }
    } else { // Handle zero-byte input
        out_dynamic_buffer = NULL; // No buffer needed for zero output
        out_buffer_capacity = 0;
    }


    do {
        strm.avail_out = MAX_BUFFER_SIZE; 
        strm.next_out = chunk_buffer;
        
        ret = deflate(&strm, (strm.avail_in > 0 || uncompressed_size == 0) ? Z_NO_FLUSH : Z_FINISH); 
        // If uncompressed_size is 0, we need to call deflate once with Z_FINISH
        // to get the empty gzip stream.

        if (ret == Z_STREAM_ERROR) {
            fprintf(stderr, "压缩数据错误: %d (%s)\n", ret, strm.msg ? strm.msg : "no message");
            if(out_dynamic_buffer) free(out_dynamic_buffer);
            deflateEnd(&strm);
            return -1;
        }
        
        unsigned int have = MAX_BUFFER_SIZE - strm.avail_out;
        if (have > 0) {
            if (total_out + have > out_buffer_capacity) {
                out_buffer_capacity = (total_out + have) * 2; // Grow buffer
                 unsigned char* new_buffer = (unsigned char*)realloc(out_dynamic_buffer, out_buffer_capacity);
                if (!new_buffer) {
                    perror("压缩输出缓冲区内存重分配失败");
                    if(out_dynamic_buffer) free(out_dynamic_buffer);
                    deflateEnd(&strm);
                    return -1;
                }
                out_dynamic_buffer = new_buffer;
            }
            memcpy(out_dynamic_buffer + total_out, chunk_buffer, have);
            total_out += have;
        }
    } while (ret != Z_STREAM_END);

    deflateEnd(&strm);

    if (ret == Z_STREAM_END) {
        if (total_out > 0) {
            unsigned char* final_buffer = (unsigned char*)realloc(out_dynamic_buffer, total_out); // Trim to actual size
            if (!final_buffer && total_out > 0) { // Should not happen if shrinking, but defensive
                *compressed_data_out = out_dynamic_buffer; // Use original potentially larger buffer
            } else {
                 *compressed_data_out = final_buffer; // Use trimmed buffer
            }
        } else { // total_out is 0
            if(out_dynamic_buffer) free(out_dynamic_buffer); // Free if it was allocated for non-empty input that compressed to zero (unlikely for gzip)
            *compressed_data_out = NULL;
        }
        *compressed_size_out = total_out;
        return 0;
    } else {
        fprintf(stderr, "压缩未完成: %d (%s)\n", ret, strm.msg ? strm.msg : "no message");
        if(out_dynamic_buffer) free(out_dynamic_buffer);
        return -1;
    }
}


void decode(const char* input_file, const char* output_file) {
    size_t size;
    unsigned char* byte_array = file_to_byte_array(input_file, &size);
    if (!byte_array) {
        return;
    }

    unsigned char* result_decrypted = (unsigned char*)malloc(size);
    if (!result_decrypted) {
        perror("内存分配失败 (decode)");
        free(byte_array);
        return;
    }

    // 进行解密处理 (Crypto part of decode)
    // Java _decode:
    // 1. Circular left shift: results[i] = (byte) (((source[i] & 255) << S) | ((source[i] & 255) >>> (8 - S)));
    // 2. XOR: results[i] = (byte) (results[i] ^ SALT);
    for (size_t i = 0; i < size; ++i) {
        unsigned int current_encrypted_byte = byte_array[i]; // This is 0-255
        int shift_val = shifts[i % num_shifts]; // This is 0-7

        // 1. Circular left shift
        unsigned int after_shift = ((current_encrypted_byte << shift_val) | (current_encrypted_byte >> (8 - shift_val))) & 0xFF;

        // 2. XOR
        int salt_val = salts[i % num_salts];
        unsigned int decrypted_byte = (after_shift ^ salt_val) & 0xFF;
        
        result_decrypted[i] = (unsigned char)decrypted_byte;
    }

    printf("正在进行解密和解压处理...\n");
    int decompress_result = decompress_gzip_in_memory(result_decrypted, size, output_file);
    
    if (decompress_result == 0) {
        printf("文件成功解密和解压到: %s\n", output_file);
    } else {
        fprintf(stderr, "解压失败，请检查文件格式或解密是否正确\n");
    }

    free(byte_array);
    free(result_decrypted);
}


void encode(const char* input_file, const char* output_file) {
    size_t uncompressed_size;
    unsigned char* uncompressed_data = file_to_byte_array(input_file, &uncompressed_size);
    if (!uncompressed_data) {
        return;
    }

    unsigned char* compressed_data = NULL;
    size_t compressed_size = 0;

    printf("正在进行压缩处理...\n");
    int compress_status = compress_gzip_in_memory(uncompressed_data, uncompressed_size, 
                                                  &compressed_data, &compressed_size);
    free(uncompressed_data); 

    if (compress_status != 0) { // compressed_data might be NULL if uncompressed_size was 0
        fprintf(stderr, "文件压缩失败.\n");
        if(compressed_data) free(compressed_data); // Free if allocated
        return;
    }
    printf("压缩完成, 压缩后大小: %zu bytes\n", compressed_size);

    // compressed_data can be NULL if input was empty and compressed to empty.
    // malloc(0) is implementation-defined, better to handle size 0 separately.
    unsigned char* result_encrypted = NULL;
    if (compressed_size > 0) {
        result_encrypted = (unsigned char*)malloc(compressed_size);
        if (!result_encrypted) {
            perror("内存分配失败 (encode)");
            if (compressed_data) free(compressed_data);
            return;
        }
    } else {
        // If compressed_size is 0, no encryption needed, result_encrypted remains NULL
    }
    

    printf("正在进行加密处理...\n");
    // 进行加密处理 (Crypto part of encode)
    // Java _encode:
    // 1. XOR: results[i] = (byte) (source[i] ^ SALT);
    // 2. Circular right shift: results[i] = (byte) (((results[i] & 255) >> S) | ((results[i] & 255) << (8 - S)));
    for (size_t i = 0; i < compressed_size; ++i) {
        unsigned int current_plain_byte = compressed_data[i]; // This is 0-255 (GZIP data)
        
        // 1. XOR with salt
        int salt_val = salts[i % num_salts];
        unsigned int after_xor = (current_plain_byte ^ salt_val) & 0xFF;

        // 2. Circular RIGHT shift
        int shift_val = shifts[i % num_shifts];
        unsigned int encrypted_byte = ((after_xor >> shift_val) | (after_xor << (8 - shift_val))) & 0xFF;
        
        result_encrypted[i] = (unsigned char)encrypted_byte;
    }

    byte_array_to_file(result_encrypted, compressed_size, output_file);
    printf("文件成功压缩和加密到: %s\n", output_file);

    if (compressed_data) free(compressed_data);
    if (result_encrypted) free(result_encrypted);
}


int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "用法: %s <encode|decode> <输入文件路径> <输出文件路径>\n", argv[0]);
        fprintf(stderr, "输入文件路径是只读路径，输出文件路径是读写路径\n");
        return 1;
    }

    const char* operation = argv[1];
    const char* input_file = argv[2];
    const char* output_file = argv[3];

    if (strcmp(operation, "decode") == 0) {
        decode(input_file, output_file);
    } else if (strcmp(operation, "encode") == 0) {
        encode(input_file, output_file);
    } else {
        fprintf(stderr, "无效操作: %s. 请使用 'encode' 或 'decode'.\n", operation);
        return 1;
    }

    return 0;
}