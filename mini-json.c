#include "mini-json.h"
#include <string.h>

#define CHECK(err) do { if ((err) != MJ_OK) { return (err); } } while(0)

static int push_start(mj_writer_t *w, char c) {
	if (w->sp < w->ep) {
		w->buffer[w->sp++] = c;
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
		w->buffer[w->sp++] = str[i];
	}
	return MJ_OK;
}

static int push_json_string(mj_writer_t *w, const char *str) {
	int len = strlen(str);
	// TODO: proper escaping
	if (w->sp + len + 2 > w->ep) {
		return MJ_NOMEM;
	}
	w->buffer[w->sp++] = '"';
	for (int i = 0; i < len; ++i) {
		w->buffer[w->sp++] = str[i];
	}
	w->buffer[w->sp++] = '"';
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

mj_status_t mj_writer_start_array(mj_writer_t *w) {
	CHECK(comma(w));
	CHECK(push_start(w, '['));
	CHECK(push_end(w, ']'));
	w->depth++;
	return MJ_OK;
}

mj_status_t mj_writer_start_object(mj_writer_t *w) {
	CHECK(comma(w));
	CHECK(push_start(w, '{'));
	CHECK(push_end(w, '}'));
	w->depth++;
	return MJ_OK;
}

mj_status_t mj_writer_end(mj_writer_t *w) {
	if (w->depth == 0) {
		CHECK(push_start(w, '\0'));
	} else if (w->depth > 0) {
		push_start(w, pop_end(w));
	} else {
		return MJ_STATE;
	}
	w->depth--;
	return MJ_OK;
}

mj_status_t mj_writer_put_key(mj_writer_t *w, const char *key) {
	CHECK(comma(w));
	CHECK(push_json_string(w, key));
	return push_start(w, ':');
}

mj_status_t mj_writer_put_bool(mj_writer_t *w, int val) {
	CHECK(comma(w));
	return push_string(w, val ? "true" : "false");
}

mj_status_t mj_writer_put_string(mj_writer_t *w, const char *str) {
	CHECK(comma(w));
	return push_json_string(w, str);
}