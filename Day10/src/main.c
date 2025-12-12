#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/param.h>

#define N_LOGS 4096
#define N_THREADS 200

enum
{
	J_MATCH,
	J_OVER,
	J_UNDER
};

struct log
{
	uint64_t	idx;
	uint64_t	min_presses;
	bool		final;
};

typedef struct tree
{
	char		*id;
	uint32_t 	ctg;
	struct tree	*left;
	struct tree	*right;
}	t_tree;

struct equation
{
	int32_t		*coefficients;
	int32_t		**vecs;
	int32_t		*remaining_ones;
	int32_t		*limits;
	int32_t		dimension;
	uint64_t	n_vecs;
	int32_t		*result;
	uint32_t	min_presses;
	struct machine *machine;
	t_tree		*memo;
	char		vec_id_buf[256];
	uint64_t	calls;
	// int32_t		*dp_table;
};

struct machine
{
	uint64_t		lights;
	uint64_t		n_lights;
	uint64_t		*buttons;
	uint64_t		n_buttons;
	uint16_t		*joltages;
	struct equation equation;
	uint32_t		min_presses;
	uint64_t		idx;
	int				logfd;
};

typedef struct buttonqueue
{
	uint64_t			light_state;
	uint64_t			n_presses;
	struct buttonqueue	*next;
	struct buttonqueue	*prev;
}	t_queue;

void	trim_nl(char *line)
{
	int32_t	len = strlen(line);

	if (line[len - 1] == '\n')
		line[len - 1] = '\0';
}

pthread_mutex_t	print_lock;

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

int	compare_button_size(const void *n1, const void *n2)
{
	uint64_t	num1 = *(uint64_t *)n1;
	uint64_t	num2 = *(uint64_t *)n2;
	uint64_t	count_n1 = 0;
	uint64_t	count_n2 = 0;

	for (uint16_t i = 0; i < 64; i++)
	{
		if (((num1 >> i) & 1) == 1)
			count_n1++;
		if (((num2 >> i) & 1) == 1)
			count_n2++;
	}

	if (count_n1 == count_n2)
		return (0);
	if (count_n1 < count_n2)
		return (1);
	return (-1);
}

struct ranker
{
	uint64_t	lightno;
	uint8_t		shared;
};

int	cmp_shared(const void *p1, const void *p2, void *arg)
{
	const struct ranker *light1 = p1;
	const struct ranker *light2 = p2;
	struct machine		*machine = arg;

	if (light1->shared < light2->shared)
		return (-1);
	if (light1->shared > light2->shared)
		return (1);
	if (machine->joltages[light1->lightno] > machine->joltages[light2->lightno])
		return (-1);
	if (machine->joltages[light2->lightno] > machine->joltages[light1->lightno])
		return (1);
	return (0);
}

void	swap_shared(struct ranker *n_shared, uint64_t i, uint64_t j)
{
	struct ranker	tmp;

	tmp = n_shared[i];
	n_shared[i] = n_shared[j];
	n_shared[j] = tmp;
}

void	qsort_shared(struct ranker *n_shared, int64_t left, int64_t right, struct machine *machine)
{
	int64_t	i;
	int64_t	pivot = (left + right) / 2;
	int64_t	last;

	if (left >= right)
		return ;

	swap_shared(n_shared, left, pivot);
	last = left;
	i = left + 1;
	while (i <= right)
	{
		if (cmp_shared(&n_shared[left], &n_shared[i], machine) > 0)
			swap_shared(n_shared, ++last, i);
		i++;
	}
	swap_shared(n_shared, left, last);
	qsort_shared(n_shared, left, last - 1, machine);
	qsort_shared(n_shared, last + 1, right, machine);
}

void	rank_buttons(struct machine *machine)
{
	struct ranker *n_shared = calloc(machine->n_lights, sizeof(*n_shared));

	for (uint64_t i = 0; i < machine->n_lights; i++)
	{
		n_shared[i].lightno = i;
		for (uint64_t j = 0; j < machine->n_buttons; j++)
		{
			if (((machine->buttons[j] >> i) & 1) == 1)
				n_shared[i].shared++;
		}
	}
	// qsort(n_shared, machine->n_lights, sizeof(*n_shared), cmp_shared);
	qsort_shared(n_shared, 0, machine->n_lights - 1, machine);
	// for (uint64_t i = 0; i < machine->n_lights; i++)
	// 	printf("shared: %d light: %lu\n", n_shared[i].shared, n_shared[i].lightno);

	uint64_t	*buttons = calloc(machine->n_buttons, sizeof(uint64_t));
	uint64_t	idx = 0;

	for (uint64_t i = 0; i < machine->n_lights; i++)
	{
		uint64_t	lightno = n_shared[i].lightno;
		uint64_t	start_idx = idx;
		for (uint64_t j = 0; j < machine->n_buttons; j++)
		{
			if (((machine->buttons[j] >> lightno) & 1) == 1)
			{
				buttons[idx++] = machine->buttons[j];
				machine->buttons[j] = 0;
			}
		}
		if (idx - start_idx > 1)
			qsort(&buttons[start_idx], idx - start_idx, sizeof(uint64_t), compare_button_size);
	}
	free(n_shared);
	free(machine->buttons);
	machine->buttons = buttons;
}

void	print_button(uint64_t button)
{
	printf("(");
	for (uint64_t i = 0; i < 64; i++)
	{
		if ((button >> i) & 1)
			printf("%lu,", i);
	}
	printf(")\n");
}

void	print_vec(int32_t *vec, int32_t dim)
{
	printf("(%3d", vec[0]);
	for (int32_t i = 1; i < dim; i++)
		printf(",%3d", vec[i]);
	printf(")");
}

int32_t vector_magnitude(int32_t *vec, int32_t dim)
{
	int32_t sum_sq = 0;
	for (int32_t i = 0; i < dim; i++)
		sum_sq += vec[i] * vec[i];
	return (sum_sq);
}

void	calculate_max_magnitudes(struct equation *eq)
{
	int32_t cur_max = 0;

	for (int32_t i = eq->n_vecs - 1; i >= 0; i--)
	{
		int32_t mag = vector_magnitude(eq->vecs[i], eq->dimension);
		if (mag > cur_max)
			cur_max = mag;
		eq->remaining_ones[i] = cur_max;
	}
	eq->remaining_ones[eq->n_vecs] = 0;

	printf("mags: ");
	for (int32_t i = 0; i <= eq->n_vecs; i++)
		printf(" %d", eq->remaining_ones[i]);
	printf("\n");
}

void precalculate_remaining_ones(struct equation *eq)
{
	for (int i = 0; i < eq->dimension; i++)
	{
        eq->remaining_ones[i] = 0;
    }
    for (int v_idx = 0; v_idx < eq->n_vecs; v_idx++)
	{
        for (int i = 0; i < eq->dimension; i++)
		{
            if (eq->vecs[v_idx][i] == 1) {
                eq->remaining_ones[i]++;
            }
        }
    }
}

void	get_equation(struct machine *machine)
{
	struct equation *eq = &machine->equation;

	eq->memo = NULL;
	eq->n_vecs = machine->n_buttons;
	eq->dimension = machine->n_lights;
	eq->coefficients = calloc(machine->n_buttons, sizeof(*eq->coefficients));
	eq->vecs = calloc(machine->n_buttons, sizeof(*eq->vecs));
	for (uint64_t i = 0; i < eq->n_vecs; i++)
		eq->vecs[i] = calloc(eq->dimension, sizeof(int32_t));
	eq->limits = calloc(machine->n_buttons, sizeof(*eq->limits));
	eq->result = calloc(eq->dimension, sizeof(int32_t));
	eq->remaining_ones = calloc(eq->dimension, sizeof(int32_t));
	eq->min_presses = UINT32_MAX;
	eq->calls = 0;

	for (uint64_t i = 0; i < eq->n_vecs; i++)
		eq->limits[i] = INT32_MAX;
	for (uint64_t i = 0; i < machine->n_lights; i++)
	{
		for (uint64_t j = 0; j < machine->n_buttons; j++)
		{
			if ((machine->buttons[j] >> i) & 1)
			{
				eq->vecs[j][i] = 1;
				// eq->coefficients[j]++;
				if (machine->joltages[i] < eq->limits[j])
					eq->limits[j] = machine->joltages[i];
			}
		}
		eq->result[i] += machine->joltages[i];
	}

	// calculate_max_magnitudes(eq);
	precalculate_remaining_ones(eq);
	printf("   ");
	print_vec(eq->vecs[0], eq->dimension);
	printf("\e[31ma\e[m\n");
	char	c = 'b';
	for (uint64_t i = 1; i < eq->n_vecs; i++)
	{
		printf(" + ");
		print_vec(eq->vecs[i], eq->dimension);
		printf("\e[31m%c\e[m\n", c++);
	}
	printf(" = ");
	print_vec(eq->result, eq->dimension);
	printf("\n");
	eq->machine = machine;
}

struct machine	*get_machine(char *line)
{
	struct machine	*machine = calloc(1, sizeof(*machine));
	char			*token;
	char			*endptr;
	uint64_t		buttons_size = 256;

	machine->buttons = calloc(buttons_size, sizeof(*(machine->buttons)));
	machine->min_presses = UINT32_MAX;

	pthread_mutex_lock(&print_lock);
	errno = 0;
	token = strtok(line, " \t\n");
	while (token != NULL)
	{
		if (*token == '[')
		{
			token++;
			for (uint64_t i = 0; token[i] != ']'; i++)
			{
				machine->n_lights++;
				if (token[i] == '#')
					machine->lights |= (1 << i);
			}
			machine->joltages = calloc(machine->n_lights, sizeof(uint16_t));
		}
		else if (*token == '(')
		{
			if (machine->n_buttons == buttons_size)
			{
				buttons_size *= 2;
				machine->buttons = realloc(machine->buttons, buttons_size * sizeof(*(machine->buttons)));
			}
			while (*token != ')')
			{
				token++;
				uint64_t num = strtol(token, &endptr, 10);
				if (errno != 0 || (*endptr != ',' && *endptr != ')'))
				{
					printf("Error parsing button!\n");
					exit(1);
				}
				machine->buttons[machine->n_buttons] |= (1 << num);
				token = endptr;
			}
			machine->n_buttons++;
		}
		else if (*token == '{')
		{
			uint64_t	i = 0;
			while (*token != '}')
			{
				token++;
				uint64_t num = strtol(token, &endptr, 10);
				if (errno != 0 || (*endptr != ',' && *endptr != '}'))
				{
					printf("Error parsing joltage!\n");
					exit(1);
				}
				machine->joltages[i++] = (uint16_t)num;
				token = endptr;
			}
		}
		token = strtok(NULL, " \t\n");
	}
	machine->buttons = realloc(
		machine->buttons,
		machine->n_buttons * sizeof(*(machine->buttons))
	);
	// qsort(machine->buttons, machine->n_buttons, sizeof(uint64_t), compare_button_size);
	rank_buttons(machine);
	get_equation(machine);
	// for (uint64_t i = 0; i < machine->n_buttons; i++)
	// {
	// 	print_button(machine->buttons[i]);
	// }
	pthread_mutex_unlock(&print_lock);
	return (machine);
}

void	free_machines(struct machine **machines, uint64_t n_machines)
{
	for (uint64_t i = 0; i < n_machines; i++)
	{
		struct machine *machine = machines[i];
		free(machine->buttons);
		free(machine->joltages);
		free(machine);
	}
	free(machines);
}

void	print_machine(struct machine *machine)
{
	printf("[%010zb] | ", machine->lights);
	for (uint64_t i = 0; i < machine->n_buttons; i++)
		printf("(%010zb) ", machine->buttons[i]);

	printf(" {%u", machine->joltages[0]);
	for (uint64_t i = 1; i < machine->n_lights; i++)
		printf(",%u", machine->joltages[i]);
	printf("}\n");
}

t_queue	*new_queuenode(uint64_t light_state, uint64_t n_presses)
{
	t_queue	*new = malloc(sizeof(*new));

	new->light_state = light_state;
	new->n_presses = n_presses;
	new->next = NULL;
	new->prev = NULL;

	return (new);
}

void	enqueue(t_queue **queue, t_queue *node)
{
	if (queue == NULL)
	{
		printf("Queue fail!\n");
		exit(1);
	}

	if (*queue == NULL)
	{
		*queue = node;
		node->next = node;
		node->prev = node;
		return ;
	}

	t_queue	*tmp = *queue;

	node->next = tmp;
	node->prev = tmp->prev;
	tmp->prev = node;
	node->prev->next = node;
}

t_queue	*dequeue(t_queue **queue)
{
	if (queue == NULL)
	{
		printf("Dequeue fail!\n");
		exit(1);
	}

	if (*queue == NULL)
		return NULL;

	t_queue	*out = (*queue);
	if (out->next == out)
	{
		*queue = NULL;
		out->next = NULL;
		out->prev = NULL;
		return (out);
	}

	out->prev->next = out->next;
	out->next->prev = out->prev;
	*queue = out->next;
	out->next = NULL;
	out->prev = NULL;

	return (out);
}

void	print_queue(t_queue *queue)
{
	if (queue == NULL)
	{
		printf("Queue empty\n");
		return ;
	}

	t_queue	*cur = queue;
	t_queue	*head = queue;

	do {
		printf("%010zb  presses: %ld\n", cur->light_state, cur->n_presses);
		cur = cur->next;
	} while (cur != head);
}

uint64_t	get_min_buttons(struct machine *machine)
{
	t_queue		*queue = NULL;
	t_queue		*cur;
	uint64_t	result;

	enqueue(&queue, new_queuenode(0, 0));
	while (queue != NULL)
	{
		cur = dequeue(&queue);

		if (cur->light_state == machine->lights)
			break ;

		for (uint64_t i = 0; i < machine->n_buttons; i++)
		{
			uint64_t state = cur->light_state ^ machine->buttons[i];
			enqueue(&queue, new_queuenode(state, cur->n_presses + 1));
		}
	}
	result = cur->n_presses;
	free(cur);
	while ((cur = dequeue(&queue)) != NULL)
	{
		printf("freeing queuenode\n");
		free(cur);
	}

	return (result);
}

uint16_t	*press_button(uint16_t *joltage, uint64_t button, uint64_t n_lights)
{
	uint16_t	*new_state = malloc(n_lights * sizeof(uint16_t));

	memcpy(new_state, joltage, n_lights * sizeof(uint16_t));
	for (uint64_t i = 0; i < n_lights; i++)
		new_state[i] += ((button >> i) & 1);

	return (new_state);
}

int32_t	get_charge_state(struct machine *machine, uint16_t *joltage)
{
	int32_t out = J_MATCH;

	for (uint64_t i = 0; i < machine->n_lights; i++)
	{
		if (joltage[i] > machine->joltages[i])
			return (J_OVER);
		if (joltage[i] < machine->joltages[i])
			out = J_UNDER;
	}
	return (out);
}

void	log_answer(struct machine *machine, bool final)
{
	FILE *fp = fopen("./log", "a");
	if (final)
		fprintf(fp, "+%lu,%u\n", machine->idx, machine->min_presses);
	else
		fprintf(fp, "-%lu,%u\n", machine->idx, machine->min_presses);
	fflush(fp);
	fclose(fp);
}

void	get_min_presses_joltage(struct machine *machine, uint16_t *joltage_state, uint64_t n_presses)
{
	int32_t	state = get_charge_state(machine, joltage_state);

	if (state == J_OVER)
	{
		free(joltage_state);
		// printf("\e[31movercharged!\e[m n_presses: %lu\n", n_presses);
		return ;
	}
	if (state == J_MATCH)
	{
		if (n_presses < machine->min_presses)
			machine->min_presses = n_presses;
		pthread_mutex_lock(&print_lock);
		// print_machine(machine);
		printf("\e[32mmatch!\e[m n_presses: %lu idx: \e[3%lum%lu\e[m\n", n_presses, machine->idx % 7 + 1, machine->idx);
		log_answer(machine, false);
		pthread_mutex_unlock(&print_lock);
		free(joltage_state);
		return ;
	}
	if (n_presses >= machine->min_presses - 1)
		return ;

	for (uint64_t i = 0; i < machine->n_buttons; i++)
	{
		get_min_presses_joltage(
			machine,
			press_button(joltage_state, machine->buttons[i], machine->n_lights),
			n_presses + 1
		);
	}
	free(joltage_state);
}

struct log *read_log(void)
{
	FILE 		*fp = fopen("./log", "r");
	uint64_t	size;
	char		*line = NULL;
	struct log	*logs = calloc(N_LOGS, sizeof(*logs));
	uint64_t	i = 0;
	char		*endptr;

	while (getline(&line, &size, fp) != -1)
	{
		if (line == NULL)
			break ;
		if (*line == '+')
			logs[i].final = true;
		else
			logs[i].final = false;
		line++;
		logs[i].idx = strtol(line, &endptr, 10);
		logs[i].min_presses = strtol(endptr + 1, &endptr, 10);
		i++;
	}
	fclose(fp);
	return (logs);
}

bool	answer_found(struct machine *machine, struct log *logs)
{
	for (uint64_t i = 0; i < N_LOGS; i++)
	{
		if (logs[i].min_presses > 0 && machine->idx == logs[i].idx)
		{
			machine->min_presses = logs[i].min_presses;
			machine->equation.min_presses = logs[i].min_presses;
			if (logs[i].final == true)
				return true;
		}
	}
	return false;
}

void	add_vec(int32_t *result, int32_t *vec, int32_t dim)
{
	for (int32_t i = 0; i < dim; i++)
		result[i] += vec[i];
}

void	subtract_vec(int32_t *result, int32_t *vec, int32_t dim)
{
	for (int32_t i = 0; i < dim; i++)
		result[i] -= vec[i];
}

void	add_vec_scale(int32_t *result, int32_t *vec, int32_t dim, int32_t scale)
{
	for (int32_t i = 0; i < dim; i++)
		result[i] += vec[i] * scale;
}

void	subtract_vec_scale(int32_t *result, int32_t *vec, int32_t dim, int32_t scale)
{
	for (int32_t i = 0; i < dim; i++)
		result[i] -= vec[i] * scale;
}

bool	is_zero_vector(int32_t *vec, int32_t dim)
{
	for (int32_t i = 0; i < dim; i++)
	{
		if (vec[i] != 0)
			return false;
	}
	return true;
}

t_tree	*new_treenode(char *id, uint32_t ctg)
{
	t_tree	*new = malloc(sizeof(*new));

	new->id = id;
	new->left = NULL;
	new->right = NULL;
	new->ctg = ctg;

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


void	make_vector_id(char *buf, int32_t *vec, int32_t dim, int32_t cur_vec_idx)
{
	int32_t written;
	int32_t	start = 0;
	for (int32_t i = 0; i < dim; i++)
	{
		written = snprintf(&buf[start], 256 - start, "%d,", vec[i]);
		start += written;
	}
	written = snprintf(&buf[start], 256 - start, "%d,", cur_vec_idx);
}

uint32_t	get_solutions_vec(struct equation *eq, uint32_t cur_vec_idx)
{
	// if (n_presses >= eq->min_presses)
	// 	return (INT32_MAX / 2);

	make_vector_id(eq->vec_id_buf, eq->result, eq->dimension, cur_vec_idx);
	t_tree *node = get_tree_node(eq->memo, eq->vec_id_buf);
	if (node != NULL)
		return (node->ctg);
	char	*id = strdup(eq->vec_id_buf);


	if (cur_vec_idx == eq->n_vecs)
	{
		// printf("solution found\n");
		if (is_zero_vector(eq->result, eq->dimension))
		{
			// printf("solution is valid!\n");
			// if (n_presses < eq->min_presses)
			// {
			// 	eq->min_presses = n_presses;
			// 	pthread_mutex_lock(&print_lock);
			// 	printf("presses: %u id: \e[3%lum%lu\e[m\n", n_presses, eq->machine->idx % 7 + 1, eq->machine->idx);
			// 	pthread_mutex_unlock(&print_lock);
			// 	eq->machine->min_presses = n_presses;
			// 	log_answer(eq->machine, false);
			// }
			return (0);
		}
		return (UINT32_MAX / 2);
	}

	int32_t *cur_vec = eq->vecs[cur_vec_idx];
	// int32_t remaining_mag = vector_magnitude(eq->result, eq->dimension);
	// int32_t max_mag_ahead = eq->max_magnitudes[cur_vec_idx];
	//
	// int32_t lower_bound_estimate = remaining_mag / max_mag_ahead;
	// if (remaining_mag % max_mag_ahead != 0)
	// 	lower_bound_estimate++;
	//
	// if (n_presses + lower_bound_estimate >= eq->min_presses)
		// return ;

	int32_t max_uses = INT32_MAX;
	for (int32_t i = 0; i < eq->dimension; i++)
	{
		if (cur_vec[i] == 0)
			continue ;
		int32_t possible_uses = eq->result[i] / cur_vec[i];
		if (possible_uses < max_uses)
			max_uses = possible_uses;
	}

	uint32_t min_ctg = UINT32_MAX / 2;
	uint32_t next_ctg;
	for (int32_t i = max_uses; i >= 0; i--)
	{
		subtract_vec_scale(eq->result, cur_vec, eq->dimension, i);
		next_ctg = get_solutions_vec(eq, cur_vec_idx + 1);
		add_vec_scale(eq->result, cur_vec, eq->dimension, i);

		if (next_ctg < UINT32_MAX / 2)
		{
			uint32_t total_ctg = i + next_ctg;
			if (total_ctg < min_ctg)
				min_ctg = total_ctg;
		}
	}
	t_tree *new_node = new_treenode(id, min_ctg);
	tree_insert(&eq->memo, new_node);

	return (min_ctg);
}

void	get_solutions_vec_orig(struct equation *eq, uint32_t cur_vec_idx, uint32_t n_presses)
{
	// eq->calls++;
	// if (eq->min_presses < UINT32_MAX && eq->calls > 10000000000)
	// 	return ;
	if (n_presses >= eq->min_presses)
		return ;

	if (cur_vec_idx == eq->n_vecs)
	{
		// printf("solution found\n");
		if (is_zero_vector(eq->result, eq->dimension))
		{
			// printf("solution is valid!\n");
			if (n_presses < eq->min_presses)
			{
				eq->min_presses = n_presses;
				pthread_mutex_lock(&print_lock);
				printf("presses: %u id: \e[3%lum%lu\e[m\n", n_presses, eq->machine->idx % 7 + 1, eq->machine->idx);
				pthread_mutex_unlock(&print_lock);
				eq->machine->min_presses = n_presses;
				log_answer(eq->machine, false);
			}
		}
		return ;
	}

	int32_t *cur_vec = eq->vecs[cur_vec_idx];

	int32_t improved_lower_bound = 0;
    for (int32_t i = 0; i < eq->dimension; i++)
	{
        int32_t remaining_sum_dim = eq->result[i];
        int32_t ones_count_ahead = eq->remaining_ones[i];

        if (remaining_sum_dim > 0 && ones_count_ahead == 0) {
            return;
        }

        if (ones_count_ahead > 0)
		{
            int32_t estimate_dim = (remaining_sum_dim + ones_count_ahead - 1) / ones_count_ahead;
            if (estimate_dim > improved_lower_bound)
			{
                improved_lower_bound = estimate_dim; 
            }
        }
    }

    if (n_presses + improved_lower_bound >= eq->min_presses)
		return ;

	int32_t max_uses = INT32_MAX;
	for (int32_t i = 0; i < eq->dimension; i++)
	{
		if (cur_vec[i] == 0)
			continue ;
		int32_t possible_uses = eq->result[i];
		if (possible_uses < max_uses)
			max_uses = possible_uses;
		// printf("max uses: %d\n", max_uses);
		// exit(1);
	}

	for (int32_t i = max_uses; i >= 0; i--)
	{
		subtract_vec_scale(eq->result, cur_vec, eq->dimension, i);
		get_solutions_vec_orig(eq, cur_vec_idx + 1, n_presses + i);
		add_vec_scale(eq->result, cur_vec, eq->dimension, i);
	}
}

// void	get_solutions(struct equation *eq, uint32_t var_idx, uint32_t remaining_sum, uint32_t n_presses)
// {
// 	if (n_presses >= eq->min_presses)
// 		return ;
//
// 	uint32_t coeff = eq->coefficients[var_idx];
// 	if (var_idx == eq->n_vars - 1)
// 	{
// 		if (remaining_sum % coeff == 0)
// 		{
// 			eq->vars[var_idx] = remaining_sum / coeff;
// 			n_presses += eq->vars[var_idx];
// 			if (n_presses < eq->min_presses)
// 			{
// 				eq->min_presses = n_presses;
// 				printf("presses: %u\n", n_presses);
// 			}
// 		}
//
// 		return ;
// 	}
//
// 	int32_t limit = MIN(remaining_sum / coeff, eq->limits[var_idx]);
// 	for (int32_t val = limit; val >= 0; val--)
// 	{
// 		eq->vars[var_idx] = val;
// 		get_solutions(eq, var_idx + 1, remaining_sum - (val * coeff), n_presses + val);
// 	}
// }

// uint64_t solve_dp(struct equation *eq)
// {
// 	for (uint64_t i = 0; i < eq->n_vars; i++)
// 	{
// 		int32_t cur_coeff = eq->coefficients[i];
//
// 		for (int32_t s = cur_coeff; s <= eq->result; s++)
// 		{
// 			if (eq->dp_table[s - cur_coeff] != INT32_MAX)
// 			{
// 				if (eq->dp_table[s - cur_coeff] + 1 < eq->dp_table[s])
// 					eq->dp_table[s] = eq->dp_table[s - cur_coeff] + 1;
// 			}
// 		}
// 	}
// 	printf("min sum: %d\n", eq->dp_table[eq->result]);
// 	return (eq->dp_table[eq->result]);
// }

void	*routine(void *arg)
{
	struct machine *machine = arg;
	// machine->min_presses = get_solutions_vec(&machine->equation, 0);
	get_solutions_vec_orig(&machine->equation, 0, 0);
	machine->min_presses = machine->equation.min_presses;

	pthread_mutex_lock(&print_lock);
	log_answer(machine, true);
	printf("Min found (%u)! thread \e[3%lum%lu\e[m exiting...\n", machine->min_presses, machine->idx % 7 + 1, machine->idx);
	pthread_mutex_unlock(&print_lock);
	return (NULL);
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

	struct machine	**machines = calloc(n_lines, sizeof(*machines));
	pthread_t		*threads = calloc(N_THREADS, sizeof(pthread_t));

	int64_t n_threads = 0;
	struct log *logs = read_log();
	for (uint64_t i = 0; i < n_lines; i++)
	{
		uint64_t presses;
		machines[i] = get_machine(lines[i]);
		machines[i]->idx = i;
		print_machine(machines[i]);
		struct equation *eq = &machines[i]->equation;
		// total = get_solutions_vec(eq, 0);
		// printf("total %lu\n", total);
		if (!answer_found(machines[i], logs))
			pthread_create(&threads[n_threads++], NULL, routine, machines[i]);
		if (n_threads == N_THREADS)
		{
			pthread_join(threads[--n_threads], NULL);
		}
	}
	for (uint64_t i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);

	free_machines(machines, n_lines);
	printf("\ntotal: %lu\n", total);
	free_ptr_array((void **)lines, n_lines);
}
