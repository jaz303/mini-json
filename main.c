#include "mini-json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
	mj_writer_t w;
	char buffer[512];
	char bindata[] = "love take me down (to the streets)";

	mj_writer_init(&w, buffer, 512);
	
	mj_writer_start_array(&w);
		mj_writer_put_string(&w, "device-state");
		mj_writer_start_object(&w);
			mj_writer_put_key(&w, "battery-level");
			mj_writer_put_float(&w, 47.8);
			mj_writer_put_key(&w, "wifi-ssid");
			mj_writer_put_string(&w, "vigor");
			mj_writer_put_key(&w, "wifi-link-quality");
			mj_writer_put_int(&w, 52);
			mj_writer_put_key(&w, "binary-data");
			mj_writer_put_base64(&w, bindata, strlen(bindata));
		mj_writer_end(&w);
	mj_writer_end(&w);
	
	int len = mj_writer_end(&w);

	printf("Here's your JSON (length=%d):\n", len);
	puts(buffer);

	return 0;
}