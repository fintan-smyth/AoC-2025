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

bool	check_linked(t_distnode *node1, t_distnode *node2)
{
	if (node1->pos1 == node2->pos1)
		return (true);
	if (node1->pos1 == node2->pos2)
		return (true);
	if (node1->pos2 == node2->pos1)
		return (true);
	if (node1->pos2 == node2->pos2)
		return (true);
	return (false);
}

void	insert_dist_list(t_distnode *list, t_distnode *to_insert)
{
	t_distnode	*tmp = list->next;

	list->next = to_insert;
	to_insert->next = tmp;
}

struct connections {
	t_distnode	**connections;
	uint64_t	n_conns;
};

void	add_connection(t_distnode *node, struct connections *conns)
{
	if (conns->n_conns >= max_conns)
		return ;
	t_distnode	**connections = conns->connections;
	t_distnode	*curr_connection;
	uint64_t	i;

	conns->n_conns++;
	for (i = 0; connections[i] != NULL; i++)
	{
		curr_connection = connections[i];
		while (curr_connection != NULL)
		{
			if (check_linked(curr_connection, node))
			{
				insert_dist_list(curr_connection, node);
				return ;
			}
			curr_connection = curr_connection->next;
		}
	}
	connections[i] = node;
}

t_distnode *dist_last(t_distnode *head)
{
	t_distnode	*curr = head;

	while (curr->next != NULL)
		curr = curr->next;
	return (curr);
}

bool	check_shared_connections(t_distnode *list1, t_distnode *list2)
{
	t_distnode *node1;
	t_distnode *node2;

	for (node1 = list1; node1 != NULL; node1 = node1->next)
	{
		for (node2 = list2; node2 != NULL; node2 = node2->next)
		{
			if (check_linked(node1, node2) == true)
			{
				printf("------------\nConnection found!\n");
				print_distnode(node1, NULL);
				print_distnode(node2, NULL);
				printf("------------\n");
				return (true);
			}
		}
	}
	return (false);
}

void	join_circuits(struct connections *connections)
{
	t_distnode	**conns = connections->connections;
	for (uint64_t i = 0; conns[i] != NULL; i++)
	{
		uint64_t j = i + 1;
		while (conns[j] != NULL)
		{
			if (check_shared_connections(conns[i], conns[j]) == true)
			{
				printf("Joining %lu to %lu\n\n", i, j);
				t_distnode *last = dist_last(conns[i]);
				last->next = conns[j];
				memmove(&conns[j], &conns[j + 1], (max_conns - (j + 1)) * sizeof(t_distnode *));
			}
			else
				j++;
		}
	}
}

uint64_t	get_num_conns(t_distnode *head)
{
	uint64_t	size = 0;
	t_distnode	*curr = head;

	while (curr != NULL)
	{
		size++;
		curr = curr->next;
	}
	return (size);
}

bool	vec_in_arr(t_vec3 **arr, t_vec3 *vec)
{
	for (uint64_t i = 0; arr[i] != NULL; i++)
	{
		if (arr[i]->x == vec->x && arr[i]->y == vec->y && arr[i]->z == vec->z)
		// if (arr[i] == vec)
			return (true);
	}
	return (false);
}

uint64_t	get_circuit_size(t_distnode *head, t_vec3 *vecs)
{
	uint64_t	size = 0;
	t_distnode	*curr = head;
	t_vec3		**unique = calloc(get_num_conns(head) + 10, sizeof(t_vec3 *));

	while (curr != NULL)
	{
		if (!vec_in_arr(unique, curr->pos1))
		{
			unique[size++] = curr->pos1;
			// printf("%3lu\n", curr->pos1 - vecs);
			printf("(%5lu,%5lu,%5lu)\n", curr->pos1->x, curr->pos1->y, curr->pos1->z);
		}
		if (!vec_in_arr(unique, curr->pos2))
		{
			unique[size++] = curr->pos2;
			// printf("%3lu\n", curr->pos2 - vecs);
			printf("(%5lu,%5lu,%5lu)\n", curr->pos2->x, curr->pos2->y, curr->pos2->z);
		}
		curr = curr->next;
	}
	free(unique);
	return (size);
}

t_distnode *last_node = NULL;
double		min_distance = 1000000000;
uint64_t	n_nodes = 0;

void	get_min_distance(t_distnode *node, void *null)
{
	if (n_nodes >= max_conns)
		return ;
	double distance;
	if (last_node != NULL)
	{
		distance = node->distance - last_node->distance;
		if (distance < min_distance)
			min_distance = distance;
	}
	last_node = node;
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
	traverse_dist_tree(dist_tree, IN_ORD, get_min_distance, NULL);
	// printf("min: %f\n", min_distance);
	// return 0;

	traverse_dist_tree(dist_tree, IN_ORD, print_distnode, NULL);
	printf("n_links: %lu\n\n", n_links);

	struct connections conns;

	conns.connections = calloc(max_conns, sizeof(t_distnode *));
	conns.n_conns = 0;
	traverse_dist_tree(dist_tree, IN_ORD,
		(void (*)(t_distnode *, void *))add_connection, &conns);

	// for (uint64_t i = 0; conns.connections[i] != NULL; i++)
	// {
	// 	for (t_distnode *curr = conns.connections[i]; curr != NULL; curr = curr->next)
	// 	{
	// 		print_distnode(curr, NULL);
	// 	}
	// 	printf("len: %lu\n\n", get_circuit_size(conns.connections[i]));
	// }
	join_circuits(&conns);
	uint64_t	biggest[3] = {};
	for (uint64_t i = 0; conns.connections[i] != NULL; i++)
	{
		uint64_t	len = get_circuit_size(conns.connections[i], vecs);
		printf("len: %lu\n\n", len);
		if (len > biggest[0])
		{
			biggest[2] = biggest[1];
			biggest[1] = biggest[0];
			biggest[0] = len;
		}
		else if (len > biggest[1])
		{
			biggest[2] = biggest[1];
			biggest[1] = len;
		}
		else if (len > biggest[2])
			biggest[2] = len;
	}

	printf("biggest: %lu %lu %lu\n", biggest[0], biggest[1], biggest[2]);
	total = biggest[0] * biggest[1] * biggest[2];

	printf("total: %lu\n", total);
	free_ptr_array((void **)lines, n_lines);
}
