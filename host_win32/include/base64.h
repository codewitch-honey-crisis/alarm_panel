#ifndef HTCW_BASE64_H
#define HTCW_BASE64_H
#include <stddef.h>
#include <stdint.h>

/// @brief Reads a byte from the input stream. Returns the byte, or -1 if no more data
typedef int(*base64_on_read_callback)(void*);

/// @brief The base64 context
typedef struct {
    int state;
    base64_on_read_callback on_read;
    void* on_read_state;
    int i;
    uint32_t triplet;
    int8_t rem;
    union {
        struct {
            uint8_t decoded[4];
        };
        struct {
            int8_t out;
            uint8_t octets[3];
        };
    };
} base64_context_t;

#ifdef __cplusplus
extern "C" {
#endif
/// @brief Initializes the base64 context
/// @param on_read The method that reads from the input
/// @param on_read_state The user defined state value to pass to the read function
/// @param out_context The context to be filled
/// @return non-zero if success, 0 if error
int base64_init(base64_on_read_callback on_read,void* on_read_state, base64_context_t* out_context);
/// @brief Decodes a base64 stream
/// @param context The context initialized with base64_init
/// @param out_buffer The buffer that holds the partial result
/// @param in_out_size On input, the size of out_buffer, on output, the number of bytes written to the out_buffer
/// @return -1 if error, 0 if end, 1 if more data
int base64_decode(base64_context_t* context,uint8_t* out_buffer, size_t* in_out_size);
/// @brief Encodes a base64 stream
/// @param context The context initialized with base64_init
/// @param out_buffer The buffer that holds the partial result
/// @param in_out_size On input, the size of out_buffer, on output, the number of bytes written to the out_buffer
/// @return -1 if error, 0 if end, 1 if more data
int base64_encode(base64_context_t* context,char* out_buffer, size_t* in_out_size);
#ifdef __cplusplus
}
#endif

#if 0 // DEMO
#include <stdio.h>
#include "base64.h"

static int file_read(void* state) {
    if (state == NULL) {
        return -1;
    }
    return fgetc((FILE*)state);
}
int main() {
    base64_context_t ctx;
    // encode
    char buffer[1024];
    size_t len;
    int result;
    FILE* file_src = fopen("test.jpg", "rb");
    if (file_src == NULL) {
        puts("File not found");
        return 1;
    }
    FILE* file_dst = fopen("test.b64", "wb");
    base64_init(file_read, file_src, &ctx);
    len = sizeof(buffer);
    do {
        result = base64_encode(&ctx, (uint8_t*)buffer, &len);
        fwrite(buffer, 1, len, file_dst);
        len = sizeof(buffer);

    } while (result > 0);
    if (result < 0) {
        puts("error");
    } else {
        puts("done");
    }
    fclose(file_src);
    fclose(file_dst);

    // decode
    file_src = fopen("test.b64", "rb");
    if (file_src == NULL) {
        puts("File not found");
        return 1;
    }
    file_dst = fopen("test2.jpg", "wb");
    base64_init(file_read, file_src, &ctx);
    len = sizeof(buffer);
    do {
        result = base64_decode(&ctx, (uint8_t*)buffer, &len);
        fwrite(buffer, 1, len, file_dst);
        len = sizeof(buffer);
    } while (result > 0);
    puts("");
    if (result < 0) {
        puts("error");
    } else {
        puts("done");
    }
    fclose(file_src);
    fclose(file_dst);

    return 0;
}
#endif // DEMO

#endif // HTCW_BASE64_H