main: main.c mini-json.c mini-json.h
	gcc -o main -DMINI_JSON_USE_SNPRINTF main.c mini-json.c
