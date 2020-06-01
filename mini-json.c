#include "mini-json.h"
#include <string.h>
#include <stdlib.h>

#include <stdio.h>

#ifdef MINI_JSON_DECODE_INT_TYPE
extern MINI_JSON_DECODE_INT_TYPE mj_decode_int(const char *start, int len);
#endif

#ifdef MINI_JSON_DECODE_FLOAT_TYPE
extern MINI_JSON_DECODE_FLOAT_TYPE mj_decode_float(const char *start, int len);
#endif

//
//

#define CHECK(err) do { if ((err) < 0) { return (err); } } while(0)

//
// Reading

static inline void fire(mj_reader_t *r, int evt) {
    if (r->callback != NULL) {
        r->callback(r, evt);
    }
}

// +ve return values from tokeniser is next action
enum {
    CONTINUE = 1,   // proceed to next char
    AGAIN = 2,      // tokenise again with same char
};

// state
enum {
    OUT,

    // tokenisation
    KEYWORD,
    SIGN,
    INT,
    FLOAT,
    STR,
    STR_ESC,
    STR_END,

    // parsing
    SCALAR,
    OBJECT,
    OBJECT_KEY,
    OBJECT_VALUE,
    ARRAY,
    ARRAY_VALUE,
    DONE
};

// tokens
enum {
    TOK_OBJECT_END,
    TOK_OBJECT_KEY,
    TOK_ARRAY_END,
    TOK_COMMA,
    TOK_EOF,

    TOK_VALUE, // dummy
    TOK_OBJECT_START,
    TOK_ARRAY_START,

    TOK_SCALAR, // dummy
    TOK_NULL,
    TOK_BOOL,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
};

static int is_whitespace(char ch) {
    return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
}

static int is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

static int reader_push_start(mj_reader_t *r, char ch) {
    if (r->sp >= r->ep) {
        return MJ_NOMEM;
    }
    r->strbuf[r->sp++] = ch;
    return MJ_OK;
}

static int reader_push_parse_state(mj_reader_t *r) {
    if (r->ep - r->sp < 4) {
        return MJ_NOMEM;
    }
    r->strbuf[--r->ep] = r->parse_state;
    r->strbuf[--r->ep] = r->child_index >> 16;
    r->strbuf[--r->ep] = r->child_index >> 8;
    r->strbuf[--r->ep] = r->child_index >> 0;
    r->depth++;
    return CONTINUE;
}

static int reader_pop_parse_state(mj_reader_t *r) {
    uint32_t ci1 = r->strbuf[r->ep++];
    uint32_t ci2 = r->strbuf[r->ep++];
    uint32_t ci3 = r->strbuf[r->ep++];

    r->child_index = ci1 | (ci2 << 8) | (ci3 << 16);
    r->parse_state = r->strbuf[r->ep++];
    r->depth--;

    // we only ever pop the parse state when we're exiting a scope
    r->parse_state++;
    
    return CONTINUE;
}

static int reader_parse_inner_value(mj_reader_t *r, int tok) {
    if (tok == TOK_OBJECT_START || tok == TOK_ARRAY_START) {
        fire(r, tok);
        r->child_index++;
        reader_push_parse_state(r);
        r->parse_state = (tok == TOK_OBJECT_START) ? OBJECT : ARRAY;
        r->child_index = 0;
        return CONTINUE;
    } else if (tok > TOK_SCALAR) {
        fire(r, tok);
        r->child_index++;
        r->parse_state++;
        return CONTINUE;
    } else {
        return MJ_PARSE_ERROR;
    }

        // if (r->callback) {
    //     r->callback(r, r->userdata);
    // }
    // if (tok == TOK_INT) {
    //     printf("token: %s (%d)\n", token_names[tok], r->value.i);
    // } else if (tok == TOK_FLOAT) {
    //     printf("token: %s (%f)\n", token_names[tok], r->value.f);
    // } else if (tok == TOK_STRING) {
    //     printf("token: %s (%s)\n", token_names[tok], r->strbuf + r->start);
    // } else if (tok == TOK_OBJECT_KEY) {
    //     printf("token: %s (%.*s)\n", token_names[tok], r->sp - r->start, r->strbuf + r->start);
    // } else {
    //     printf("token: %s\n", token_names[tok]);
    // }
}

// TODO: fix depth handling
// TODO: handle EOF correctly
//
// Called after a token is parsed successfully.
//
// If the tok > TOK_SCALAR, or tok == TOK_OBJECT_KEY, the null-terminated
// token text can be found at r->strbuf[r->start]. If the token is
// TOK_OBJECT_KEY or TOK_STRING this text will be the string value, without
// surrounding quotes, and with escape characters decoded.
static int reader_handle_token(mj_reader_t *r, int tok) {
    int ret = CONTINUE;
    switch (r->parse_state) {
        case OUT:
            if (tok == TOK_OBJECT_START || tok == TOK_ARRAY_START) {
                r->depth = 1;
                r->parse_state = (tok == TOK_OBJECT_START) ? OBJECT : ARRAY;
            } else if (tok > TOK_SCALAR) {
                // TODO: fire callback
                r->parse_state = SCALAR;
            } else {
                ret = MJ_PARSE_ERROR;
            }
            break;
        case OBJECT:
            if (tok == TOK_OBJECT_END) {
                ret = reader_pop_parse_state(r);
            } else if (tok == TOK_OBJECT_KEY) {
                // TODO: push path
                r->parse_state = OBJECT_KEY;
            }
            break;
        case OBJECT_KEY:
            ret = reader_parse_inner_value(r, tok);
            break;
        case OBJECT_VALUE:
            if (tok == TOK_COMMA) {
                r->parse_state = OBJECT;
            } else if (tok == TOK_OBJECT_END) {
                ret = reader_pop_parse_state(r);
            } else {
                ret = MJ_PARSE_ERROR;
            }
            break;
        case ARRAY:
            if (tok == TOK_ARRAY_END) {
                ret = reader_pop_parse_state(r);
            } else {
                ret = reader_parse_inner_value(r, tok);
            }
            break;
        case ARRAY_VALUE:
            if (tok == TOK_COMMA) {
                r->parse_state = ARRAY;
            } else if (tok == TOK_ARRAY_END) {
                ret = reader_pop_parse_state(r);
            } else {
                ret = MJ_PARSE_ERROR;
            }
            break;
        case SCALAR:
            if (tok == TOK_EOF) {
                r->parse_state = DONE;
            } else {
                ret = MJ_PARSE_ERROR;
            }
            break;
    }
    return ret;
}

// Keywords are handled by copying them into the buffer and then
// incrementally comparing as more characters become available.
inline static int reader_start_keyword(mj_reader_t *r, const char *keyword, int len) {
    r->tok_state = KEYWORD;
    r->sp = r->start;
    if (r->sp + len + 1 > r->ep) {
        return MJ_NOMEM;
    }
    int wp = r->sp;
    while (*keyword) {
        r->strbuf[wp++] = *keyword++;
    }
    r->strbuf[wp++] = 0;
    r->sp++; // we've already seen the first character
    return CONTINUE;
}

inline static int do_OUT(mj_reader_t *r, char ch) {
    switch (ch) {
        case '{': return reader_handle_token(r, TOK_OBJECT_START);
        case '}': return reader_handle_token(r, TOK_OBJECT_END);
        case '[': return reader_handle_token(r, TOK_ARRAY_START);
        case ']': return reader_handle_token(r, TOK_ARRAY_END);
        case ',': return reader_handle_token(r, TOK_COMMA);
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            r->sp = r->start;
            CHECK(reader_push_start(r, ch));
            r->tok_state = INT;
            break;
        case '-':
        case '+':
            r->sp = r->start;
            CHECK(reader_push_start(r, ch));
            r->tok_state = SIGN;
            break;
        case '"':
            r->sp = r->start;
            r->tok_state = STR;
            break;
        case 't':
            r->value.b = 1;
            CHECK(reader_start_keyword(r, "true", 4));
            break;
        case 'f':
            r->value.b = 0;
            CHECK(reader_start_keyword(r, "false", 5));
            break;
        case 'n':
            CHECK(reader_start_keyword(r, "null", 4));
            break;
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            // skip whitespace
            break;
        case 0:
            return reader_handle_token(r, TOK_EOF);
        default:
            return MJ_TOK_ERROR;
    }
    return CONTINUE;
}

inline static int do_KEYWORD(mj_reader_t *r, char ch) {
    if (ch == r->strbuf[r->sp]) {
        r->sp++;
        if (r->strbuf[r->sp] == 0) {
            r->tok_state = OUT;
            int tok = (r->strbuf[r->start] == 'n') ? TOK_NULL : TOK_BOOL;
            return reader_handle_token(r, tok);
        } else {
            return CONTINUE;
        }
    } else {
        return MJ_TOK_ERROR;
    }
}

inline static int do_SIGN(mj_reader_t *r, char ch) {
    if (is_digit(ch)) {
        CHECK(reader_push_start(r, ch));
        r->tok_state = INT;
        return CONTINUE;
    } else {
        return MJ_TOK_ERROR;
    }
}

inline static int do_INT(mj_reader_t *r, char ch) {
    if (is_digit(ch)) {
        CHECK(reader_push_start(r, ch));
        return CONTINUE;
    } else if (ch == '.') {
        CHECK(reader_push_start(r, ch));
        r->tok_state = FLOAT;
        return CONTINUE;
    } else {
#ifdef MINI_JSON_DECODE_INT_TYPE
        int len = r->sp - r->start;
        CHECK(reader_push_start(r, 0));
        r->value.i = mj_decode_int(r->strbuf + r->start, len);
#endif
        reader_handle_token(r, TOK_INT);
        r->tok_state = OUT;
        return AGAIN;
    }
    
}

inline static int do_FLOAT(mj_reader_t *r, char ch) {
    if (is_digit(ch)) {
        CHECK(reader_push_start(r, ch));
        return CONTINUE;
    } else {
#ifdef MINI_JSON_DECODE_FLOAT_TYPE
        int len = r->sp - r->start;
        CHECK(reader_push_start(r, 0));
        r->value.f = mj_decode_float(r->strbuf + r->start, len);
#endif
        reader_handle_token(r, TOK_FLOAT);
        r->tok_state = OUT;
        return AGAIN;
    }
}

inline static int do_STR(mj_reader_t *r, char ch) {
    if (ch == '"') {
        CHECK(reader_push_start(r, 0));
        r->tok_state = STR_END;
        return CONTINUE;
    }

    if (ch == '\\') {
        r->tok_state = STR_ESC;
        return CONTINUE;
    }

    CHECK(reader_push_start(r, ch));

    return CONTINUE;
}

inline static int do_STR_ESC(mj_reader_t *r, char ch) {
    switch (ch) {
        case 'b':   CHECK(reader_push_start(r, '\b')); break;
        case 'f':   CHECK(reader_push_start(r, '\f')); break;
        case 'n':   CHECK(reader_push_start(r, '\n')); break;
        case 'r':   CHECK(reader_push_start(r, '\r')); break;
        case 't':   CHECK(reader_push_start(r, '\t')); break;
        case '"':   CHECK(reader_push_start(r, '"'));  break;
        case '\\':  CHECK(reader_push_start(r, '\\')); break;
        default:    return MJ_TOK_ERROR;
    }
    r->tok_state = STR;
    return CONTINUE;
}

inline static int do_STR_END(mj_reader_t *r, char ch) {
    if (is_whitespace(ch)) {
        return CONTINUE;
    }

    r->tok_state = OUT;

    if (ch == ':') {
        reader_handle_token(r, TOK_OBJECT_KEY);
        return CONTINUE;
    }

    reader_handle_token(r, TOK_STRING);
    return AGAIN;
}

void mj_reader_init(mj_reader_t *r, char *string_buffer, int string_buffer_len) {
    r->strbuf = string_buffer;
    r->strbuf_len = string_buffer_len;

    r->callback = NULL;
    
    r->start = 0;
    r->sp = 0;
    r->ep = string_buffer_len;

    r->depth = 0;
    r->parse_state = OUT;
    r->child_index = 0;

    r->tok_state = OUT;
}

void mj_reader_set_callback(mj_reader_t *r, mj_callback_fn cb) {
    r->callback = cb;
}

int mj_reader_push(mj_reader_t *r, const char *data, int len) {
    for (int i = 0; i < len;) {
        char ch = data[i];
        int res;
        switch (r->tok_state) {
            case OUT:       res = do_OUT(r, ch);        break;
            case KEYWORD:   res = do_KEYWORD(r, ch);    break;
            case SIGN:      res = do_SIGN(r, ch);       break;
            case INT:       res = do_INT(r, ch);        break;
            case FLOAT:     res = do_FLOAT(r, ch);      break;
            case STR:       res = do_STR(r, ch);        break;
            case STR_ESC:   res = do_STR_ESC(r, ch);    break;
            case STR_END:   res = do_STR_END(r, ch);    break;
            default:        res = r->tok_state;         break;
        }
        if (res < 0) {
            r->tok_state = res;
            return res;
        } if (res == CONTINUE) {
            i++;
        }
    }
    return MJ_OK;
}

void mj_reader_push_end(mj_reader_t *r) {
    char eof = 0;
    mj_reader_push(r, &eof, 1);
}

//
// Writing

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

int mj_writer_put_long(mj_writer_t *w, long val) {
    CHECK(comma(w));
    int maxlen = w->ep - w->sp;
    int written = snprintf(&w->buffer[w->sp], maxlen, "%ld", val);
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

// maybe this should just call mj_writer_put_long() ?
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

int mj_writer_put_long(mj_writer_t *w, long val) {
    CHECK(comma(w));
    
    if (val == 0) {
        return push_start(w, '0');
    }

    if (val < 0) {
        CHECK(push_start(w, '-'));
        val = -val;
    }

    long len = 0, tmp = val;
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