main: main.c mini-json.c mini-json.h
	gcc -o main -DMINI_JSON_USE_SNPRINTF -DMINI_JSON_DECODE_INT_TYPE=int -DMINI_JSON_DECODE_FLOAT_TYPE=double main.c mini-json.c
