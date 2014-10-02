#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yaml.h>

#include "util/getopt.h"
#include "util/strutil.h"

#define LOGI(...) fprintf(stdout, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)

typedef struct source {
    char *path;

    struct source *next, *prev;
} source_t;

typedef struct crea {
    char *name;
    char *version;

    source_t *sources;
    source_t *headers;

    char *cflags;
} crea_t;

static int
crea_init(crea_t **crea)
{
    *crea = malloc(sizeof(crea_t));
    memset(*crea, 0, sizeof(crea_t));

    return 0;
}

#define FREE_UNLESS_NULL(x) \
    do { \
        if ((x) != NULL) { \
            free((x)); \
        } \
    } while (0);

static int
add_to_sources(source_t **listp, char *path)
{
    source_t *add = NULL;
    source_t *tmp = NULL;

    if (*listp == NULL) {
        *listp = malloc(sizeof(source_t));
        (*listp)->path = strdup(path);
        (*listp)->next = NULL;
        (*listp)->prev = NULL;
    } else {
        /*
         * Traverse the list until the end, or until we find a duplicate
         */
        for (tmp = *listp; tmp != NULL; tmp = tmp->next) {
            if (!strcmp(path, tmp->path)) {
                printf("Attempted to add duplicate source path: %s\n", path);
                return -1;
            }
            if (tmp->next == NULL) {
                break;
            }
        }
        add = malloc(sizeof(source_t));
        add->path = strdup(path);
        add->next = NULL;
        add->prev = tmp;
        tmp->next = add;
    }

    return 0;
}

static void
free_source_tree(source_t *tree)
{
    FREE_UNLESS_NULL(tree->path);
    tree->prev = NULL;
    if (tree->next != NULL) {
        free_source_tree(tree->next);
        tree->next = NULL;
    }
    free(tree);
}

static int
crea_fini(crea_t **crea)
{
    FREE_UNLESS_NULL((*crea)->name);
    FREE_UNLESS_NULL((*crea)->version);
    FREE_UNLESS_NULL((*crea)->cflags);
    if ((*crea)->sources != NULL) {
        free_source_tree((*crea)->sources);
    }
    if ((*crea)->headers != NULL) {
        free_source_tree((*crea)->headers);
    }

    return 0;
}

int done = 0;

enum {
    STATE_UNKNOWN,
    STATE_ROOT,
    STATE_AUTHORS,
    STATE_CONTRIBUTORS,
    STATE_SOURCES,
    STATE_HEADERS,
    STATE_DEPENDENCIES,
    STATE_CONFIGURATIONS,

    STATE_NAME,
    STATE_VERSION,
    STATE_CFLAGS
};

int state_stack[250] = {STATE_UNKNOWN};
int state_index = -1;

static void
push_state(int state) {
    state_stack[++state_index] = state;
}

static int
pop_state(void) {
    if (state_index < 0) {
        return STATE_UNKNOWN;
    }
    return state_stack[state_index--];
}

static int
get_state(void) {
    return state_stack[state_index];
}

static int
parent_state(void) {
    if (state_index < 1) {
        return STATE_UNKNOWN;
    }
    return state_stack[state_index - 1];
}

static int
is_within_state(int state) {
    int i;

    for (i = state_index; i > -1; i--) {
        if (state_stack[i] == state) {
            return 1;
        }
    }

    return 0;
}

static crea_t *
parse_file(const char *file)
{
    FILE *fin = fopen(file, "rb");
    yaml_parser_t parser;
    yaml_event_t event;
    crea_t *crea;
    char *last_scalar = NULL;
    int level = 0;

    crea_init(&crea);
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fin);
    do {
        if (!yaml_parser_parse(&parser, &event)) {
            LOGE("Parser error %d\n", parser.error);
            goto finish;
        }
        switch(event.type) {
            case YAML_NO_EVENT:
                break;
            case YAML_STREAM_START_EVENT:
                break;
            case YAML_STREAM_END_EVENT:
                break;
            case YAML_DOCUMENT_START_EVENT:
                push_state(STATE_ROOT);
                break;
            case YAML_DOCUMENT_END_EVENT:
                pop_state();
                break;
            case YAML_SEQUENCE_START_EVENT:
                break;
            case YAML_SEQUENCE_END_EVENT:
                pop_state();
                break;
            case YAML_MAPPING_START_EVENT:
                if (last_scalar != NULL) {
                    free(last_scalar);
                }
                last_scalar = NULL;
                break;
            case YAML_MAPPING_END_EVENT:
                pop_state();
                break;
            case YAML_ALIAS_EVENT:
                break;
            case YAML_SCALAR_EVENT:
                switch (get_state()) {
                    case STATE_ROOT:
                        if (!strcmp(event.data.scalar.value, "name")) {
                            push_state(STATE_NAME);
                        } else if (!strcmp(event.data.scalar.value, "version")) {
                            push_state(STATE_VERSION);
                        } else if (!strcmp(event.data.scalar.value, "authors")) {
                            push_state(STATE_AUTHORS);
                        } else if (!strcmp(event.data.scalar.value, "contributors")) {
                            push_state(STATE_CONTRIBUTORS);
                        } else if (!strcmp(event.data.scalar.value, "sources")) {
                            push_state(STATE_SOURCES);
                        } else if (!strcmp(event.data.scalar.value, "headers")) {
                            push_state(STATE_HEADERS);
                        } else if (!strcmp(event.data.scalar.value, "cflags")) {
                            push_state(STATE_CFLAGS);
                        } else if (!strcmp(event.data.scalar.value, "dependencies")) {
                            push_state(STATE_DEPENDENCIES);
                        } else if (!strcmp(event.data.scalar.value, "configurations")) {
                            push_state(STATE_CONFIGURATIONS);
                        }
                        break;
                    case STATE_NAME:
                        if (parent_state() == STATE_ROOT) {
                            crea->name = strdup(event.data.scalar.value);
                        }
                        pop_state();
                        break;
                    case STATE_VERSION:
                        if (parent_state() == STATE_ROOT) {
                            crea->version = strdup(event.data.scalar.value);
                        }
                        pop_state();
                        break;
                    case STATE_SOURCES:
                        if (parent_state() == STATE_ROOT) {
                            add_to_sources(&crea->sources, event.data.scalar.value);
                        }
                        break;
                    case STATE_HEADERS:
                        if (parent_state() == STATE_ROOT) {
                            add_to_sources(&crea->headers, event.data.scalar.value);
                        }
                        break;
                    case STATE_CFLAGS:
                        if (parent_state() == STATE_ROOT) {
                            crea->cflags = strdup(event.data.scalar.value);
                        }
                        pop_state();
                        break;
                }
                if (last_scalar != NULL) {
                    free(last_scalar);
                }
                last_scalar = strdup(event.data.scalar.value);
                break;
        }
        if (event.type != YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
        }
    } while (event.type != YAML_STREAM_END_EVENT);
    yaml_event_delete(&event);
finish:
    yaml_parser_delete(&parser);
    fclose(fin);

    return crea;
}

static void
print_sources(source_t *sources)
{
    source_t *tmp = NULL;

    for (tmp = sources; tmp != NULL; tmp = tmp->next) {
        printf("  %s\n", tmp->path);
    }
}

static void
run_build(crea_t *crea)
{
    source_t *src = crea->sources;
    pathsplit_t *path = NULL;
    char cmd[250];
    char *buffer, *tmp;
    size_t bsz;

    if (src != NULL) {
        system("mkdir -p .crea/obj");
        for (; src != NULL; src = src->next) {
            path = path_split(src->path);
            sprintf(cmd, "clang -c %s -o .crea/obj/%s.o", src->path, path->filename);
            system(cmd);
            free(path);
        }
        bsz = 0;
        for (src = crea->sources; src != NULL; src = src->next) {
            path = path_split(src->path);
            bsz += strlen(".crea/obj/") + strlen(path->filename) + 3;
            free(path);
        }
        tmp = buffer = malloc(bsz + 1);
        memset(buffer, 0, bsz + 1);
        for (src = crea->sources; src != NULL; src = src->next) {
            path = path_split(src->path);
            strcat(tmp, ".crea/obj/");
            strcat(tmp, path->filename);
            strcat(tmp, ".o ");
            free(path);
        }
        buffer[bsz + 1] = '\0';
        sprintf(cmd, "clang -o %s %s %s", crea->name, buffer, crea->cflags);
        system(cmd);
    }
}

static void
print_usage(void)
{
    LOGI("Usage: crea <task> <crea.yaml>\n\n");
    LOGI("Tasks:\n");
    LOGI("   info: Produces information about the specified crea file\n");
    LOGI("  build: Begins a build process based on the specified crea file\n");
    LOGI("\n");
}

static void
print_info(crea_t *crea)
{
    LOGI("Project name: %s\n", crea->name);
    LOGI("Project version: %s\n", crea->version);
    LOGI("Sources:\n");
    print_sources(crea->sources);
    LOGI("Headers:\n");
    print_sources(crea->headers);
    LOGI("Cflags:\n  %s\n", crea->cflags);
}

int
main(int argc, char **argv)
{
    crea_t *crea = NULL;

    if (argc != 3) {
        print_usage();
        return -1;
    }
    crea = parse_file(argv[2]);
    if (crea == NULL) {
        LOGE("Unable to parse crea file: %s\n\n", argv[2]);
        print_usage();
        return -2;
    }
    if (!strcmp(argv[1], "info")) {
        print_info(crea);
    } else if(!strcmp(argv[1], "build")) {
        run_build(crea);
    } else {
        print_usage();
    }
    crea_fini(&crea);

    return 0;
}
