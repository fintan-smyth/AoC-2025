#include <ctype.h>
#include <errno.h>
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

void	align_num(char **num, char *line, char *op, char *op_line)
{
	uint64_t	distance = op - op_line;
	*num = line + distance;
}

uint64_t	do_sum(char ***num_strs, uint64_t op_num, char op, uint64_t max_digits)
{
	uint64_t	n_nums = 0;
	uint64_t	result;

	for (uint64_t i = 0; i < max_digits; i++)
	{
		uint64_t len = strlen(num_strs[i][op_num]);
		if (len  > n_nums)
			n_nums = len;
	}

	for (uint64_t i = 0; i < n_nums; i++)
	{
		uint64_t	num = 0;
		for (uint64_t j = 0; j < max_digits; j++)
		{
			char digit = num_strs[j][op_num][i];
			if (isdigit(digit))
			{
				num *= 10;
				num += digit - '0';
			}
		}
		if (i == 0)
			result = num;
		else if (op == '+')
			result += num;
		else if (op == '*')
			result *= num;
	}
	printf("result: %lu\n", result);
	return (result);
}

void	free_ptr_array(void **lines, uint64_t n)
{
	for (uint64_t i = 0; i < n; i++)
		free(lines[i]);
	free(lines);
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

	// for (uint64_t i = 0; i < n_lines; i++)
	// {
	// 	printf("\e[31m>\e[m %s\n", lines[i]);
	// }
	uint64_t	opbuf_size = 256;
	char		**ops = calloc(opbuf_size, sizeof(char *));
	uint64_t	n_ops = 0;
	char		*token = strtok(lines[n_lines - 1], " \t\n");
	while (token != NULL)
	{
		if (n_ops == opbuf_size)
		{
			opbuf_size *= 2;
			ops = realloc(ops, opbuf_size * sizeof(char *));
		}
		ops[n_ops++] = token;
		// printf("<%s>\n", token);
		token = strtok(NULL, " \t\n");
	}

	char		***num_strs = calloc(n_lines - 1, sizeof(char **));

	for (uint64_t i = 0; i < n_lines - 1; i++)
	{
		uint64_t	j = 0;
		num_strs[i] = calloc(n_ops, sizeof(char *));

		token = strtok(lines[i], " \t\n");
		while (token != NULL && j < n_ops)
		{
			align_num(&token, lines[i], ops[j], lines[n_lines - 1]);
			if (j < 8)
				printf("%s\t", token);
			num_strs[i][j++] = token;
			token = strtok(NULL, " \t\n");
		}
		printf("\n");
	}

	uint64_t	total = 0;
	for (uint64_t i = 0; i < n_ops; i++)
	{
		total += do_sum(num_strs, i, *(ops[i]), n_lines - 1);
	}

	printf("Total: %lu\n", total);
	free_ptr_array((void **)lines, n_lines);
	free_ptr_array((void **)num_strs, n_lines - 1);
	free(ops);
}
