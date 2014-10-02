#ifndef INCLUDED_CREA_STRUTIL_H
#define INCLUDED_CREA_STRUTIL_H

char *strrep(char *original, char *pattern, char *replacement);

typedef struct pathsplit_s {
    char *path;
    char *filename;
    char *extension;
} pathsplit_t;

pathsplit_t *path_split(char *path);

#endif /* INCLUDED_CREA_STRUTIL_H */
