#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *delim_chars = " \t\v\r\f\n:={},";
static char *quote_pairs = "''\"\"()";
static char *quote_chars = "'\"() \t\v\r\f\n";

int find_token (FILE *f)
{
	for (int ch = fgetc(f); ; ch = fgetc(f))
		if (ch == EOF || !isspace(ch))
			return ch;
}

int skip_quote (FILE *f, char *quote, char **capture)
{
	for (int ch = fgetc(f); ; ch = fgetc(f)) {
		if (capture) {
			**capture = ch;
			*capture += 1;
		}
		if (ch == EOF) return 1;
		if (ch == quote[1]) return 2;
		if (ch == quote[0]) skip_quote(f, quote, capture);
	}
}

char *get_token (FILE *f, int ch)
{
	char *quote, *token, *trace;
	size_t num_quotes = 0;
	size_t start = ftell(f);
	size_t len;

	for (char capture = 0; capture < 2; capture++) {
		for (; ch != EOF && !strchr(delim_chars, ch); ch = fgetc(f)) {
			quote = strchr(quote_pairs, ch);
			if (quote && quote[1]) {
				if (capture) {
					skip_quote(f, quote, &trace);
					trace--;
				} else num_quotes += skip_quote(f, quote, NULL);
			} else if (capture) {
				*trace = ch;
				trace++;
			}
		}
		if (!capture) {
			len = ftell(f) - start - num_quotes;
			if (!len) return NULL;
			token = malloc(len + 1);
			if (!token) {
				fprintf(stderr, "Error allocating memory.\n");
				return NULL;
			}
			trace = token;
			fseek(f, start - 1, SEEK_SET);
			ch = fgetc(f);
		} else *trace = 0;
	}
	if (ch != EOF) fseek(f, -1, SEEK_CUR);

	return token;
}

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
void tree_free (fig_tree_t tree)
{
	if (tree->key)
		free(tree->key);
	fig_free(tree->value);
	if (tree->left) tree_free(tree->left);
	if (tree->right) tree_free(tree->right);
	free(tree);
}

void fig_free (fig_t value)
{
	if (value.type == Str && value.str) {
		free(value.str);
	} else if (value.type == Tree && value.tree) {
		tree_free(value.tree);
	}
}

int balanced_parens (char *str)
{
	int parens = 0;
	for (; *str; str++) {
		if (*str == '(') parens++;
		if (*str == ')') parens--;
		if (parens < 0) return 0;
	}
	return !parens;
}

void fig_print (FILE *f, fig_t value);
void print_str (FILE *f, char *str)
{
	if (!str) {
		fprintf(f, "\"\"");
	} else if (strchr(str, '"')) {
		if (strchr(str, '\'')) {
			fputc('(', f);
			if (balanced_parens(str)) {
				fprintf(f, "%s", str);
			} else for (; *str; str++) {
				if (*str == '(') {
					fprintf(f, ")'('(");
				} else if (*str == ')') {
					fprintf(f, ")')'(");
				} else fputc(*str, f);
			}
			fputc(')', f);
		} else fprintf(f, "'%s'", str);
	} else if (strpbrk(str, quote_chars)) {
		fprintf(f, "\"%s\"", str);
	} else fprintf(f, "%s", str);
}

void print_tree (FILE *f, fig_tree_t node)
{
	while (node) {
		if (node->key && (node->value.type != Str ||
		                  node->key != node->value.str))
			fprintf(f, "%s = ", node->key);
		fig_print(f, node->value);
		if (node->left) {
			fprintf(f, ", ");
			print_tree(f, node->left);
		}
		node = node->right;
		if (node) fprintf(f, ", ");
	}
}

void fig_print (FILE *f, fig_t value)
{
	if (value.type == Str) {
		print_str(f, value.str);
	} else if (value.type == Tree) {
		fputc('{', f);
		print_tree(f, value.tree);
		fputc('}', f);
	}
}

fig_tree_t new_node (char *key, fig_t value)
{
	fig_tree_t tree = calloc(1, sizeof(struct fig_tree));
	if (!tree) {
		fprintf(stderr, "Error allocating memory.\n");
		return NULL;
	}
	tree->key = key;
	tree->value = value;
	return tree;
}

int convert_to_tree (fig_t *value)
{
	if (value->type == Tree) return 1;
	fig_t new_value;
	if (value->str) {
		new_value = fig_tree(new_node(value->str, fig_str(value->str)));
	} else new_value = fig_tree(NULL);
	if (value->str && !new_value.tree)
		return 0;
	*value = new_value;
}

fig_tree_t *get_node (fig_tree_t *head, char *key)
{
	int cmp;
	fig_tree_t *node = head;
	while (*node) {
		if (!(*node)->key || !key) {
			node = &(*node)->right;
		} else {
			cmp = strcmp((*node)->key, key);
			if (cmp < 0) {
				node = &(*node)->left;
			} else if (cmp > 0) {
				node = &(*node)->right;
			} else return node;
		}
	}
	return node;
}

fig_tree_t fig_touch (fig_tree_t *head, char *key)
{
	fig_tree_t *node = get_node(head, key);
	if (*node) return *node;
	fig_tree_t new = new_node(key, fig_str(key));
	if (new) *node = new;
	return new;
}

fig_t fig_define (fig_tree_t *head, char *key, fig_t value)
{
	fig_tree_t node = fig_touch(head, key);
	if (!node) return fig_str(NULL);
	if (node->value.type == Str && node->key == node->value.str)
		node->value.str = NULL;
	fig_free(node->value);
	if (!node->key && value.type == Str)
		node->key = value.str;
	node->value = value;
	return node->value;
}

fig_t parse_key (fig_tree_t *dest, char append, FILE *f, int ch);
fig_tree_t parse_tree (FILE *f)
{
	fig_tree_t tree = NULL;
	int ch = find_token(f);

	while (ch != EOF && ch != '}') {
		parse_key(&tree, 1, f, ch);
		ch = find_token(f);
	}

	ch = find_token(f);
	if (ch != EOF && ch != ',')
		fseek(f, -1, SEEK_CUR);

	return tree;
}

fig_t parse_key (fig_tree_t *dest, char append, FILE *f, int ch)
{
	if (ch == ':' || ch == '=') {
		fprintf(stderr, "Expected token before '%c'.\n", ch);
		return fig_str("");
	} else if (ch == ',') {
		if (append) {
			return fig_define(dest, NULL, fig_str(""));
		} else return fig_str("");
	} else if (ch == '{') {
		if (append) {
			return fig_define(dest, NULL, fig_tree(parse_tree(f)));
		} else return fig_tree(parse_tree(f));
	}

	fig_tree_t next;
	char *token = get_token(f, ch);
	ch = find_token(f);
	if (ch == ':') {
		next = fig_touch(dest, token);
		if (next->value.type == Str && next->value.str == next->key)
			next->value.str = NULL;
		if (!(convert_to_tree(&next->value)))
			return fig_str(NULL);
		return parse_key(&next->value.tree, append, f, find_token(f));
	} else if (ch == '=') {
		return fig_define(dest, token, parse_key(dest, 0, f, find_token(f)));
	} else {
		if (ch != EOF && ch != ',')
			fseek(f, -1, SEEK_CUR);
		if (append) {
			return fig_touch(dest, token)->value;
		} else return fig_str(token);
	}
}

fig_tree_t fig_parse (char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Error opening '%s'.\n", path);
		return NULL;
	}
	fig_tree_t tree = parse_tree(f);
	fclose(f);
	return tree;
}

fig_t fig_lookup (fig_tree_t dict, char *key)
{
	fig_tree_t node = *get_node(&dict, key);
	return node ? node->value : fig_str(NULL);
}

size_t fig_len (fig_tree_t tree)
{
	return tree ? 1 + fig_len(tree->left) + fig_len(tree->right) : 0;
}

fig_t fig_index (fig_tree_t tree, size_t index)
{
	if (!tree) return fig_str(NULL);
	if (!index) return tree->value;
	size_t left = fig_len(tree->left);
	if (index - 1 < left) return fig_index(tree->left, index - 1);
	if (index - 1 - left >= fig_len(tree->right)) return fig_str(NULL);
	return fig_index(tree->right, index - 1 - left);
}

int main (int argc, char **args)
{
	fig_tree_t tree = fig_parse(args[1]);
	fig_print(stdout, fig_tree(tree));
}
