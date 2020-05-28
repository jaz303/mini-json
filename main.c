#include "mini-json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cb(void *userdata) {
	puts("got a token!\n");
}

int main() {
	mj_reader_t r;
	char str_buffer[512];
	mj_reader_init(&r, str_buffer, 512);
	mj_reader_set_callback(&r, cb, NULL);

	const char *json = "{ \"foo\": -1234.0016, \"bar\": \"quux\", \"items\": [true, false, null, -123] }";
	mj_reader_push(&r, json, strlen(json));
	mj_reader_push_end(&r);

	// mj_reader_push(&r, "{ \"fo", 5);
	// mj_reader_push(&r, "o\": -12", 7);
	// mj_reader_push(&r, "34.0016, \"moose\": -1992 }", 24);
	
	// mj_writer_t w;
	// char buffer[512];
	// char bindata[] = "love take me down (to the streets)";

	// mj_writer_init(&w, buffer, 512);
	
	// mj_writer_start_array(&w);
	// 	mj_writer_put_string(&w, "device-state");
	// 	mj_writer_start_object(&w);
	// 		mj_writer_put_key(&w, "battery-level");
	// 		mj_writer_put_float(&w, 47.8);
	// 		mj_writer_put_key(&w, "wifi-ssid");
	// 		mj_writer_put_string(&w, "vigor");
	// 		mj_writer_put_key(&w, "wifi-link-quality");
	// 		mj_writer_put_int(&w, 52);
	// 		mj_writer_put_key(&w, "binary-data");
	// 		mj_writer_put_base64(&w, bindata, strlen(bindata));
	// 	mj_writer_end(&w);
	// mj_writer_end(&w);
	
	// int len = mj_writer_end(&w);

	// printf("Here's your JSON (length=%d):\n", len);
	// puts(buffer);

	return 0;
}