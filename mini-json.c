#include "mini-json.h"
#include <string.h>
#include <stdlib.h>

#include <stdio.h>

//
//

#define CHECK(err) do { if ((err) < 0) { return (err); } } while(0)

//
// Reading

// +ve return values from tokeniser is next action
enum {
    CONTINUE = 1, // proceed to next char
    AGAIN = 2, // tokenise again with same char
};

// tokeniser state
enum {
    OUT,
    KEYWORD,
    SIGN,
    INT,
    FLOAT,
    STR,
    STR_ESC,
    STR_END
};

// tokens
enum {
    TOK_OBJECT_START = 1, // 1
    TOK_OBJECT_END, // 2
    TOK_OBJECT_KEY, // 3
    TOK_ARRAY_START, // 4
    TOK_ARRAY_END, // 5
    TOK_NULL, // 6
    TOK_TRUE, // 7
    TOK_FALSE, // 8
    TOK_INT, // 9
    TOK_FLOAT, // 10
    TOK_STRING, // 11
    TOK_COMMA, // 12
};

const char *token_names[] = {
    "!",
    "{",
    "}",
    "<object-key>",
    "[",
    "]",
    "null",
    "true",
    "false",
    "<int>",
    "<float>",
    "<string>",
    ","
};

static void nop(void *userdata) {

}

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

static int reader_emit(mj_reader_t *r, int tok) {
    if (tok == TOK_INT) {
        printf("token: %s (%ld)\n", token_names[tok], r->value.i);
    } else if (tok == TOK_FLOAT) {
        printf("token: %s (%f)\n", token_names[tok], r->value.f);
    } else if (tok == TOK_STRING) {
        printf("token: %s (%.*s)\n", token_names[tok], r->sp - r->start, r->strbuf + r->start);
    } else if (tok == TOK_OBJECT_KEY) {
        printf("token: %s (%.*s)\n", token_names[tok], r->sp - r->start, r->strbuf + r->start);
    } else {
        printf("token: %s\n", token_names[tok]);
    }
    return CONTINUE;
}

#define START_KEYWORD(tail, token) \
    r->tok_state = KEYWORD; \
    r->kw_tok = token; \
    r->kw = tail; \
    r->kw_next = 0

inline static int do_OUT(mj_reader_t *r, char ch) {
    switch (ch) {
        case '{': return reader_emit(r, TOK_OBJECT_START);
        case '}': return reader_emit(r, TOK_OBJECT_END);
        case '[': return reader_emit(r, TOK_ARRAY_START);
        case ']': return reader_emit(r, TOK_ARRAY_END);
        case ',': return reader_emit(r, TOK_COMMA);
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
            r->value.i = 1;
            START_KEYWORD("rue", TOK_TRUE);
            break;
        case 'f':
            r->value.i = 0;
            START_KEYWORD("alse", TOK_FALSE);
            break;
        case 'n':
            r->tok_state = KEYWORD;
            START_KEYWORD("ull", TOK_NULL);
            break;
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            break;
        default:
            return MJ_PARSE_ERROR;
    }
    return CONTINUE;
}

inline static int do_KEYWORD(mj_reader_t *r, char ch) {
    if (ch == r->kw[r->kw_next]) {
        r->kw_next++;
        if (r->kw[r->kw_next] == 0) {
            r->tok_state = OUT;
            return reader_emit(r, r->kw_tok);
        } else {
            return CONTINUE;    
        }
    } else {
        return MJ_PARSE_ERROR;
    }
}

inline static int do_SIGN(mj_reader_t *r, char ch) {
    if (is_digit(ch)) {
        CHECK(reader_push_start(r, ch));
        r->tok_state = INT;
        return CONTINUE;
    } else {
        return MJ_PARSE_ERROR;
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
        char *end = r->strbuf + r->sp;
        r->value.i = strtol(r->strbuf + r->start, &end, 10);
        reader_emit(r, TOK_INT);
        r->tok_state = OUT;
        return AGAIN;
    }
    
}

inline static int do_FLOAT(mj_reader_t *r, char ch) {
    if (is_digit(ch)) {
        CHECK(reader_push_start(r, ch));
        return CONTINUE;
    } else {
        char *end = r->strbuf + r->sp;
        r->value.f = strtof(r->strbuf + r->start, &end);
        reader_emit(r, TOK_FLOAT);
        r->tok_state = OUT;
        return AGAIN;
    }
}

inline static int do_STR(mj_reader_t *r, char ch) {
    if (ch == '"') {
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
        default:    return MJ_PARSE_ERROR;
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
        reader_emit(r, TOK_OBJECT_KEY);
        return CONTINUE;
    }

    reader_emit(r, TOK_STRING);
    return AGAIN;
}

void mj_reader_init(mj_reader_t *r, char *string_buffer, int string_buffer_len) {
    r->strbuf = string_buffer;
    r->strbuf_len = string_buffer_len;
    r->callback = nop;  
    r->userdata = NULL;
    r->tok_state = OUT;
    r->start = 0;
    r->sp = 0;
    r->ep = string_buffer_len;
}

void mj_reader_set_callback(mj_reader_t *r, mj_callback_fn cb, void *userdata) {
    r->callback = (cb == NULL) ? nop : cb;
    r->userdata = userdata;
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