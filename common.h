/**
 * @file common.h
 * Common macros.
 */

#define MAX_KEY_SIZE 128
#define MAX_VALUE_SIZE 256
#define MAX_TAG_SIZE 20
#define MAX_ENTRY_SIZE (MAX_VALUE_SIZE + MAX_KEY_SIZE + MAX_TAG_SIZE)
#define LINE_SIZE (MAX_ENTRY_SIZE + 3 + 4 + 10 + 10)

#define PORT 8086
