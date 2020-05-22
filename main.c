#include "mini-json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
	mj_writer_t w;
	char buffer[512];

	mj_writer_init(&w, buffer, 512);
	mj_writer_start_array(&w);
	mj_writer_put_string(&w, "awesome");
	mj_writer_put_bool(&w, 0);
	mj_writer_start_object(&w);
	mj_writer_put_key(&w, "foo");
	mj_writer_put_string(&w, "bar");
	mj_writer_put_key(&w, "bleem");
	mj_writer_put_bool(&w, 0);
	mj_writer_end(&w);
	mj_writer_put_bool(&w, 1);
	mj_writer_put_bool(&w, 0);
	mj_writer_end(&w);
	mj_writer_end(&w);

	puts("Here's your JSON:");
	puts(buffer);

	return 0;
}