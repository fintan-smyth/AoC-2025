#include <bits/types/struct_timeval.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
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

void	process_converted_line(int64_t **arr, uint64_t lineno, uint64_t linelen)
{
	int64_t	*cur_line = arr[lineno];
	int64_t	*next_line = arr[lineno + 1];

	for (uint64_t i = 0; i < linelen; i++)
	{
		if (cur_line[i] > 0)
		{
			int64_t num = cur_line[i];
			if (next_line[i] >= 0)
				next_line[i] += num;
			else
			{
				if (i > 0)
					next_line[i - 1] += num;
				if (i + 1 < linelen)
					next_line[i + 1] += num;
			}
		}
	}
}

void	print_path(int fd, char **lines, uint64_t *pos_arr, uint64_t n_lines, uint64_t paths)
{
	for (uint64_t i = 0; i < n_lines; i++)
	{
		for (uint64_t j = 0; lines[i][j] != '\0'; j++)
		{
			if (j == pos_arr[i])
				dprintf(fd, "\e[31m|\e[m");
			else
				dprintf(fd, "%c", lines[i][j]);
		}
		dprintf(fd, "\n");
	}
	dprintf(fd, "paths: %lu\n\n", paths);
}

struct data
{
	char			**lines;
	uint64_t		n_lines;
	uint64_t		n_paths;
	int				logfd;
	struct timeval	start;
};

void	log_path(struct data *data)
{
	struct timeval time;

	gettimeofday(&time, NULL);

	uint64_t	secs = time.tv_sec - data->start.tv_sec;
	uint64_t	usecs;
	if (time.tv_usec < data->start.tv_usec)
	{
		secs--;
		usecs = time.tv_usec + (1000000 - data->start.tv_usec);
	}
	else
		usecs = time.tv_usec - data->start.tv_usec;

	dprintf(data->logfd, "%10lu.%03lu  paths: %lu\n", secs, usecs / 1000, data->n_paths);
}

void	follow_path(struct data *data, uint64_t pos, uint64_t lineno)
{
	while (lineno < data->n_lines && data->lines[lineno][pos] != '^')
		lineno++;
	if (lineno == data->n_lines)
	{
		data->n_paths++;
		log_path(data);
		return ;
	}

	if (pos > 0)
		follow_path(data, pos - 1, lineno);
	if (data->lines[lineno][pos + 1] != '\0')
		follow_path(data, pos + 1, lineno);
}

int64_t	**convert_lines(char **lines, uint64_t n_lines)
{
	int64_t	**converted = calloc(n_lines, sizeof(uint64_t *));

	for (uint64_t i = 0; i < n_lines; i++)
	{
		converted[i] = calloc(strlen(lines[i]), sizeof(uint64_t));
		for (uint64_t j = 0; lines[i][j] != 0; j++)
		{
			switch (lines[i][j]) {
				case ('.'):
					converted[i][j] = 0;
					break ;
				case ('S'):
					converted[i][j] = 1;
					break ;
				case ('^'):
					converted[i][j] = -1;
					break ;
				default:
					printf("invalid character!\n");
					exit(1);
			}
		}
	}
	return (converted);
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


	int64_t		**arr = convert_lines(lines, n_lines);
	uint64_t	linelen = strlen(lines[0]);

	for (uint64_t i = 0; i < n_lines - 1; i++)
		process_converted_line(arr, i, linelen);

	uint64_t	total_paths = 0;
	for (uint64_t i = 0; i < linelen; i++)
		total_paths += arr[n_lines - 1][i];
	printf("total: %lu\n", total_paths);

	// uint64_t	start_pos = strchr(lines[0], 'S') - lines[0];
	// int logfd = open("./log.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
	//
	// uint64_t	total = 0;
	// struct data	data = {.lines = lines, .n_lines = n_lines, .logfd = logfd, .n_paths = total};
	// gettimeofday(&data.start, NULL);
	// follow_path(&data, start_pos, 0);

	free_ptr_array((void **)lines, n_lines);
	free_ptr_array((void **)arr, n_lines);
	// close(logfd);
}
