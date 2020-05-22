# mini-json

`mini-json` is a lightweight JSON library designed for use in embedded systems.

Core principles:

  - no dynamic memory allocation
  - minimal dependence on stdlib (currently `strlen()` and, optionally, `snprintf()`)

The API is designed to be simple and explict, exposing one function for each JSON type. The start/end pattern utilised for objects and arrays allows for arbitrary nesting with no performance penalty, and all state is maintained within `mj_writer_t*` structs so it's possible to pass these around and so that JSON can be composed from multiple subsystems in your program.

At present only the encoder is written. I'll likely add a streaming decoder in the near future too.

Status: working; unit tests required.



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
```

Output:

```json
Here's your JSON (length=152):
["device-state",{"battery-level":47.799999,"wifi-ssid":"vigor","wifi-link-quality":52,"binary-data":"bG92ZSB0YWtlIG1lIGRvd24gKHRvIHRoZSBzdHJlZXRzKQ=="}]
```

### API

#### `void mj_writer_init(mj_writer_t *w, char *buffer, int len)`

Initialise writer `w` with the given `buffer` of length `len`.

#### `int mj_writer_start_array(mj_writer_t *w)`

Start a new array.

Returns `MJ_OK` on success or error code on failure.

#### `int mj_writer_start_object(mj_writer_t *w)`

Start a new object.

Returns `MJ_OK` on success or error code on failure.

#### `int mj_writer_end(mj_writer_t *w)`

If currently inside an array or object, `mj_writer_end` inserts the correct closing symbol (either `]` or `}`), returning `MJ_OK` on success or error code on failure.

If at the top level, a null terminator is written to the underlying buffer, returning the final length of the JSON string on success (excluding null terminator), or error code on failure.

#### `int mj_writer_put_key(mj_writer_t *w, const char *key)`

Write an object key.

Returns `MJ_OK` on success or error code on failure.

#### `int mj_writer_put_bool(mj_writer_t *w, int val)`

Write a boolean value.

Returns `MJ_OK` on success or error code on failure.

#### `int mj_writer_put_string(mj_writer_t *w, const char *str)`

Write a string value.

Returns `MJ_OK` on success or error code on failure.

#### `int mj_writer_put_base64(mj_writer_t *w, char *data, int len)`

Writes `len` bytes of binary `data` encoded as base64, with padding.

Returns `MJ_OK` on success or error code on failure.

#### `int mj_writer_put_null(mj_writer_t *w)`

Write a null value.

Returns `MJ_OK` on success or error code on failure.

#### `int mj_writer_put_int(mj_writer_t *w, int val)`

Write an int value.

Returns `MJ_OK` on success or error code on failure.

#### `int mj_writer_put_float(mj_writer_t *w, float val)`

Write a float value (see Notes, below).

Returns `MJ_OK` on success or error code on failure.

#### `int mj_writer_put_double(mj_writer_t *w, double val)`

Write a double value (see Notes, below).

Returns `MJ_OK` on success or error code on failure.

## Notes

### `float` and `double` support

At present `mini-json` only supports the encoding of floats and doubles via `snprintf()`; if you wish to use this functionality you must opt-in by compiling with the define `MINI_JSON_USE_SNPRINTF`. If you try to write a floating point value without this compilation flag `mini-json` will return the status `MJ_UNSUPPORTED`.
