#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <math.h>

uint64_t	max_conns;

enum {
	PRE_ORD,
	IN_ORD,
	POST_ORD,
};

typedef struct vec3
{
	int64_t	x;
	int64_t	y;
	int64_t	z;
}	t_vec3;

typedef struct dist_node
{
	double				distance;
	t_vec3				*pos1;
	t_vec3				*pos2;
	struct dist_node	*left;
	struct dist_node	*right;
	struct dist_node	*next;
}	t_distnode;

struct adjlist
{
	uint64_t		vert_idx;
	struct adjlist	*next;
};

struct graph
{
	uint64_t		n_vertices;
	struct adjlist	**vertices;
	uint8_t			*visited;
};

struct graphbuilder
{
	struct graph	*graph;
	t_vec3			*vecs;
	uint64_t		n_edges;
	uint64_t		answer_p2;
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

t_vec3	*get_vecs(char **lines, uint64_t n_lines)
{
	t_vec3	*vecs = calloc(n_lines, sizeof(t_vec3));
	char	*endptr;

	errno = 0;
	for (uint64_t i = 0; i < n_lines; i++)
	{
		vecs[i].x = strtol(lines[i], &endptr, 10);
		if (errno != 0 || *endptr != ',')
			return (free(vecs), NULL);
		vecs[i].y = strtol(endptr + 1, &endptr, 10);
		if (errno != 0 || *endptr != ',')
			return (free(vecs), NULL);
		vecs[i].z = strtol(endptr + 1, &endptr, 10);
		if (errno != 0 || *endptr != '\0')
			return (free(vecs), NULL);
		// printf("(%6lu,%6lu,%6lu)\n", vecs[i].x, vecs[i].y, vecs[i].z);
	}
	return (vecs);
}

double	calculate_distance(t_vec3 *vec1, t_vec3 *vec2)
{
	double	result;

	double x_diff = (vec1->x - vec2->x);
	double y_diff = (vec1->y - vec2->y);
	double z_diff = (vec1->z - vec2->z);

	result = sqrt((x_diff * x_diff) + (y_diff * y_diff) + (z_diff * z_diff));

	return (result);
}

t_distnode	*new_distnode(t_vec3 *vec1, t_vec3 *vec2)
{
	t_distnode	*new = malloc(sizeof(*new));

	new->distance = calculate_distance(vec1, vec2);
	new->pos1 = vec1;
	new->pos2 = vec2;
	new->next = NULL;
	new->left = NULL;
	new->right = NULL;

	return (new);
}

void	dist_tree_insert(t_distnode **root, t_distnode *node)
{
	if (root == NULL || node == NULL)
	{
		printf("Error!\n");
		exit(1);
	}

	if (*root == NULL)
	{
		*root = node;
		return ;
	}

	t_distnode	**addr;
	t_distnode	*curr = *root;
	while (curr != NULL)
	{
		if (node->distance < curr->distance)
			addr = &curr->left;
		else
			addr = &curr->right;
		curr = *addr;
	}
	*addr = node;
}

void	print_distnode(t_distnode *node, void *null)
{
	t_vec3	*vec1 = node->pos1;
	t_vec3	*vec2 = node->pos2;
	printf("(%6lu,%6lu,%6lu)\t(%6lu,%6lu,%6lu)\t%f\n",
		vec1->x, vec1->y, vec1->z,
		vec2->x, vec2->y, vec2->z,
		node->distance);
	(void)null;
}

t_distnode *build_dist_tree(t_vec3 *vecs, uint64_t n_lines, uint64_t *n_links)
{
	t_distnode	*dist_tree = NULL;
	t_distnode	*new = NULL;
	uint64_t	n = 0;

	for (uint64_t i = 0; i < n_lines; i++)
	{
		for (uint64_t j = n_lines - 1; j > i; j--)
		{
			new = new_distnode(&vecs[i], &vecs[j]);
			// print_distnode(new, NULL);
			n++;
			dist_tree_insert(&dist_tree, new);
		}
	}

	*n_links = n;
	return (dist_tree);
}

void	traverse_dist_tree(t_distnode *node, int order,
			void (*f)(t_distnode *, void *), void *data)
{
	if (node == NULL)
		return ;

	if (order == PRE_ORD)
		f(node, data);
	traverse_dist_tree(node->left, order, f, data);
	if (order == IN_ORD)
		f(node, data);
	traverse_dist_tree(node->right, order, f, data);
	if (order == POST_ORD)
		f(node, data);
}

struct adjlist	*new_adjnode(uint64_t vertex)
{
	struct adjlist	*new = malloc(sizeof(struct adjlist));

	new->vert_idx = vertex;
	new->next = NULL;
	return (new);
}

void	add_to_adjlist(struct adjlist **list, struct adjlist *node)
{
	if (list == NULL)
		return ;

	node->next = *list;
	*list = node;
}

void	add_graph_edge(struct graph *graph, uint64_t src, uint64_t dst)
{
	struct adjlist	*new = new_adjnode(dst);
	new->next = graph->vertices[src];
	add_to_adjlist(&graph->vertices[src], new);

	new = new_adjnode(src);
	new->next = graph->vertices[dst];
	add_to_adjlist(&graph->vertices[dst], new);
}

void	add_edge_from_distnode(t_distnode *dist_node, void *graph_builder)
{
	struct graphbuilder *gbuilder = graph_builder;
	if (gbuilder->n_edges >= max_conns)
		return ;

	uint64_t	src = dist_node->pos1 - gbuilder->vecs;
	uint64_t	dst = dist_node->pos2 - gbuilder->vecs;

	add_graph_edge(gbuilder->graph, src, dst);
	gbuilder->n_edges++;
}

uint64_t	get_component_size(struct graph *graph, uint64_t vertex)
{
	uint64_t		size = 1;
	struct adjlist	*curr;

	graph->visited[vertex] = 1;
	curr = graph->vertices[vertex];
	while (curr != NULL)
	{
		if (graph->visited[curr->vert_idx] == 0)
			size += get_component_size(graph, curr->vert_idx);
		curr = curr->next;
	}

	return (size);
}

void	find_final_connection(t_distnode *dist_node, void *graph_builder)
{
	struct graphbuilder *gbuilder = graph_builder;
	uint64_t			src = dist_node->pos1 - gbuilder->vecs;
	uint64_t			dst = dist_node->pos2 - gbuilder->vecs;

	if (gbuilder->answer_p2 != 0)
		return ;

	add_graph_edge(gbuilder->graph, src, dst);

	memset(gbuilder->graph->visited, 0, gbuilder->graph->n_vertices);
	uint64_t size = get_component_size(gbuilder->graph, 0);
	// printf("component_size: %lu\n", size);
	if (size == gbuilder->graph->n_vertices)
	{
		printf("edges required: %lu\n", gbuilder->n_edges);
		print_distnode(dist_node, NULL);
		gbuilder->answer_p2 = dist_node->pos1->x * dist_node->pos2->x;
	}
	gbuilder->n_edges++;
}

void	print_adjlist(struct graph *graph, t_vec3 *vecs)
{
	for (uint64_t i = 0; i < graph->n_vertices; i++)
	{
		struct adjlist *curr = graph->vertices[i];
		if (curr == NULL)
			continue ;

		printf("Component %lu\n", i);
		while (curr != NULL)
		{
			printf("(%5lu,%5lu,%5lu)\n",
				vecs[curr->vert_idx].x,
				vecs[curr->vert_idx].y,
				vecs[curr->vert_idx].z
			);
			curr = curr->next;
		}
		printf("\n");
	}
}

int	main(int argc, char **argv)
{
	if (argc < 2)
		return (printf("No file provided\n"), 1);
	if (argc < 3)
		return (printf("No max connections provided\n"), 1);

	char *endptr;
	max_conns = strtol(argv[2], &endptr, 10);
	if (*endptr != '\0' || errno != 0)
		return (printf("Error parsing max connections\n"), 1);

	FILE *fp = fopen(argv[1], "r");
	if (fp == NULL)
		return (printf("Failed to open file\n"), 1);

	char		**lines = NULL;
	uint64_t	n_lines = 0;
	uint64_t	total = 0;

	lines = read_lines(fp, &n_lines);
	fclose(fp);


	t_vec3	*vecs = get_vecs(lines, n_lines);
	if (vecs == NULL)
		return (free_ptr_array((void **)lines, n_lines), printf("Error reading vecs\n"), 1);

	uint64_t	n_links = 0;
	t_distnode	*dist_tree = build_dist_tree(vecs, n_lines, &n_links);

	traverse_dist_tree(dist_tree, IN_ORD, print_distnode, NULL);
	printf("n_links: %lu\n\n", n_links);

	struct graph graph = {
		.n_vertices = n_lines,
		.vertices = calloc(n_lines, sizeof(struct adjlist *)),
		.visited = calloc(n_lines, sizeof(uint8_t)),
	};

	struct graphbuilder gbuilder = {
		.graph = &graph,
		.vecs = vecs,
		.n_edges = 0,
		.answer_p2 = 0,
	};

	// traverse_dist_tree(dist_tree, IN_ORD, add_edge_from_distnode, &gbuilder);
	// print_adjlist(&graph, vecs);
	// uint64_t	biggest[3] = {};
	//
	// for (uint64_t i = 0; i < graph.n_vertices; i++)
	// {
	// 	if (graph.visited[i] == 0)
	// 	{
	// 		uint64_t	size = get_component_size(&graph, i);
	// 		printf("circuit size: %lu\n", size);
	// 		if (size > biggest[0])
	// 		{
	// 			biggest[2] = biggest[1];
	// 			biggest[1] = biggest[0];
	// 			biggest[0] = size;
	// 		}
	// 		else if (size > biggest[1])
	// 		{
	// 			biggest[2] = biggest[1];
	// 			biggest[1] = size;
	// 		}
	// 		else if (size > biggest[2])
	// 			biggest[2] = size;
	// 	}
	// }
	//
	// printf("biggest: %lu %lu %lu\n", biggest[0], biggest[1], biggest[2]);
	// total = biggest[0] * biggest[1] * biggest[2];

	traverse_dist_tree(dist_tree, IN_ORD, find_final_connection, &gbuilder);
	total = gbuilder.answer_p2;

	printf("total: %lu\n", total);
	free_ptr_array((void **)lines, n_lines);
}
