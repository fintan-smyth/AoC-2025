#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>

enum
{
	PRE_ORD_LR,
	IN_ORD_LR,
	POST_ORD_LR,
	PRE_ORD_RL,
	IN_ORD_RL,
	POST_ORD_RL,
};

typedef struct adjlist
{
	char			*id;
	struct adjlist	*next;
}	t_list;

typedef struct tree
{
	char		*id;
	t_list		*adjlist;
	bool		visited;
	uint64_t	n_paths;
	struct tree	*left;
	struct tree	*right;
}	t_tree;

struct graph
{
	t_tree		*id_tree;
	t_list		*path;
};

void	trim_nl(char *line)
{
	int32_t	len = strlen(line);

	if (line[len - 1] == '\n')
		line[len - 1] = '\0';
}

char	**read_lines(FILE *fp, uint64_t *n_lines)
{
	uint64_t	lines_size = 256;
	char		**lines = calloc(lines_size, sizeof(char *));
	uint64_t	len = 0;
	uint64_t	i = 0;

	while (getline(&lines[i], &len, fp) != -1)
	{
		if (i == lines_size - 1)
		{
			lines_size *= 2;
			lines = realloc(lines, lines_size * sizeof(char *));
			memset(&lines[i + 1], 0, (lines_size - i - 1) * sizeof(char *));
		}
		trim_nl(lines[i]);
		i++;
	}
	free(lines[i]);
	lines = realloc(lines, i * sizeof(char *));
	*n_lines = i;
	return (lines);
}

void	free_ptr_array(void **lines, uint64_t n)
{
	for (uint64_t i = 0; i < n; i++)
		free(lines[i]);
	free(lines);
}

t_list	*new_list_node(char *id)
{
	t_list	*new = malloc(sizeof(*new));

	new->id = id;
	new->next = NULL;

	return (new);
}

void	list_add(t_list **list, t_list *new)
{
	t_list	*head;

	if (list == NULL)
	{
		printf("Error adding to adjlist\n");
		exit(1);
	}

	head = *list;
	new->next = head;
	*list = new;
}

t_list	*list_pop(t_list **list)
{
	t_list	*out;

	if (list == NULL)
	{
		printf("Error! list head is null\n");
		exit(1);
	}

	out = *list;
	if (out != NULL)
	{
		*list = out->next;
		out->next = NULL;
	}

	return (out);
}

t_tree	*new_treenode(char *id)
{
	t_tree	*new = malloc(sizeof(*new));

	new->id = id;
	new->left = NULL;
	new->right = NULL;
	new->adjlist = NULL;
	new->visited = false;
	new->n_paths = 0;

	return (new);
}

void	tree_insert(t_tree **root, t_tree *node)
{
	if (root == NULL || node == NULL)
	{
		printf("Error inserting into tree\n");
		exit(1);
	}

	if (*root == NULL)
	{
		*root = node;
		return ;
	}

	t_tree	**addr;
	t_tree	*cur = *root;

	while (cur != NULL)
	{
		int32_t retval = strcmp(cur->id, node->id);
		if (retval < 0)
			addr = &cur->right;
		else if (retval > 0)
			addr = &cur->left;
		else
		{
			printf("Error! node already exists\n");
			exit(1);
		}
		cur = *addr;
	}
	*addr = node;
}

void	print_node(t_tree *node, void *none)
{
	t_list	*cur = node->adjlist;

	printf("\e[31m%s\e[m:", node->id);
	while (cur != NULL)
	{
		printf(" %s", cur->id);
		cur = cur->next;
	}
	printf("\n");
	return ;
	(void)none;
}

void	traverse_tree(t_tree *node, int order,
			void (*f)(t_tree *, void *), void *data)
{
	if (node == NULL)
		return ;

	switch(order) {
		case (PRE_ORD_LR):
			f(node, data);
			traverse_tree(node->left, order, f, data);
			traverse_tree(node->right, order, f, data);
			break ;
		case (PRE_ORD_RL):
			f(node, data);
			traverse_tree(node->right, order, f, data);
			traverse_tree(node->left, order, f, data);
			break ;
		case (IN_ORD_LR):
			traverse_tree(node->left, order, f, data);
			f(node, data);
			traverse_tree(node->right, order, f, data);
			break ;
		case (IN_ORD_RL):
			traverse_tree(node->right, order, f, data);
			f(node, data);
			traverse_tree(node->left, order, f, data);
			break ;
		case (POST_ORD_LR):
			traverse_tree(node->left, order, f, data);
			traverse_tree(node->right, order, f, data);
			f(node, data);
			break ;
		case (POST_ORD_RL):
			traverse_tree(node->right, order, f, data);
			traverse_tree(node->left, order, f, data);
			f(node, data);
			break ;
	}
}

t_tree	*get_tree_node(t_tree *root, char *id)
{
	t_tree	*cur = root;
	int32_t	retval;

	while (cur != NULL && (retval = strcmp(cur->id, id)) != 0)
	{
		if (retval < 0)
			cur = cur->right;
		else
			cur = cur->left;
	}

	return (cur);
}

t_tree	*parse_input(char **lines, uint64_t n_lines)
{
	t_tree		*tree = NULL;
	t_tree		*node;

	for (uint64_t lineno = 0; lineno < n_lines; lineno++)
	{
		char *line = lines[lineno];

		char *id = strtok(line, ": ");
		// printf("id: %s adj: ", id);
		node = new_treenode(id);
		tree_insert(&tree, node);
		while ((id = strtok(NULL, " \n")) != NULL)
			list_add(&node->adjlist, new_list_node(id));
	}
	if (get_tree_node(tree, "out") == NULL)
		tree_insert(&tree, new_treenode("out"));
	return (tree);
}

bool	check_path_valid(t_list *visited)
{
	t_list	*cur = visited;
	bool	fft_found = false;
	bool	dac_found = false;

	while (cur != NULL)
	{
		if (strcmp(cur->id, "dac") == 0)
		{
			// printf("\e[32m%s\e[m ", cur->id);
			dac_found = true;
		}
		else if (strcmp(cur->id, "fft") == 0)
		{
			// printf("\e[31m%s\e[m ", cur->id);
			fft_found = true;
		}
		else
		{
			// printf("%s ", cur->id);
		}
		cur = cur->next;
	}
	// printf("\n");
	return (fft_found && dac_found);
}

uint64_t	count_paths(struct graph *graph, char *start, char *goal)
{
	t_tree *node = get_tree_node(graph->id_tree, start);

	if (node == NULL)
	{
		printf("Error! id not in tree\n");
		exit(1);
	}
	list_add(&graph->path, new_list_node(node->id));

	if (node->visited == true)
	{
		return (node->n_paths);
	}
	if (strcmp(node->id, goal) == 0)
	// if (node->visited == true || strcmp(node->id, goal) == 0)
	{
		free(list_pop(&graph->path));
		if (check_path_valid(graph->path))
		// if (node->valid == 1 || check_path_valid(graph->path))
		{
			// printf("\e[32;1mVALID!\e[m\n");
			return (1);
		}
		// printf("\e[31;1mINVALID!\e[m\n");
		return (1);
	}

	uint64_t n_paths = 0;
	t_list	*adjlist = node->adjlist;

	node->visited = true;
	while (adjlist != NULL)
	{
		n_paths += count_paths(graph, adjlist->id, goal);
		adjlist = adjlist->next;
	}
	free(list_pop(&graph->path));
	node->n_paths = n_paths;
	return (n_paths);
}

void	reset_node(t_tree *node, void *none)
{
	node->visited = false;
	node->n_paths = 0;
}

uint64_t	count_paths_wrapper(struct graph *graph, char *start, char *goal)
{
	uint64_t total;

	traverse_tree(graph->id_tree, PRE_ORD_LR, reset_node, NULL);
	total = count_paths(graph, start, goal);
	printf("%s->%s paths: %lu\n", start, goal, total);
	return (total);
}

uint64_t	count_valid_paths(struct graph *graph)
{
	uint64_t dac_fft = count_paths_wrapper(graph, "dac", "fft");

	if (dac_fft != 0)
	{
		uint64_t svr_dac = count_paths_wrapper(graph, "svr", "fft");
		uint64_t fft_out = count_paths_wrapper(graph, "fft", "out");

		return (svr_dac * dac_fft * fft_out);
	}

	uint64_t svr_fft = count_paths_wrapper(graph, "svr", "fft");
	uint64_t fft_dac = count_paths_wrapper(graph, "fft", "dac");
	uint64_t dac_out = count_paths_wrapper(graph, "dac", "out");

	return (svr_fft * fft_dac * dac_out);
}

int	main(int argc, char **argv)
{
	if (argc != 2)
		return (printf("No file provided\n"), 1);

	FILE *fp = fopen(argv[1], "r");
	if (fp == NULL)
		return (printf("Failed to open file\n"), 1);

	char		**lines = NULL;
	uint64_t	n_lines = 0;
	uint64_t	total = 0;

	lines = read_lines(fp, &n_lines);
	fclose(fp);

	struct graph graph = {
		.id_tree = parse_input(lines, n_lines),
		.path = NULL,
	};

	// print_n_paths(&graph, "svr", "dac");
	// print_n_paths(&graph, "svr", "fft");
	// print_n_paths(&graph, "fft", "dac");
	// print_n_paths(&graph, "dac", "fft");
	// print_n_paths(&graph, "fft", "out");
	// print_n_paths(&graph, "dac", "out");

	total = count_valid_paths(&graph);
	printf("valid paths: %lu\n", total);


	free_ptr_array((void **)lines, n_lines);
}
