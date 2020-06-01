#ifndef MINI_JSON_H
#define MINI_JSON_H

#include <stdint.h>

#ifndef MINI_JSON_FLOAT_TYPE
#define MINI_JSON_FLOAT_TYPE float
#endif

struct mj_reader;

typedef void (*mj_callback_fn)(struct mj_reader *r, int event);

typedef struct mj_reader {
    char *strbuf;
    int strbuf_len;
    
    mj_callback_fn callback;

    union {
        char b;
#ifdef MINI_JSON_DECODE_INT_TYPE
        MINI_JSON_DECODE_INT_TYPE i;
#endif
#ifdef MINI_JSON_DECODE_FLOAT_TYPE
        MINI_JSON_DECODE_FLOAT_TYPE f;
#endif
    } value;

    // Buffer management    
    int start, sp, ep;

    // Parsing
    uint16_t depth;
    uint8_t parse_state;
    uint32_t child_index;

    // Tokenisation
    int tok_state;
} mj_reader_t;

typedef struct mj_writer {
    char *buffer;
    int buffer_len, sp, ep, depth;
} mj_writer_t;

typedef enum mj_status {
    MJ_OK           = 0,
    MJ_NOMEM        = -1,
    MJ_STATE        = -2,
    MJ_UNSUPPORTED  = -3,
    MJ_TOK_ERROR    = -4,
    MJ_PARSE_ERROR  = -5
} mj_status_t;

void mj_reader_init(mj_reader_t *r, char *string_buffer, int string_buffer_len);
void mj_reader_set_callback(mj_reader_t *r, mj_callback_fn cb);
int mj_reader_push(mj_reader_t *r, const char *data, int len);
void mj_reader_push_end(mj_reader_t *r);

void mj_writer_init(mj_writer_t *w, char *buffer, int len);
int mj_writer_start_array(mj_writer_t *w);
int mj_writer_start_object(mj_writer_t *w);
int mj_writer_end(mj_writer_t *w);
int mj_writer_put_key(mj_writer_t *w, const char *key);
int mj_writer_put_bool(mj_writer_t *w, int val);
int mj_writer_put_string(mj_writer_t *w, const char *str);
int mj_writer_put_base64(mj_writer_t *w, const char *data, int len);
int mj_writer_put_null(mj_writer_t *w);
int mj_writer_put_int(mj_writer_t *w, int val);
int mj_writer_put_long(mj_writer_t *w, long val);
int mj_writer_put_float(mj_writer_t *w, float val);
int mj_writer_put_double(mj_writer_t *w, double val);

#endif