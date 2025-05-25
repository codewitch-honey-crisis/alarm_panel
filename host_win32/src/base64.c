#include "base64.h"

static char encoding_table[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

static const int decoding_table[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63, 52, 53, 54, 55, 56, 57,
    58, 59, 60, 61, 0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 0,  0,  0,  0,  63, 0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

int base64_init(base64_on_read_callback on_read, void* on_read_state,
                base64_context_t* out_context) {
    if (on_read == NULL || out_context == NULL) {
        return -1;  // invalid arg
    }
    out_context->rem = 0;
    out_context->out = 0;
    out_context->triplet = 0;
    out_context->state = -1;
    out_context->on_read = on_read;
    out_context->on_read_state = on_read_state;
    return 0;
}
int base64_encode(base64_context_t* context, char* out_buffer,
                  size_t* in_out_size) {
    if (context == NULL || out_buffer == NULL || in_out_size == NULL ||
        !*in_out_size) {
        return -1;  // invalid arg
    }
    size_t size = *in_out_size;
    *in_out_size = 0;
    while (1) {
        // we can compute some output
        switch (context->state) {
            case -3:
                return -1;
            case -2:
                return 0;
            case -1:
                context->i = context->on_read(context->on_read_state);
                if (context->i == -1) {  // can't be empty
                    context->state = -3;
                    return -1;
                }
                ++context->state;
                context->rem = 1;
                break;
            case 0:
                if (context->i == -1) {
                    context->state = 8;  // end state
                    break;
                }
                context->octets[0] = context->i;
                ++context->state;
                break;
            case 1:
            case 2:
                context->i = context->on_read(context->on_read_state);
                if (context->i != -1) {
                    context->octets[context->state] = context->i;
                    ++context->rem;
                    if (context->rem > 3) {
                        context->rem = 0;
                    }
                } else {
                    context->octets[context->state] = 0;
                }
                ++context->state;
                break;
            case 3:
                context->triplet = (context->octets[0] << 0x10) +
                                   (context->octets[1] << 0x08) +
                                   context->octets[2];
                ++context->state;
                context->out = 0;
                break;
            case 4:
            case 5:
            case 6:
            case 7:
                ++context->out;
                if (context->out > 4) {
                    context->out = 0;
                }
                if (context->state > context->rem + 4) {
                    context->state = 8;
                    break;
                }
                *(out_buffer++) = encoding_table[(context->triplet >>
                                                  (7 - context->state) * 6) &
                                                 0x3F];
                ++(*in_out_size);
                --size;
                ++context->state;
                if (!size) {
                    return 1;
                }
                break;
            case 8:
                if (context->i != -1) {
                    context->i = context->on_read(context->on_read_state);
                    if (context->i == -1) {
                        break;
                    }

                    context->octets[0] = context->i;
                    context->state = 1;
                    context->rem = 1;

                    break;
                }
                if (context->out <= 4) {
                    ++context->out;
                    *(out_buffer++) = '=';
                    ++(*in_out_size);
                    --size;
                    if (!size) {
                        return 1;
                    }
                    break;
                }
                context->state = -2;
                return 1;
        }
    }
}

int base64_decode(base64_context_t* context, uint8_t* out_buffer,
                  size_t* in_out_size) {
    if (context == NULL || out_buffer == NULL || in_out_size == NULL ||
        !*in_out_size) {
        return -1;  // invalid arg
    }
    size_t size = *in_out_size;
    *in_out_size = 0;
    while (1) {
        // we can compute some output
        switch (context->state) {
            case -3:
                return -1;
            case -2:
                return 0;
            case -1:
                ++context->state;
                break;
            case 0:
            case 1:
            case 2:
            case 3:
                context->i = context->on_read(context->on_read_state);
                if (context->i == -1) {
                    context->state = -3;
                    return 1;
                }
                switch (context->i) {
                    case 'A':
                    case '=':
                        context->decoded[context->state] = 0;
                        break;
                    default:
                        context->decoded[context->state] =
                            decoding_table[context->i];
                        if (!context->decoded[context->state]) {
                            context->state = -3;
                            return 1;
                        }
                        break;
                }
                ++context->state;
                break;
            case 4:
                context->triplet = (context->decoded[0] << 3 * 6) +
                                   (context->decoded[1] << 2 * 6) +
                                   (context->decoded[2] << 1 * 6) +
                                   (context->decoded[3] << 0 * 6);
                *(out_buffer++) = (context->triplet >> 2 * 8) & 0xFF;
                --size;
                ++*in_out_size;
                ++context->state;
                if (!size) {
                    return 1;
                }
                break;
            case 5:
                *(out_buffer++) = (context->triplet >> 1 * 8) & 0xFF;
                --size;
                ++*in_out_size;
                ++context->state;
                if (!size) {
                    return 1;
                }
                break;
            case 6:
                *(out_buffer++) = (context->triplet >> 0 * 8) & 0xFF;
                --size;
                ++*in_out_size;
                ++context->state;
                if (!size) {
                    return 1;
                }
                break;
            case 7:
                context->i = context->on_read(context->on_read_state);
                if (context->i == -1) {
                    context->state = -1;
                    return 0;
                }
                context->decoded[0] =
                    context->i == '=' ? 0 : decoding_table[context->i];
                context->state = 1;
                break;
            default:
                return -1;
        }
    }
}