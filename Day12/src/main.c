#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>

struct shape
{
	uint8_t tiles[3][3];
	uint8_t	n_filled;
};

struct problem
{
	uint32_t	x;
	uint32_t	y;
	uint32_t	*n_shapes;
	bool		invalid;
};

struct data
{
	struct shape	*shapes;
	uint32_t		n_shapes;
	struct problem	*problems;
	uint32_t		n_problems;
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

void	parse_input(struct data *data, char **lines, uint64_t n_lines)
{
	uint64_t	i = 0;
	char		*endptr;
	int64_t		num;

	errno = 0;
	data->shapes = calloc(10, sizeof(struct shape));
	while (i < n_lines)
	{
		if (strlen(lines[i]) > 3)
			break ;
		num = strtol(lines[i], &endptr, 10);
		if (errno != 0)
		{
			printf("num parsing error!\n");
			exit(0);
		}
		if (*endptr != ':')
		{
			printf("num parsing error!\n");
			exit(0);
		}
		i++;

		for (uint64_t j = 0; j < 3; j++)
		{
			for (uint64_t k = 0; k < 3; k++)
			{
				if (lines[i + j][k] == '#')
				{
					data->shapes[data->n_shapes].tiles[j][k] = 1;
					data->shapes[data->n_shapes].n_filled++;
				}
				else
					data->shapes[data->n_shapes].tiles[j][k] = 0;
			}
		}
		printf("%u: %u\n", data->n_shapes, data->shapes[data->n_shapes].n_filled);
		data->n_shapes++;
		i += 4;
	}

	data->n_problems = n_lines - i;
	data->problems = calloc(data->n_problems, sizeof(struct problem));
	for (uint64_t j = 0; j < data->n_problems; i++, j++)
	{
		data->problems[j].n_shapes = calloc(data->n_shapes, sizeof(uint32_t));
		data->problems[j].x = strtol(lines[i], &endptr, 10);
		if (errno != 0 || *endptr != 'x')
		{
			printf("num parsing error!\n");
			exit(0);
		}
		data->problems[j].y = strtol(endptr + 1, &endptr, 10);
		if (errno != 0 || *endptr != ':')
		{
			printf("num parsing error!\n");
			exit(0);
		}
		for (uint32_t k = 0; k < data->n_shapes; k++)
		{
			data->problems[j].n_shapes[k] = strtol(endptr + 1, &endptr, 10);
			if (errno != 0 || (*endptr != ' ' && *endptr != '\0'))
			{
				printf("num parsing error!\n");
				exit(0);
			}
		}
		printf("x: %u y: %u %u %u %u %u %u %u\n",
			data->problems[j].x, data->problems[j].y,
			data->problems[j].n_shapes[0], data->problems[j].n_shapes[1],
			data->problems[j].n_shapes[2], data->problems[j].n_shapes[3],
			data->problems[j].n_shapes[4], data->problems[j].n_shapes[5]
		);
	}
}

uint64_t	remove_invalid(struct data *data)
{
	uint64_t 	total_potentially_valid = 0;
	uint64_t	total_space;
	uint64_t	tiles_used;

	for (uint32_t i = 0; i < data->n_problems; i++)
	{
		total_space = data->problems[i].x * data->problems[i].y;
		tiles_used = 0;
		for (uint32_t j = 0; j < data->n_shapes; j++)
		{
			tiles_used += data->shapes[j].n_filled * data->problems[i].n_shapes[j];
		}
		if (tiles_used > total_space)
		{
			data->problems[i].invalid = true;
			printf("problem %u not solvable!\n", i);
		}
		else
		{
			total_potentially_valid++;
			printf("problem %u potentially solvable!\n", i);
		}
	}
	return total_potentially_valid;
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

	struct data data = {};
	parse_input(&data, lines, n_lines);
	total = remove_invalid(&data);

	printf("total: %lu\n", total);
	free_ptr_array((void **)lines, n_lines);
}
