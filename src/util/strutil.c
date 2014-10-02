#include <stdlib.h>
#include <string.h>

#include "strutil.h"

char *
strrep(char *original,
       char *pattern,
       char *replacement)
{
    size_t olen;
    size_t plen;
    size_t rlen;
    uint32_t count = 0;
    uint32_t len_front;
    char *tmp;
    char *ins;
    char *result;

    if (original == NULL) {
        return NULL;
    }
    olen = strlen(original);

#define STRLEN_EMPTY_IF_NULL(str, len) \
    do {\
        if ((str) == NULL) { \
            str = ""; \
            len = 0; \
        } else { \
            len = strlen(str); \
        } \
    } while (0);

    STRLEN_EMPTY_IF_NULL(pattern, plen);
    STRLEN_EMPTY_IF_NULL(replacement, rlen);

    for (ins = original, count = 0; tmp = strstr(ins, pattern); ++count) {
        ins = tmp + plen;
    }

    tmp = result = malloc(olen + (rlen - plen) * count + 1);
    if (result == NULL) {
        return NULL;
    }
    while (count--) {
        ins = strstr(original, pattern);
        len_front = ins - original;
        tmp = strncpy(tmp, original, len_front) + len_front;
        tmp = strcpy(tmp, replacement) + rlen;
        original += len_front + plen;
    }
    strcpy(tmp, original);
    return result;
}

pathsplit_t *
path_split(char *path)
{
    char *cleaned, *tmp;
    size_t len, sz;
    size_t tlen, plen;
    pathsplit_t *ret;

    if (path == NULL) {
        return NULL;
    }
    cleaned = strrep(path, "\\\\", "/");
    len = strlen(cleaned);
    ret = malloc(sizeof(pathsplit_t));
    tmp = strrchr(cleaned, '/');
    if (tmp == NULL) {
        ret->path = NULL;
        tmp = strrchr(cleaned, '.');
        if (tmp == NULL) {
            ret->extension = NULL;
            ret->filename = strdup(cleaned);
        } else {
            tlen = strlen(tmp);
            sz = len - tlen;
            ret->filename = malloc(sz + 1);
            strncpy(ret->filename, cleaned, sz);
            ret->filename[sz] = '\0';
            ret->extension = malloc(tlen);
            strncpy(ret->extension, (cleaned + len) - (tlen - 1), tlen - 1);
            ret->extension[tlen] = '\0';
        }
    } else {
        ret->path = malloc((tmp - cleaned) + 1);
        strncpy(ret->path, cleaned, tmp - cleaned);
        ret->path[(tmp - cleaned)] = '\0';
        tmp = strrchr(cleaned, '.');
        plen = strlen(ret->path);
        if (tmp == NULL) {
            ret->extension = NULL;
            ret->filename = strdup(cleaned + plen + 1);
        } else {
            tlen = strlen(tmp);
            sz = len - (plen + 1) - tlen;
            ret->filename = malloc(sz + 1);
            strncpy(ret->filename, cleaned + plen + 1, sz);
            ret->filename[sz] = '\0';
            ret->extension = malloc(tlen);
            strncpy(ret->extension, (cleaned + len) - (tlen - 1), tlen - 1);
            ret->extension[tlen] = '\0';
        }
    }
    free(cleaned);

    return ret;
}
