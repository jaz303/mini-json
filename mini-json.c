#include "mini-json.h"
#include <string.h>
#include <stdlib.h>

#include <stdio.h>

//
// Reading

enum {
    OUT,
    KEYWORD,
    INT,
    FLOAT,
    STR,
    STR_ESC
};

enum {
    TOK_CONTINUE = 0,
    TOK_OBJECT_START, // 1
    TOK_OBJECT_END, // 2
    TOK_ARRAY_START, // 3
    TOK_ARRAY_END, // 4
    TOK_NULL, // 5
    TOK_TRUE, // 6
    TOK_FALSE, // 7
    TOK_INT, // 8
    TOK_FLOAT, // 9
    TOK_STRING, // 10
    TOK_COMMA, // 11
    TOK_COLON // 12
};

static void nop(void *userdata) {

}

static int is_whitespace(char ch) {
    return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
}

static int is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

inline static int do_OUT(mj_reader_t *r, char ch) {
    switch (ch) {
        case '{': return TOK_OBJECT_START;
        case '}': return TOK_OBJECT_END;
        case '[': return TOK_ARRAY_START;
        case ']': return TOK_ARRAY_END;
        case ',': return TOK_COMMA;
        case ':': return TOK_COLON;
        case '"':
            r->state = STR;
            break;
        case 't':
            r->value.i = 1;
            r->state = KEYWORD;
            r->kw = "rue";
            r->kw_tok = TOK_TRUE;
            r->kw_next = 0;
            break;
        case 'f':
            r->value.i = 0;
            r->state = KEYWORD;
            r->kw = "alse";
            r->kw_tok = TOK_FALSE;
            r->kw_next = 0;
            break;
        case 'n':
            r->state = KEYWORD;
            r->kw = "ull";
            r->kw_tok = TOK_NULL;
            r->kw_next = 0;
            break;
        default:
            if (is_whitespace(ch)) {
                // do nothing
            } else if (is_digit(ch)) {
                r->acc = ch - '0';
                r->state = INT;
            } else {
                // error
            }
            break;
    }
    return TOK_CONTINUE;
}

inline static int do_KEYWORD(mj_reader_t *r, char ch) {
    if (ch == r->kw[r->kw_next]) {
        r->kw_next++;
        if (r->kw[r->kw_next] == 0) {
            // TODO: emit token
            r->state = OUT;
            return r->kw_tok;
        }
    } else {
        // TODO: error
    }
    return TOK_CONTINUE;
}

inline static int do_INT(mj_reader_t *r, char ch) {
    if (is_digit(ch)) {
        r->acc = (r->acc * 10) + (ch - '0');
        return TOK_CONTINUE;
    } else if (ch == '.') {
        r->value.i = r->acc;
        r->divisor = 1;
        r->acc = 0;
        r->state = FLOAT;
        return TOK_CONTINUE;
    }
    r->value.i = r->acc;
    r->state =  OUT;
    return TOK_INT;
}

inline static int do_FLOAT(mj_reader_t *r, char ch) {
    if (is_digit(ch)) {
        r->acc = (r->acc * 10) + (ch - '0');
        r->divisor *= 10;
        return TOK_CONTINUE;
    } else {
        int ipart = r->value.i;
        // TODO: just put number in string buffer and use strtof/strtod
        r->value.f = (MINI_JSON_FLOAT_TYPE)ipart + ((MINI_JSON_FLOAT_TYPE)r->acc / r->divisor);
        r->state = OUT;
        return TOK_FLOAT;
    }
}

inline static int do_STR(mj_reader_t *r, char ch) {
    if (ch == '"') {
        r->state = OUT;
        return TOK_STRING;
    }

    if (ch == '\\') {
        r->state = STR_ESC;
    }

    return TOK_CONTINUE;
}

inline static int do_STR_ESC(mj_reader_t *r, char ch) {
    switch (ch) {
        case 'b':
            break;
        case 'f':
            break;
        case 'n':
            break;
        case 'r':
            break;
        case 't':
            break;
        case '"':
            break;
        case '\\':
            break;
        default:
            // TODO: error
            break;
    }

    return TOK_CONTINUE;
}

void mj_reader_init(mj_reader_t *r, char *string_buffer, int string_buffer_len) {
    r->strbuf = string_buffer;
    r->strbuf_len = string_buffer_len;
    r->callback = nop;  
    r->userdata = NULL;
    r->state = OUT;
}

void mj_reader_set_callback(mj_reader_t *r, mj_callback_fn cb, void *userdata) {
    r->callback = (cb == NULL) ? nop : cb;
    r->userdata = userdata;
}

void mj_reader_push(mj_reader_t *r, const char *data, int len) {
    for (int i = 0; i < len; ++i) {
        char ch = data[i];
        int tok;
        switch (r->state) {
            case OUT:       tok = do_OUT(r, ch);        break;
            case KEYWORD:   tok = do_KEYWORD(r, ch);    break;
            case INT:       tok = do_INT(r, ch);        break;
            case FLOAT:     tok = do_FLOAT(r, ch);      break;
            case STR:       tok = do_STR(r, ch);        break;
            case STR_ESC:   tok = do_STR_ESC(r, ch);    break;
        }
        if (tok > TOK_CONTINUE) {
            if (tok == TOK_INT) {
                printf("token: %d (val=%d)\n", tok, r->value.i);
            } else if (tok == TOK_FLOAT) {
                printf("token: %d (val=%f)\n", tok, r->value.f);
            } else {
                printf("token: %d\n", tok);
            }
        }
    }
}

void mj_reader_push_end(mj_reader_t *r) {
    char eof = 0;
    mj_reader_push(r, &eof, 1);
}

//
// Writing

#define CHECK(err) do { if ((err) != MJ_OK) { return (err); } } while(0)
#define P(c) w->buffer[w->sp++] = (c)

static int push_start(mj_writer_t *w, char c) {
    if (w->sp < w->ep) {
        P(c);
        return MJ_OK;
    } else {
        return MJ_NOMEM;
    }
}

static int push_end(mj_writer_t *w, char c) {
    if (w->ep > w->sp) {
        w->buffer[--w->ep] = c;
        return MJ_OK;
    } else {
        return MJ_NOMEM;
    }
}

static char pop_end(mj_writer_t *w) {
    return w->buffer[w->ep++];
}

static int push_string(mj_writer_t *w, const char *str) {
    int len = strlen(str);
    if (w->sp + len > w->ep) {
        return MJ_NOMEM;
    }
    for (int i = 0; i < len; ++i) {
        P(str[i]);
    }
    return MJ_OK;
}

static int push_json_string(mj_writer_t *w, const char *str) {
    int len = 0;

    const char *tmp = str;
    while (*tmp) {
        char ch = *(tmp++);
        if (ch == '\b' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '"' || ch == '\\') {
            len += 2;
        } else {
            len += 1;
        }
    }

    if (w->sp + len + 2 > w->ep) {
        return MJ_NOMEM;
    }

    P('"');
    while (*str) {
        char ch = *(str++);
        switch (ch) {
            case '\b': P('\\'); P('b'); break;
            case '\f': P('\\'); P('f'); break;
            case '\n': P('\\'); P('n'); break;
            case '\r': P('\\'); P('r'); break;
            case '\t': P('\\'); P('t'); break;
            case '"' : P('\\'); P('"'); break;
            case '\\': P('\\'); P('\\'); break;
            default:   P(ch);
        }
    }
    P('"');

    return MJ_OK;
}

static int comma(mj_writer_t *w) {
    if (w->sp == 0)
        return MJ_OK;
    
    char prev = w->buffer[w->sp - 1];
    if (prev == '{' || prev == '[' || prev == ':')
        return MJ_OK;

    return push_start(w, ',');
}

void mj_writer_init(mj_writer_t *w, char *buffer, int len) {
    w->buffer = buffer;
    w->buffer_len = len;
    w->sp = 0;
    w->ep = len;
    w->depth = 0;
}

int mj_writer_start_array(mj_writer_t *w) {
    CHECK(comma(w));
    CHECK(push_start(w, '['));
    CHECK(push_end(w, ']'));
    w->depth++;
    return MJ_OK;
}

int mj_writer_start_object(mj_writer_t *w) {
    CHECK(comma(w));
    CHECK(push_start(w, '{'));
    CHECK(push_end(w, '}'));
    w->depth++;
    return MJ_OK;
}

int mj_writer_end(mj_writer_t *w) {
    if (w->depth < 0)
        return MJ_STATE;

    int ret = MJ_OK;
    if (w->depth == 0) {
        CHECK(push_start(w, '\0'));
        ret = w->sp - 1;
    } else if (w->depth > 0) {
        push_start(w, pop_end(w));
    }

    w->depth--;
    return ret;
}

int mj_writer_put_key(mj_writer_t *w, const char *key) {
    CHECK(comma(w));
    CHECK(push_json_string(w, key));
    return push_start(w, ':');
}

int mj_writer_put_bool(mj_writer_t *w, int val) {
    CHECK(comma(w));
    return push_string(w, val ? "true" : "false");
}

int mj_writer_put_string(mj_writer_t *w, const char *str) {
    CHECK(comma(w));
    return push_json_string(w, str);
}

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define LSHM(v, s, m) (((v) << (s)) & (m))
#define RSHM(v, s, m) (((v) >> (s)) & (m))

int mj_writer_put_base64(mj_writer_t *w, const char *data, int len) {
    CHECK(comma(w));
    
    int encoded_len = ((4 * len / 3) + 3) & ~3;
    if (w->sp + encoded_len + 2 > w->ep)
        return MJ_NOMEM;

    P('"');
    
    int end = w->sp + encoded_len;
    for (int i = 0; i < len; i += 3) {
        P(base64_table[RSHM(data[i], 2, 0x3F)]);
        char e = LSHM(data[i], 4, 0x30);
        if (i + 1 < len) {
            P(base64_table[e | RSHM(data[i+1], 4, 0x0F)]);
            e = LSHM(data[i+1], 2, 0x3C);
            if (i + 2 < len) {
                P(base64_table[e | RSHM(data[i+2], 6, 0x03)]);
                P(base64_table[LSHM(data[i+2], 0, 0x3F)]);
            } else {
                P(base64_table[e]);
            }
        } else {
            P(base64_table[e]); 
        }
    }

    while (w->sp < end) {
        P('=');
    }

    P('"');

    return MJ_OK;
}

int mj_writer_put_null(mj_writer_t *w) {
    CHECK(comma(w));
    return push_string(w, "null");
}

#ifdef MINI_JSON_USE_SNPRINTF

#include <stdio.h>

int mj_writer_put_int(mj_writer_t *w, int val) {
    CHECK(comma(w));
    int maxlen = w->ep - w->sp;
    int written = snprintf(&w->buffer[w->sp], maxlen, "%d", val);
    if (written >= maxlen) {
        return  MJ_NOMEM;
    }
    w->sp += written;
    return MJ_OK;
}

int mj_writer_put_float(mj_writer_t *w, float val) {
    CHECK(comma(w));
    int maxlen = w->ep - w->sp;
    int written = snprintf(&w->buffer[w->sp], maxlen, "%f", val);
    if (written >= maxlen) {
        return  MJ_NOMEM;
    }
    w->sp += written;
    return MJ_OK;
}

int mj_writer_put_double(mj_writer_t *w, double val) {
    CHECK(comma(w));
    int maxlen = w->ep - w->sp;
    int written = snprintf(&w->buffer[w->sp], maxlen, "%f", val);
    if (written >= maxlen) {
        return  MJ_NOMEM;
    }
    w->sp += written;
    return MJ_OK;
}

#else

int mj_writer_put_int(mj_writer_t *w, int val) {
    CHECK(comma(w));
    
    if (val == 0) {
        return push_start(w, '0');
    }

    if (val < 0) {
        CHECK(push_start(w, '-'));
        val = -val;
    }

    int len = 0, tmp = val;
    while (tmp > 0) {
        tmp /= 10;
        len++;
    }

    int end = w->sp + len;
    if (end > w->ep) {
        return MJ_NOMEM;
    }

    w->sp = end;
    
    while (val != 0) {
        w->buffer[--end] = '0' + (val % 10);
        val /= 10;
    }

    return MJ_OK;
}

int mj_writer_put_float(mj_writer_t *w, float val) {
    return MJ_UNSUPPORTED;
}

int mj_writer_put_double(mj_writer_t *w, double val) {
    return MJ_UNSUPPORTED;
}

#endif