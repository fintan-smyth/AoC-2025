#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>

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

uint64_t	process_line(char **lines, uint64_t lineno)
{
	uint64_t	n_splits = 0;
	char		*cur_line = lines[lineno];
	char		*next_line = lines[lineno + 1];

	for (uint64_t i = 0; cur_line[i] != '\0'; i++)
	{
		if (cur_line[i] == 'S' || cur_line[i] == '|')
		{
			if (next_line[i] != '^')
				next_line[i] = '|';
			else
			{
				n_splits++;
				if (i > 0)
					next_line[i - 1] = '|';
				if (next_line[i + 1] != '\0')
					next_line[i + 1] = '|';
			}
		}
	}
	return (n_splits);
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

	for (uint64_t i = 0; i < n_lines; i++)
		printf("\e[31m>\e[m %s\n", lines[i]);
	for (uint64_t i = 0; i < n_lines - 1; i++)
		total += process_line(lines, i);
	for (uint64_t i = 0; i < n_lines; i++)
		printf("\e[31m>\e[m %s\n", lines[i]);

	printf("total: %lu\n", total);
	free_ptr_array((void **)lines, n_lines);
}
