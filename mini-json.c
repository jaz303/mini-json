#include "mini-json.h"
#include <string.h>

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