#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

void	trim_nl(char *line)
{
	int32_t	len = strlen(line);

	if (line[len - 1] == '\n')
		line[len- 1 ] = '\0';
}

void	remove_accessible(char **lines, uint64_t len, uint64_t n_lines)
{
	for (uint64_t i = 0; i < n_lines; i++)
	{
		for (uint64_t j = 0; j < len; j++)
		{
			if (lines[i][j] == 'x')
				lines[i][j] = '.';
		}
	}
}

uint64_t	count_accessible(char **lines, uint64_t n_lines)
{
	uint64_t	len = strlen(lines[0]);
	uint64_t	accessible = 0;
	int32_t		neighbours;
	uint64_t	max_y = n_lines - 1;
	uint64_t	max_x = len - 1;

	for (uint64_t i = 0; i < n_lines; i++)
	{
		// printf("<%s>\n", lines[i]);
		for (uint64_t j = 0; j < len; j++)
		{
			if (lines[i][j] != '@')
				continue ;
			neighbours = 0;
			if (j > 0 && (lines[i][j - 1] == '@' || lines[i][j - 1] == 'x'))
				neighbours++;
			if (i > 0 && (lines[i - 1][j] == '@' || lines[i - 1][j] == 'x'))
				neighbours++;
			if (j > 0 && i < max_y && (lines[i + 1][j - 1] == '@' || lines[i + 1][j - 1] == 'x'))
				neighbours++;
			if (i > 0 && j < max_x && (lines[i - 1][j + 1] == '@' || lines[i - 1][j + 1] == 'x'))
				neighbours++;
			if (i < max_y && j < max_x && (lines[i + 1][j + 1] == '@' || lines[i + 1][j + 1] == 'x'))
				neighbours++;
			if (j < max_x && (lines[i][j + 1] == '@' || lines[i][j + 1] == 'x'))
				neighbours++;
			if (i < max_y && (lines[i + 1][j] == '@' || lines[i + 1][j] == 'x'))
				neighbours++;
			if (i > 0 && j > 0 && (lines[i - 1][j - 1] == '@' || lines[i - 1][j - 1] == 'x'))
				neighbours++;
			if (neighbours < 4)
			{
				printf("%s  x: %lu y: %lu\n", lines[i], j, i);
				accessible++;
				lines[i][j] = 'x';
			}
		}
	}
	remove_accessible(lines, len, n_lines);
	return accessible;
}

int	main(int argc, char **argv)
{
	if (argc != 2)
		return (printf("No file provided\n"), 1);

	FILE *fp = fopen(argv[1], "r");
	if (fp == NULL)
		return (printf("Failed to open file\n"), 1);

	uint64_t	lines_size = 256;
	char		**lines = calloc(lines_size, sizeof(char *));
	uint64_t	size;
	uint64_t	n_lines = 0;

	while (getline(&lines[n_lines], &size, fp) != -1)
	{
		if (n_lines == lines_size - 1)
		{
			lines_size *= 2;
			lines = realloc(lines, lines_size * sizeof(char *));
		}
		trim_nl(lines[n_lines]);
		n_lines++;
	}
	printf("n_lines: %lu\n", n_lines);

	// for (uint64_t i = 0; i < n_lines; i++)
	// 	printf("%s\n", lines[i]);

	uint64_t	total = 0;
	uint64_t	accessible = count_accessible(lines, n_lines);
	while (accessible > 0)
	{
		total += accessible;
		accessible = count_accessible(lines, n_lines);
	}

	printf("total: %lu\n", total);

	for (uint64_t i = 0; i <= n_lines; i++)
		free(lines[i]);
	free(lines);
	fclose(fp);
}
