#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>

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
};

struct machine
{
	uint64_t	lights;
	uint64_t	n_lights;
	uint64_t	*buttons;
	uint64_t	n_buttons;
	uint16_t	*joltages;
	uint64_t	min_presses;
	uint64_t	idx;
	int			logfd;
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

struct machine	*get_machine(char *line)
{
	struct machine	*machine = calloc(1, sizeof(*machine));
	char			*token;
	char			*endptr;
	uint64_t		buttons_size = 256;

	machine->buttons = calloc(buttons_size, sizeof(*(machine->buttons)));
	machine->min_presses = UINT64_MAX;

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

void	log_answer(struct machine *machine)
{
	FILE *fp = fopen("./log", "a");
	fprintf(fp, "%lu,%lu\n", machine->idx, machine->min_presses);
	fflush(fp);
	fclose(fp);
}

void	*routine(void *arg)
{
	struct machine *machine = arg;
	uint16_t	*joltage_state = calloc(machine->n_lights, sizeof(uint16_t));
	get_min_presses_joltage(machine, joltage_state, 0);

	pthread_mutex_lock(&print_lock);
	log_answer(machine);
	printf("Min found! thread \e[3%lum%lu\e[m exiting...\n", machine->idx % 7 + 1, machine->idx);
	pthread_mutex_unlock(&print_lock);
	return (NULL);
}

struct log *read_log(void)
{
	FILE 		*fp = fopen("./log", "r");
	uint64_t	size;
	char		*line = NULL;
	struct log	*logs = calloc(256, sizeof(*logs));
	uint64_t	i = 0;
	char		*endptr;

	while (getline(&line, &size, fp) != -1)
	{
		if (line == NULL)
			break ;
		logs[i].idx = strtol(line, &endptr, 10);
		logs[i].min_presses = strtol(endptr + 1, &endptr, 10);
		i++;
	}
	fclose(fp);
	return (logs);
}

bool	log_exists(uint64_t idx, struct log *logs)
{
	for (uint64_t i = 0; i < 256; i++)
	{
		if (logs[i].min_presses > 0 && idx == logs[i].idx)
			return true;
	}
	return false;
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
	pthread_t		*threads = calloc(n_lines, sizeof(pthread_t));

	struct log *logs = read_log();
	for (uint64_t i = 0; i < n_lines; i++)
	{
		uint64_t presses;
		machines[i] = get_machine(lines[i]);
		machines[i]->idx = i;
		// presses = get_min_buttons(machines[i]);
		// uint16_t	*joltage_state = calloc(machines[i]->n_lights, sizeof(uint16_t));
		// get_min_presses_joltage(machines[i], joltage_state, 0);
		// presses = machines[i]->min_presses;
		// printf("presses: %lu\n", presses);
		// total += presses;
		if (!log_exists(i, logs))
			pthread_create(&threads[i], NULL, routine, machines[i]);
	}
	for (uint64_t i = 0; i < n_lines; i++)
		pthread_join(threads[i], NULL);
	// uint16_t	*joltage_state = calloc(machines[55]->n_lights, sizeof(uint16_t));
	// get_min_presses_joltage(machines[55], joltage_state, 0);

	free_machines(machines, n_lines);
	printf("\ntotal: %lu\n", total);
	free_ptr_array((void **)lines, n_lines);
}
