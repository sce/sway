#ifndef _SWAY_STRINGOP_H
#define _SWAY_STRINGOP_H
#include "list.h"

#if !HAVE_DECL_SETENV
// Not sure why we need to provide this
extern int setenv(const char *, const char *, int);
#endif

// array of whitespace characters to use for delims
extern const char whitespace[];

char *strip_whitespace(char *str);
char *strip_comments(char *str);
void strip_quotes(char *str);

// Simply split a string with delims, free with `free_flat_list`
list_t *split_string(const char *str, const char *delims);
void free_flat_list(list_t *list);

// Splits an argument string, keeping quotes intact
char **split_args(const char *str, int *argc);
void free_argv(int argc, char **argv);

char *code_strchr(const char *string, char delimiter);
char *code_strstr(const char *haystack, const char *needle);
int unescape_string(char *string);
char *join_args(char **argv, int argc);
char *join_list(list_t *list, char *separator);

// split string into 2 by delim.
char *cmdsep(char **stringp, const char *delim);
// Split string into 2 by delim, handle quotes
char *argsep(char **stringp, const char *delim);

char *strdup(const char *);

char **reverse_sprintf(int argc, char** argv, const char *format, ...);
#endif
