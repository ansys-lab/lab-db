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

struct city {
  struct juso *jusos;
  size_t num_jusos;
};

struct state {
  struct city *cities;
  size_t num_cities;
};

static char *dup_str(const char *s) {
  size_t len = strlen(s);
  char *p = malloc(len + 1);
  if (p)
    memcpy(p, s, len + 1);
  return p;
}

static void free_juso_fields(struct juso *j) {
  free(j->zip);
  free(j->state);
  free(j->city);
  free(j->street1);
  free(j->street2);
}

/* Safe on partially built input as long as unused slots are zeroed. */
void free_states(struct state *states, size_t num_states) {
  if (!states)
    return;
  for (size_t s = 0; s < num_states; s++) {
    for (size_t c = 0; c < states[s].num_cities; c++) {
      struct city *city = &states[s].cities[c];
      for (size_t j = 0; j < city->num_jusos; j++)
        free_juso_fields(&city->jusos[j]);
      free(city->jusos);
    }
    free(states[s].cities);
  }
  free(states);
}

static int cmp_state_city(const void *a, const void *b) {
  const struct juso *x = a, *y = b;
  int r = strcmp(x->state, y->state);
  if (r != 0)
    return r;
  return strcmp(x->city, y->city);
}

/* End of the run of equal states starting at i. */
static size_t state_run_end(const struct juso *e, size_t n, size_t i) {
  size_t j = i + 1;
  while (j < n && strcmp(e[j].state, e[i].state) == 0)
    j++;
  return j;
}

/* End of the run of equal cities starting at i, bounded by the state run. */
static size_t city_run_end(const struct juso *e, size_t limit, size_t i) {
  size_t j = i + 1;
  while (j < limit && strcmp(e[j].city, e[i].city) == 0)
    j++;
  return j;
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
    *count_ret = 0; /* empty file: no rows, not an error */
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
        fields[i] = ""; /* missing trailing fields */
        continue;
      }
      if (rest[0] == '"') {
        fields[i] = rest + 1;
        char *end_field = strchr(rest, '"');
        if (!end_field) {
          printf("ERROR: no trailing '\"\n");
          goto fail;
        }
        *end_field = '\0';
        char *comma = strchr(rest, ',');
        if (comma) {
          rest = comma + 1;
        } else {
          rest = NULL;
        }
      } else {
        fields[i] = rest;
        char *comma = strchr(rest, ',');
        if (comma) {
          *comma = '\0';
          rest = comma + 1;
        } else {
          rest = NULL;
        }
      }
    }

    struct juso *e = &entries[curr];
    e->zip = dup_str(fields[0]);
    e->state = dup_str(fields[1]);
    e->city = dup_str(fields[2]);
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
/* Phase 2: sort, then carve into state -> city -> juso.              */

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
    return NULL; /* no rows */
  }

  qsort(entries, n, sizeof *entries, cmp_state_city);

  /* count distinct states */
  size_t num_states = 0;
  for (size_t i = 0; i < n; i = state_run_end(entries, n, i))
    num_states++;

  /* calloc so every unbuilt slot is {NULL, 0} and free_states is safe */
  struct state *states = calloc(num_states, sizeof *states);
  if (!states) {
    for (size_t i = 0; i < n; i++)
      free_juso_fields(&entries[i]);
    free(entries);
    return NULL;
  }

  size_t moved = 0; /* strings in [0, moved) now belong to the groups */
  size_t s = 0;

  for (size_t i = 0; i < n;) {
    size_t send = state_run_end(entries, n, i);

    /* count distinct cities inside this state run */
    size_t num_cities = 0;
    for (size_t k = i; k < send; k = city_run_end(entries, send, k))
      num_cities++;

    states[s].cities = calloc(num_cities, sizeof *states[s].cities);
    if (!states[s].cities)
      goto fail;
    /* num_cities stays 0 until each city's jusos are actually in place */

    size_t c = 0;
    for (size_t k = i; k < send;) {
      size_t cend = city_run_end(entries, send, k);
      size_t run = cend - k;

      struct city *city = &states[s].cities[c];
      city->jusos = malloc(run * sizeof *city->jusos);
      if (!city->jusos)
        goto fail;

      /* shallow copy: the char* fields move, they are not duplicated */
      memcpy(city->jusos, &entries[k], run * sizeof *entries);
      city->num_jusos = run;
      states[s].num_cities = c + 1;
      moved = cend;

      c++;
      k = cend;
    }

    s++;
    i = send;
  }

  free(entries); /* structs were copied out; strings belong to the groups */
  *num_states_ret = num_states;
  return states;

fail:
  free_states(states, num_states);
  for (size_t k = moved; k < n; k++)
    free_juso_fields(&entries[k]);
  free(entries);
  return NULL;
}