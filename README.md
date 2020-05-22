# mini-json

Status: encoder is working; unit tests required.

## Notes

### `float` and `double` support

At present `mini-json` only supports the encoding of floats and doubles via `snprintf()`; if you wish to use this functionality you must opt-in by compiling with the define `MINI_JSON_USE_SNPRINTF`. If you try to write a floating point value without this compilation flag `mini-json` will return the status `MJ_UNSUPPORTED`.
