#ifndef FIG_H
#define FIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct fig_value {
	enum { Str, Tree } type;
	union {
		char *str;
		struct fig_tree *tree;
	};
} fig_t;

struct fig_tree {
	char *key;
	fig_t value;
	struct fig_tree *left;
	struct fig_tree *right;
};
typedef struct fig_tree *fig_tree_t;

#define fig_str(ptr)  (fig_t) {.type = Str,  .str  = ptr}
#define fig_tree(ptr) (fig_t) {.type = Tree, .tree = ptr}

void fig_free (fig_t value);
void fig_print (FILE *f, fig_t value);
fig_tree_t fig_touch (fig_tree_t *head, char *key);
fig_t fig_define (fig_tree_t *head, char *key, fig_t value);
fig_t fig_append (fig_tree_t *head, fig_t value);
fig_tree_t fig_parse (char *path);
fig_t fig_lookup (char *key);

#endif
