#include "ws_server.h"
#ifdef ESP_PLATFORM
// ESP32 has built in websocket support
void ws_srv_frame_to_esp32(const ws_srv_frame_t* frame,httpd_ws_frame_t* out_esp32_frame) {
    out_esp32_frame->final = frame->final;
    out_esp32_frame->fragmented = frame->fragmented;
    out_esp32_frame->len = frame->len;
    out_esp32_frame->payload = frame->payload;
    out_esp32_frame->type = (httpd_ws_type_t)frame->type;
}
void ws_srv_esp32_to_frame(const httpd_ws_frame_t* esp32_frame, ws_srv_frame_t* out_frame) {
    out_frame->final = esp32_frame->final;
    out_frame->fragmented = esp32_frame->fragmented;
    out_frame->len = esp32_frame->len;
    out_frame->payload = esp32_frame->payload;
    out_frame->type = out_frame->type;
    out_frame->masked = 0;
}
#else
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

typedef struct {
    int(*socket_recv)(void* out_data,size_t* in_out_data_length, void*state);
    void* socket_recv_state;
} ws_srv_recv_context_t;
static size_t ws_srv_read(ws_srv_recv_context_t* ctx, uint8_t* data, size_t len) {
    size_t index = 0;
    long l = (long)len;
    while(l>0) {
        size_t size = l;
        if(0!=ctx->socket_recv(data+index,&size,ctx->socket_recv_state) || size==0) {
            return 0;
        }
        index+=size;
        l-=size;
    } 
    return len;
}
int ws_srv_recv_frame(int(*socket_recv)(void* out_data,size_t* in_out_data_length, void*state),void* socket_recv_state,ws_srv_frame_t* out_frame) {
    if(out_frame==NULL) {
        return -1;
    }
    out_frame->payload = NULL;
    uint8_t data[8];
    ws_srv_recv_context_t ctx;
    ctx.socket_recv = socket_recv;
    ctx.socket_recv_state = socket_recv_state;
    if(0==ws_srv_read(&ctx,data,2)) {
        return WS_SRV_RECV_ERROR;
    }
    out_frame->final = (data[0]>>7);
    out_frame->type = (ws_srv_msg_type_t)(data[0]&0x0F);
    out_frame->masked = !!(data[0]&0x80);
    out_frame->fragmented = !out_frame->final;
    uint8_t tmp = data[1]&0x7F;
    if(tmp<126) {
        out_frame->len = tmp;
        
    } else if(tmp==126) {
        if(0==ws_srv_read(&ctx,data,2)) {
            return WS_SRV_RECV_ERROR;
        }
        out_frame->len=(((uint16_t)data[0])<<8)|data[1];
    } else {
        if(0==ws_srv_read(&ctx,data,8)) {
            return WS_SRV_RECV_ERROR;
        }
        out_frame->len = 
                (((uint64_t)data[0])<<56)|
                (((uint64_t)data[1])<<48)|
                (((uint64_t)data[2])<<40)|
                (((uint64_t)data[3])<<32)|
                (((uint64_t)data[4])<<24)|
                (((uint64_t)data[5])<<16)|
                (((uint64_t)data[6])<<8)|
                (((uint64_t)data[7])<<0);
    }
    if(out_frame->masked) {
        if(0==ws_srv_read(&ctx,out_frame->mask_key,4)) {
            return WS_SRV_RECV_ERROR;
        }
    }
    
    return 0;
}
int ws_srv_send_frame(const ws_srv_frame_t* frame,int(*socket_send)(const void* data,size_t length, void*state), void* socket_send_state) {
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
#endif
void ws_srv_unmask_payload(const ws_srv_frame_t* frame, uint8_t* payload) {
    if(frame->masked) {
        for(size_t i = 0;i<frame->len;++i) {
            payload[i]=frame->payload[i]^frame->mask_key[i%4];
        }
    }
}
