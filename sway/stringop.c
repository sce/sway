#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "stringop.h"
#include "log.h"
#include "string.h"
#include "list.h"

const char whitespace[] = " \f\n\r\t\v";

/* Note: This returns 8 characters for trimmed_start per tab character. */
char *strip_whitespace(char *_str) {
	if (*_str == '\0')
		return _str;
	char *strold = _str;
	while (*_str == ' ' || *_str == '\t') {
		_str++;
	}
	char *str = strdup(_str);
	free(strold);
	int i;
	for (i = 0; str[i] != '\0'; ++i);
	do {
		i--;
	} while (i >= 0 && (str[i] == ' ' || str[i] == '\t'));
	str[i + 1] = '\0';
	return str;
}

char *strip_comments(char *str) {
	int in_string = 0, in_character = 0;
	int i = 0;
	while (str[i] != '\0') {
		if (str[i] == '"' && !in_character) {
			in_string = !in_string;
		} else if (str[i] == '\'' && !in_string) {
			in_character = !in_character;
		} else if (!in_character && !in_string) {
			if (str[i] == '#') {
				str[i] = '\0';
				break;
			}
		}
		++i;
	}
	return str;
}

void strip_quotes(char *str) {
	bool in_str = false;
	bool in_chr = false;
	bool escaped = false;
	char *end = strchr(str,0);
	while (*str) {
		if (*str == '\'' && !in_str && !escaped) {
			in_chr = !in_chr;
			goto shift_over;
		} else if (*str == '\"' && !in_chr && !escaped) {
			in_str = !in_str;
			goto shift_over;
		} else if (*str == '\\') {
			escaped = !escaped;
			++str;
			continue;
		}
		escaped = false;
		++str;
		continue;
		shift_over:
		memmove(str, str+1, end-- - str);
	}
	*end = '\0';
}

list_t *split_string(const char *str, const char *delims) {
	list_t *res = create_list();
	char *copy = strdup(str);
	char *token;

	token = strtok(copy, delims);
	while(token) {
		token = strdup(token);
		list_add(res, token);
		token = strtok(NULL, delims);
	}
	free(copy);
	return res;
}

void free_flat_list(list_t *list) {
	int i;
	for (i = 0; i < list->length; ++i) {
		free(list->items[i]);
	}
	list_free(list);
}

char **split_args(const char *start, int *argc) {
	*argc = 0;
	int alloc = 2;
	char **argv = malloc(sizeof(char *) * alloc);
	bool in_token = false;
	bool in_string = false;
	bool in_char = false;
	bool escaped = false;
	const char *end = start;
	if (start) {
		while (*start) {
			if (!in_token) {
				start = (end += strspn(end, whitespace));
				in_token = true;
			}
			if (*end == '"' && !in_char && !escaped) {
				in_string = !in_string;
			} else if (*end == '\'' && !in_string && !escaped) {
				in_char = !in_char;
			} else if (*end == '\\') {
				escaped = !escaped;
			} else if (*end == '\0' || (!in_string && !in_char && !escaped
						&& strchr(whitespace, *end))) {
				goto add_token;
			}
			if (*end != '\\') {
				escaped = false;
			}
			++end;
			continue;
			add_token:
			if (end - start > 0) {
				char *token = malloc(end - start + 1);
				strncpy(token, start, end - start + 1);
				token[end - start] = '\0';
				argv[*argc] = token;
				if (++*argc + 1 == alloc) {
					argv = realloc(argv, (alloc *= 2) * sizeof(char *));
				}
			}
			in_token = false;
			escaped = false;
		}
	}
	argv[*argc] = NULL;
	return argv;
}

void free_argv(int argc, char **argv) {
	while (--argc > 0) {
		free(argv[argc]);
	}
	free(argv);
}

char *code_strstr(const char *haystack, const char *needle) {
	/* TODO */
	return strstr(haystack, needle);
}

char *code_strchr(const char *str, char delimiter) {
	int in_string = 0, in_character = 0;
	int i = 0;
	while (str[i] != '\0') {
		if (str[i] == '"' && !in_character) {
			in_string = !in_string;
		} else if (str[i] == '\'' && !in_string) {
			in_character = !in_character;
		} else if (!in_character && !in_string) {
			if (str[i] == delimiter) {
				return (char *)str + i;
			}
		}
		++i;
	}
	return NULL;
}

int unescape_string(char *string) {
	/* TODO: More C string escapes */
	int len = strlen(string);
	int i;
	for (i = 0; string[i]; ++i) {
		if (string[i] == '\\') {
			switch (string[++i]) {
			case '0':
				string[i - 1] = '\0';
				return i - 1;
			case 'a':
				string[i - 1] = '\a';
				string[i] = '\0';
				break;
			case 'b':
				string[i - 1] = '\b';
				string[i] = '\0';
				break;
			case 'f':
				string[i - 1] = '\f';
				string[i] = '\0';
				break;
			case 'n':
				string[i - 1] = '\n';
				string[i] = '\0';
				break;
			case 'r':
				string[i - 1] = '\r';
				string[i] = '\0';
				break;
			case 't':
				string[i - 1] = '\t';
				string[i] = '\0';
				break;
			case 'v':
				string[i - 1] = '\v';
				string[i] = '\0';
				break;
			case '\\':
				string[i] = '\0';
				break;
			case '\'':
				string[i - 1] = '\'';
				string[i] = '\0';
				break;
			case '\"':
				string[i - 1] = '\"';
				string[i] = '\0';
				break;
			case '?':
				string[i - 1] = '?';
				string[i] = '\0';
				break;
			case 'x':
				{
					unsigned char c = 0;
					if (string[i+1] >= '0' && string[i+1] <= '9') {
						c = string[i+1] - '0';
						if (string[i+2] >= '0' && string[i+2] <= '9') {
							c *= 0x10;
							c += string[i+2] - '0';
							string[i+2] = '\0';
						}
						string[i+1] = '\0';
					}
					string[i] = '\0';
					string[i - 1] = c;
				}
			}
		}
	}
	// Shift characters over nullspaces
	int shift = 0;
	for (i = 0; i < len; ++i) {
		if (string[i] == 0) {
			shift++;
			continue;
		}
		string[i-shift] = string[i];
	}
	string[len - shift] = 0;
	return len - shift;
}

char *join_args(char **argv, int argc) {
	int len = 0, i;
	for (i = 0; i < argc; ++i) {
		len += strlen(argv[i]) + 1;
	}
	char *res = malloc(len);
	len = 0;
	for (i = 0; i < argc; ++i) {
		strcpy(res + len, argv[i]);
		len += strlen(argv[i]);
		res[len++] = ' ';
	}
	res[len - 1] = '\0';
	return res;
}

/*
 * Join a list of strings, adding separator in between. Separator can be NULL.
 */
char *join_list(list_t *list, char *separator) {
	if (!sway_assert(list != NULL, "list != NULL") || list->length == 0) {
		return NULL;
	}

	size_t len = 1; // NULL terminator
	size_t sep_len = 0;
	if (separator != NULL) {
		sep_len = strlen(separator);
		len += (list->length - 1) * sep_len;
	}

	for (int i = 0; i < list->length; i++) {
		len += strlen(list->items[i]);
	}

	char *res = malloc(len);

	char *p = res + strlen(list->items[0]);
	strcpy(res, list->items[0]);

	for (int i = 1; i < list->length; i++) {
		if (sep_len) {
			memcpy(p, separator, sep_len);
			p += sep_len;
		}
		strcpy(p, list->items[i]);
		p += strlen(list->items[i]);
	}

	*p = '\0';

	return res;
}

char *cmdsep(char **stringp, const char *delim) {
	// skip over leading delims
	char *head = *stringp + strspn(*stringp, delim);
	// Find end token
	char *tail = *stringp += strcspn(*stringp, delim);
	// Set stringp to begining of next token
	*stringp += strspn(*stringp, delim);
	// Set stringp to null if last token
	if (!**stringp) *stringp = NULL;
	// Nullify end of first token
	*tail = 0;
	return head;
}

char *argsep(char **stringp, const char *delim) {
	char *start = *stringp;
	char *end = start;
	bool in_string = false;
	bool in_char = false;
	bool escaped = false;
	while (1) {
		if (*end == '"' && !in_char && !escaped) {
			in_string = !in_string;
		} else if (*end == '\'' && !in_string && !escaped) {
			in_char = !in_char;
		} else if (*end == '\\') {
			escaped = !escaped;
		} else if (*end == '\0') {
			*stringp = NULL;
			goto found;
		} else if (!in_string && !in_char && !escaped && strchr(delim, *end)) {
			if (end - start) {
				*(end++) = 0;
				*stringp = end + strspn(end, delim);;
				if (!**stringp) *stringp = NULL;
				goto found;
			} else {
				++start;
				end = start;
			}
		}
		if (*end != '\\') {
			escaped = false;
		}
		++end;
	}
	found:
	return start;
}

char *strdup(const char *str) {
	char *dup = malloc(strlen(str) + 1);
	if (dup) {
		strcpy(dup, str);
	}
	return dup;
}

// Compare given data with given format while ignoring percentage signs. If
// they match, returns data at percentage sign location in data array.
//
// If a number trails behind any percentage sign it is assumed to be the length
// of the next trailing char*[] array, and this function will make sure data extracted
// matches one of the elements in that corresponding array.
//
// Returns NULL if not successful.
//
// In other words, a convenient way to verify string syntax and extract data at
// the same time.
//
//   reverse_sprintf(argc, argv, "move %");
//   reverse_sprintf(argc, argv, "move % to % %");
//   reverse_sprintf(argc, argv, "move % to workspace number %");
//
//   reverse_sprintf(argc, argv, "move %4", (const char*[]){ "left", "right", "up", "down" });
//   reverse_sprintf(argc, argv, "move %2 to %2 %",
//       (const char*[]){ "container", "window" },
//       (const char*[]){ "workspace", "output" }); // last argument is not checked
//
char **reverse_sprintf(int argc, char** argv, const char *format, ...) {
	list_t *format_args = split_string(format, " ");
	if (format_args->length != argc) {
		free_flat_list(format_args);
		return NULL;
	}
	va_list args;
	va_start(args, format);
	char **extracted = calloc(argc, sizeof(char*));
	int found = 0;
	for(int i=0; i<format_args->length; i++) {
		char *token = (char*)format_args->items[i];
		if (*token == '%') {
			extracted[found++] = token;
			int candidates = atoi(token+1);
			if (candidates > 0) {
				bool verified = false;
				const char **filter = va_arg(args, char**);
				for(int i=0; i<candidates; i++) {
					if (strcmp(token, filter[i]) == 0) {
						verified = true;
						break;
					}
				}
				if (!verified) {
					goto rev_spf_failed;
				}
			}
		} else if (strcmp(token, argv[i]) != 0) {
			goto rev_spf_failed;
		}
	}
	va_end(args);
	return extracted;
rev_spf_failed:
	va_end(args);
	free(extracted);
	return NULL;
}
