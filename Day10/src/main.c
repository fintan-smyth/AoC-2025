#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>

struct machine
{
	uint64_t	lights;
	uint64_t	*buttons;
	uint64_t	n_buttons;
	bool		complete;
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

struct machine	*get_machine(char *line)
{
	struct machine	*machine = calloc(1, sizeof(*machine));
	char			*token;
	char			*endptr;
	uint64_t		buttons_size = 256;

	machine->buttons = calloc(buttons_size, sizeof(*(machine->buttons)));

	errno = 0;
	token = strtok(line, " \t\n");
	while (token != NULL)
	{
		if (*token == '[')
		{
			token++;
			for (uint64_t i = 0; token[i] != ']'; i++)
			{
				if (token[i] == '#')
					machine->lights |= (1 << i);
			}
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
		token = strtok(NULL, " \t\n");
	}
	machine->buttons = realloc(machine->buttons, machine->n_buttons * sizeof(*(machine->buttons)));
	return (machine);
}

void	print_machine(struct machine *machine)
{
	printf("[%010zb] | ", machine->lights);
	for (uint64_t i = 0; i < machine->n_buttons; i++)
		printf("(%010zb) ", machine->buttons[i]);
	printf("\n");
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
		free(cur);

	return (result);
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

	for (uint64_t i = 0; i < n_lines; i++)
	{
		machines[i] = get_machine(lines[i]);
		uint64_t presses = get_min_buttons(machines[i]);
		total += presses;
		print_machine(machines[i]);
		printf("presses: %lu\n", presses);
	}

	printf("\ntotal: %lu\n", total);
	free_ptr_array((void **)lines, n_lines);
}
