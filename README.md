# mini-json

Status: encoder is working; unit tests required.

## Writer

### Example

```c
#include "mini-json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
	mj_writer_t w;
	char buffer[512];

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
		mj_writer_end(&w);
	mj_writer_end(&w);
	
	mj_writer_end(&w);

	puts("Here's your JSON:");
	puts(buffer);

	return 0;
}
```

### API

#### `void mj_writer_init(mj_writer_t *w, char *buffer, int len)`

#### `mj_status_t mj_writer_start_array(mj_writer_t *w)`

#### `mj_status_t mj_writer_start_object(mj_writer_t *w)`

#### `mj_status_t mj_writer_end(mj_writer_t *w)`

#### `mj_status_t mj_writer_put_key(mj_writer_t *w, const char *key)`

#### `mj_status_t mj_writer_put_bool(mj_writer_t *w, int val)`

#### `mj_status_t mj_writer_put_string(mj_writer_t *w, const char *str)`

#### `mj_status_t mj_writer_put_null(mj_writer_t *w)`

#### `mj_status_t mj_writer_put_int(mj_writer_t *w, int val)`

#### `mj_status_t mj_writer_put_float(mj_writer_t *w, float val)`

#### `mj_status_t mj_writer_put_double(mj_writer_t *w, double val)`

## Notes

### `float` and `double` support

At present `mini-json` only supports the encoding of floats and doubles via `snprintf()`; if you wish to use this functionality you must opt-in by compiling with the define `MINI_JSON_USE_SNPRINTF`. If you try to write a floating point value without this compilation flag `mini-json` will return the status `MJ_UNSUPPORTED`.
