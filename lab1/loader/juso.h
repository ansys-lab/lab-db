#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct juso {
  char *zip;
  char *state;
  char *city;
  char *street1;
  char *street2;
};


struct state {
    struct juso* jusos;
    size_t num_jusos;
};
static char *dup_str(const char *s) {
    size_t len = strlen(s);
    char *p = malloc(len + 1);
    if (p)
        memcpy(p, s, len + 1);
    if(!p)
        printf("####\n");
    return p;
}

static void free_juso_fields(struct juso *j) {
    free(j->zip);
    free(j->state);
    free(j->city);
    free(j->street1);
    free(j->street2);
}

void free_states(struct state *states, size_t num_states) {
    for (size_t i = 0; i < num_states; i++) {
        for (size_t j = 0; j < states[i].num_jusos; j++)
            free_juso_fields(&states[i].jusos[j]);
        free(states[i].jusos);
    }
    free(states);
}

static int cmp_by_state(const void *a, const void *b) {
    const struct juso *x = a, *y = b;
    return strcmp(x->state, y->state);
}

/* ------------------------------------------------------------------ */
/* Phase 1: read every row into one flat array.                       */

static struct juso *read_all_jusos(FILE *file, size_t *count_ret) {
    size_t capacity = 1024;
    size_t curr = 0;
    char line[1024];

    struct juso *entries = malloc(capacity * sizeof *entries);
    if (!entries)
        return NULL;

    /* skip header line */
    if (!fgets(line, sizeof line, file)) {
        *count_ret = 0;          /* empty file: no rows, not an error */
        return entries;
    }

    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0')
            continue;

        if (curr == capacity) {
            size_t new_cap = capacity * 2;
            struct juso *tmp = realloc(entries, new_cap * sizeof *entries);
            if (!tmp)
                goto fail;
            entries = tmp;
            capacity = new_cap;
        }

        /* split into exactly 5 comma-separated fields */
        char *fields[5];
        char *rest = line;
        for (int i = 0; i < 5; i++) {
            if (rest == NULL) {
                fields[i] = "";          /* missing trailing fields */
                continue;
            }
            fields[i] = rest;
            char *comma = strchr(rest, ',');
            if (comma) {
                *comma = '\0';
                rest = comma + 1;
            } else {
                rest = NULL;
            }
        }

        struct juso *e = &entries[curr];
        e->zip     = dup_str(fields[0]);
        e->state   = dup_str(fields[1]);
        e->city    = dup_str(fields[2]);
        e->street1 = dup_str(fields[3]);
        e->street2 = dup_str(fields[4]);

        if (!e->zip || !e->state || !e->city || !e->street1 || !e->street2) {
            free_juso_fields(e);
            goto fail;
        }
        curr++;
    }

    *count_ret = curr;
    return entries;

fail:
    for (size_t i = 0; i < curr; i++)
        free_juso_fields(&entries[i]);
    free(entries);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Phase 2: sort, then carve the flat array into per-state groups.    */

struct state *load_juso(size_t *num_states_ret) {
    *num_states_ret = 0;

    FILE *file = fopen("juso.csv", "r");
    if (!file) {
        perror("failed to open juso.csv");
        return NULL;
    }

    size_t n = 0;
    struct juso *entries = read_all_jusos(file, &n);
    fclose(file);
    if (!entries)
        return NULL;

    if (n == 0) {
        free(entries);
        return NULL;               /* no rows */
    }

    qsort(entries, n, sizeof *entries, cmp_by_state);

    /* count distinct states */
    size_t num_states = 0;
    for (size_t i = 0; i < n; ) {
        size_t j = i + 1;
        while (j < n && strcmp(entries[j].state, entries[i].state) == 0)
            j++;
        num_states++;
        i = j;
    }

    struct state *states = malloc(num_states * sizeof *states);
    if (!states) {
        for (size_t i = 0; i < n; i++)
            free_juso_fields(&entries[i]);
        free(entries);
        return NULL;
    }

    size_t s = 0;
    for (size_t i = 0; i < n; ) {
        size_t j = i + 1;
        while (j < n && strcmp(entries[j].state, entries[i].state) == 0)
            j++;

        size_t run = j - i;
        states[s].jusos = malloc(run * sizeof *states[s].jusos);
        if (!states[s].jusos) {
            /* built groups own their copies; [i, n) are still owned by entries */
            free_states(states, s);
            for (size_t k = i; k < n; k++)
                free_juso_fields(&entries[k]);
            free(entries);
            return NULL;
        }

        /* shallow copy: the char* fields move, they are not duplicated */
        memcpy(states[s].jusos, &entries[i], run * sizeof *entries);
        states[s].num_jusos = run;
        s++;
        i = j;
    }

    free(entries);   /* structs were copied out; strings now belong to groups */
    *num_states_ret = num_states;
    return states;
}