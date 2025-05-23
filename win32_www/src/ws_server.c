#include "ws_server.h"
#include "sha1.h"
#include "base64.h"
#define WS_FIN      128

/**
 * @brief Frame FIN shift
 */
#define WS_FIN_SHIFT  7

typedef struct {
    size_t remaining;
    const uint8_t* input;
} ws_srv_array_cur_t;
static int ws_srv_array_read(void* state) {
    ws_srv_array_cur_t* cur = (ws_srv_array_cur_t*)state;
    if(cur->remaining<=0) {
        return -1;
    }
    uint8_t result = *cur->input;
    ++cur->input;
    --cur->remaining;
    return result;
}
int ws_srv_compute_sec(const char* ws_sec, size_t ws_sec_length, char* out_buffer, size_t out_buffer_length) {
    if(out_buffer_length < 28 || ws_sec==NULL||out_buffer==NULL) {
        return -1;
    }
    if(!ws_sec_length) {
        ws_sec_length = strlen(ws_sec);
    }
    SHA1_CTX sha;
    uint8_t results[20];
    SHA1Init(&sha);
    SHA1Update(&sha, (uint8_t*)ws_sec, ws_sec_length);
    SHA1Update(&sha, (uint8_t*)"258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36);
    SHA1Final(results, &sha);
    base64_context_t ctx;
    ws_srv_array_cur_t arr_cur;
    arr_cur.input = results;
    arr_cur.remaining=sizeof(results);
    base64_init(ws_srv_array_read, &arr_cur, &ctx);
    int result;
    size_t len = out_buffer_length;
    do {
        result = base64_encode(&ctx, out_buffer, &len);
        out_buffer+=len;
        len = out_buffer_length;
    } while (result > 0);
    if(out_buffer_length>28) {
        *out_buffer='\0';
    }
    return 0;
}
int ws_srv_send_frame(ws_srv_frame_t* frame,int(*socket_send)(void* data,size_t length, void*state), void* socket_send_state) {
    if(frame==NULL||socket_send==NULL) {
        return -1;
    }
    uint8_t idx_first_rData; 
    uint8_t data[10];
    data[0] = (frame->final!=0?WS_FIN:0)|(int)frame->type;
    /* Split the size between octets. */
	if (frame->len <= 125)
	{
		data[1] = frame->len & 0x7F;
		idx_first_rData = 2;
	}

	/* Size between 126 and 65535 bytes. */
	else if (frame->len >= 126 && frame->len <= 65535)
	{
		data[1] = 126;
		data[2] = (frame->len >> 8) & 255;
		data[3] = frame->len & 255;
		idx_first_rData = 4;
	}
	/* More than 65535 bytes. */
	else
	{
		data[1] = 127;
		data[2] = (unsigned char)((frame->len >> 56) & 255);
		data[3] = (unsigned char)((frame->len >> 48) & 255);
		data[4] = (unsigned char)((frame->len >> 40) & 255);
		data[5] = (unsigned char)((frame->len >> 32) & 255);
		data[6] = (unsigned char)((frame->len >> 24) & 255);
		data[7] = (unsigned char)((frame->len >> 16) & 255);
		data[8] = (unsigned char)((frame->len >> 8) & 255);
		data[9] = (unsigned char)(frame->len & 255);
		idx_first_rData = 10;
	}
    if(0!=socket_send(data,idx_first_rData,socket_send_state)) {
        return -1;
    }
    if(0!=socket_send(frame->payload,frame->len,socket_send_state)) {
        return -1;
    }
    return 0;
}