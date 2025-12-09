#include <errno.h>
#include <limits.h>
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

enum
{
	NORTH = 0,
	SOUTH,
	EAST,
	WEST,
};

typedef struct vec2
{
	int64_t	x;
	int64_t	y;
}	t_vec2;

struct edge
{
	t_vec2	p1;
	t_vec2	p2;
};

struct shape
{
	struct edge	*edges;
	t_vec2		*vertices;
	uint64_t	n_edges;
	t_vec2		min;
	t_vec2		max;
	uint8_t		*crossed;
};

typedef struct area_node
{
	int64_t				area;
	t_vec2				*p1;
	t_vec2				*p2;
	bool				valid;
	struct area_node	*left;
	struct area_node	*right;
}	t_areanode;

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

t_vec2	*get_vecs(char **lines, uint64_t n_lines)
{
	t_vec2	*vecs = calloc(n_lines, sizeof(*vecs));
	char	*endptr;

	errno = 0;
	for (uint64_t i = 0; i < n_lines; i++)
	{
		vecs[i].x = strtol(lines[i], &endptr, 10);
		if (errno != 0 || *endptr != ',')
			return (free(vecs), NULL);
		vecs[i].y = strtol(endptr + 1, &endptr, 10);
		if (errno != 0 || *endptr != '\0')
			return (free(vecs), NULL);
	}
	return (vecs);
}

t_areanode	*new_area_node(t_vec2 *p1, t_vec2 *p2)
{
	t_areanode	*new = malloc(sizeof(*new));

	new->p1 = p1;
	new->p2 = p2;
	new->area = (labs(p1->x - p2->x) + 1) * (labs(p1->y - p2->y) + 1);
	new->left = NULL;
	new->right = NULL;
	new->valid = true;

	return (new);
}

void	area_tree_insert(t_areanode **root, t_areanode *node)
{
	if (root == NULL || node == NULL)
	{
		printf("Error inserting!\n");
		exit(1);
	}

	if (*root == NULL)
	{
		*root = node;
		return ;
	}

	t_areanode	**addr;
	t_areanode	*curr = *root;
	while (curr != NULL)
	{
		if (node->area > curr->area)
			addr = &curr->right;
		else
			addr = &curr->left;
		curr = *addr;
	}
	*addr = node;
}

t_areanode	*build_area_tree(t_vec2 *vecs, uint64_t n_vecs)
{
	t_areanode	*tree = NULL;
	t_areanode	*new = NULL;

	for (uint64_t i = 0; i < n_vecs; i++)
	{
		for (uint64_t j = n_vecs - 1; j > i; j--)
		{
			new = new_area_node(&vecs[i], &vecs[j]);
			area_tree_insert(&tree, new);
		}
	}

	return (tree);
}

void	traverse_area_tree(t_areanode *node, int order,
			void (*f)(t_areanode *, void *), void *data)
{
	if (node == NULL)
		return ;

	switch(order) {
		case (PRE_ORD_LR):
			f(node, data);
			traverse_area_tree(node->left, order, f, data);
			traverse_area_tree(node->right, order, f, data);
			break ;
		case (PRE_ORD_RL):
			f(node, data);
			traverse_area_tree(node->right, order, f, data);
			traverse_area_tree(node->left, order, f, data);
			break ;
		case (IN_ORD_LR):
			traverse_area_tree(node->left, order, f, data);
			f(node, data);
			traverse_area_tree(node->right, order, f, data);
			break ;
		case (IN_ORD_RL):
			traverse_area_tree(node->right, order, f, data);
			f(node, data);
			traverse_area_tree(node->left, order, f, data);
			break ;
		case (POST_ORD_LR):
			traverse_area_tree(node->left, order, f, data);
			traverse_area_tree(node->right, order, f, data);
			f(node, data);
			break ;
		case (POST_ORD_RL):
			traverse_area_tree(node->right, order, f, data);
			traverse_area_tree(node->left, order, f, data);
			f(node, data);
			break ;
	}
}

void	print_area_node(t_areanode *node, void *none)
{
	printf("(%5ld,%5ld)\t(%5ld,%5ld)\t%ld valid: %d\n",
		node->p1->x, node->p1->y,
		node->p2->x, node->p2->y,
		node->area, node->valid
	);
	(void)none;
}

void	find_max_min(t_vec2 *vecs, struct shape *shape)
{
	for (uint64_t i = 0; i < shape->n_edges; i++)
	{
		if (vecs[i].x < shape->min.x)
			shape->min.x = vecs[i].x;
		if (vecs[i].y < shape->min.y)
			shape->min.y = vecs[i].y;
		if (vecs[i].x > shape->max.x)
			shape->max.x = vecs[i].x;
		if (vecs[i].y > shape->max.y)
			shape->max.y = vecs[i].y;
	}
}

struct edge	*get_edges(t_vec2 *vecs, uint64_t n_vecs)
{
	struct edge	*edges = calloc(n_vecs, sizeof(*edges));

	for (uint64_t i = 0; i < n_vecs - 1; i++)
	{
		edges[i].p1 = vecs[i];
		edges[i].p2 = vecs[i + 1];
	}
	edges[n_vecs - 1].p1 = vecs[n_vecs - 1];
	edges[n_vecs - 1].p2 = vecs[0];

	return (edges);
}

bool	point_on_line(t_vec2 *point, struct edge *edge)
{
	if (edge->p1.x == edge->p2.x)
	{
		if (edge->p1.y < edge->p2.y)
			return (point->x == edge->p1.x && point->y >= edge->p1.y && point->y <= edge->p2.y);
		else
			return (point->x == edge->p1.x && point->y <= edge->p1.y && point->y >= edge->p2.y);
	}
	else if (edge->p1.y == edge->p2.y)
	{
		if (edge->p1.x < edge->p2.x)
			return (point->y == edge->p1.y && point->x >= edge->p1.x && point->x <= edge->p2.x);
		else
			return (point->y == edge->p1.y && point->x <= edge->p1.x && point->x >= edge->p2.x);
	}
	else
	{
		printf("Error: edge does not form a line\n");
		exit(1);
	}
}

bool	point_crosses_border(t_vec2 *point, struct shape *shape)
{
	bool crossed = false;

	for (uint64_t i = 0; i < shape->n_edges; i++)
	{
		if (shape->crossed[i] == 0 && point_on_line(point, &shape->edges[i]))
		{
			shape->crossed[i] = 1;
			crossed = true;
		}
	}
	return (crossed);
}

void	get_area_max_min(t_areanode *node, t_vec2 *min, t_vec2 *max)
{
	if (node->p1->x < node->p2->x)
	{
		min->x = node->p1->x;
		max->x = node->p2->x;
	}
	else
	{
		min->x = node->p2->x;
		max->x = node->p1->x;
	}
	if (node->p1->y < node->p2->y)
	{
		min->y = node->p1->y;
		max->y = node->p2->y;
	}
	else
	{
		min->y = node->p2->y;
		max->y = node->p1->y;
	}
}

bool	vertex_in_area(t_vec2 *vertex, t_vec2 *min, t_vec2 *max)
{
	return (vertex->x > min->x && vertex->x < max->x
		&& vertex->y > min->y && vertex->y < max->y);
}

bool	edge_in_area(struct edge *edge, t_vec2 *min, t_vec2 *max)
{
	t_vec2	vertex;
	if (edge->p1.x == edge->p2.x)
	{
		vertex.x = edge->p1.x;
		if (edge->p1.y < edge->p2.y)
		{
			for (vertex.y = edge->p1.y; vertex.y <= edge->p2.y; vertex.y++)
			{
				if (vertex_in_area(&vertex, min, max))
					return (true);
			}
		}
		else
		{
			for (vertex.y = edge->p2.y; vertex.y <= edge->p1.y; vertex.y++)
			{
				if (vertex_in_area(&vertex, min, max))
					return (true);
			}
		}
	}
	else
	{
		vertex.y = edge->p1.y;
		if (edge->p1.x < edge->p2.x)
		{
			for (vertex.x = edge->p1.x; vertex.x <= edge->p2.x; vertex.x++)
			{
				if (vertex_in_area(&vertex, min, max))
					return (true);
			}
		}
		else
		{
			for (vertex.x = edge->p2.x; vertex.x <= edge->p1.x; vertex.x++)
			{
				if (vertex_in_area(&vertex, min, max))
					return (true);
			}
		}
	}
	return (false);
}

void	area_valid_alt(t_areanode *node, void *data)
{
	struct shape	*shape = data;

	t_vec2		min;
	t_vec2		max;

	get_area_max_min(node, &min, &max);
	for (uint64_t i = 0; i < shape->n_edges; i++)
	{
		if (edge_in_area(&shape->edges[i], &min, &max))
		{
			node->valid = false;
			// print_area_node(node, NULL);
			return ;
		}
	}
	print_area_node(node, NULL);
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

	lines = read_lines(fp, &n_lines);
	fclose(fp);

	t_vec2	*vecs = get_vecs(lines, n_lines);

	struct	shape shape = {
		.edges = get_edges(vecs, n_lines),
		.vertices = vecs,
		.n_edges = n_lines,
		.min = { INT64_MAX, INT64_MAX },
		.max = { INT64_MIN, INT64_MIN },
		.crossed = calloc(n_lines, sizeof(uint8_t))
	};

	find_max_min(vecs, &shape);
	printf("max: (%ld,%ld)\nmin: (%ld,%ld)\n",
		shape.max.x, shape.max.y,
		shape.min.x, shape.min.y
	);

	t_areanode	*tree = build_area_tree(vecs, n_lines);
	traverse_area_tree(tree, IN_ORD_RL, area_valid_alt, &shape);
	// traverse_area_tree(tree, IN_ORD_LR, print_area_node, NULL);

	free_ptr_array((void **)lines, n_lines);
	free(vecs);
}
