#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>

enum {
	MODE_GET_RANGES = 0,
	MODE_CHECK_ID = 1,
};

struct range {
	uint64_t	low;
	uint64_t	high;
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

void	free_ptr_array(void **arr, uint64_t n)
{
	for (uint64_t i = 0; i < n; i++)
		free(arr[i]);
	free(arr);
}

struct range	parse_range(char *line)
{
	struct range range = {};
	char		*endptr;
	uint64_t	num1;
	uint64_t	num2;

	errno = 0;
	num1 = strtol(line, &endptr, 10);
	if (*endptr != '-' || errno != 0)
		return (errno = 1, range);
	num2 = strtol(endptr + 1, &endptr, 10);
	if (*endptr != '\0' || errno != 0)
		return (errno = 1, range);

	if (num1 < num2)
	{
		range.low = num1;
		range.high = num2;
	}
	else
	{
		range.low = num2;
		range.high = num1;
	}
	return (range);
}

uint8_t	check_fresh(char *line, struct range *ranges, uint64_t n_ranges)
{
	uint64_t	id;
	char		*endptr;

	errno = 0;
	id = strtol(line, &endptr, 10);
	if (*endptr != '\0' || errno != 0)
		return (errno = 1, 0);

	for (uint64_t i = 0; i < n_ranges; i++)
	{
		if (id >= ranges[i].low && id <= ranges[i].high)
			return (1);
	}
	return (0);
}

void	swap_ranges(struct range *ranges, uint64_t i, uint64_t j)
{
	struct range	tmp;

	tmp = ranges[i];
	ranges[i] = ranges[j];
	ranges[j] = tmp;
}

void	quicksort_ranges_low(struct range *ranges, int64_t left, int64_t right)
{
	int64_t	i;
	int64_t	pivot = (left + right) / 2;
	int64_t	last;

	if (left >= right)
		return ;

	swap_ranges(ranges, left, pivot);
	last = left;
	i = left + 1;
	while (i <= right)
	{
		if (ranges[left].low > ranges[i].low)
			swap_ranges(ranges, ++last, i);
		i++;
	}
	swap_ranges(ranges, left, last);
	quicksort_ranges_low(ranges, left, last - 1);
	quicksort_ranges_low(ranges, last + 1, right);
}

uint64_t	calculate_total_fresh(struct range *ranges, uint64_t n_ranges)
{
	uint64_t	total = 0;
	uint64_t	i = 0;

	quicksort_ranges_low(ranges, 0, n_ranges - 1);
	while (i < n_ranges - 1)
	{
		// printf("%lu : %lu\n", ranges[i].low, ranges[i].high);

		if (ranges[i].high >= ranges[i + 1].low)
		{
			if (ranges[i].high < ranges[i + 1].high)
				ranges[i].high = ranges[i + 1].high;
			memmove(&ranges[i + 1], &ranges[i + 2], (n_ranges - i - 2) * sizeof(struct range));
			n_ranges--;
		}
		else
			i++;
	}

	for (uint64_t i = 0; i < n_ranges; i++)
	{
		printf("%lu : %lu\n", ranges[i].low, ranges[i].high);
		total += ranges[i].high - ranges[i].low + 1;
	}

	return (total);
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
	uint64_t	fresh = 0;

	lines = read_lines(fp, &n_lines);
	fclose(fp);

	uint64_t		ranges_size = 256;
	struct range	*ranges = calloc(ranges_size, sizeof(struct range));
	uint64_t		n_ranges = 0;

	for (uint64_t i = 0; i < n_lines; i++)
	{
		// printf("\e[31m>\e[m %s\n", lines[i]);
		if (!isdigit(lines[i][0]))
			break ;
		if (n_ranges == ranges_size)
		{
			ranges_size *= 2;
			ranges = realloc(ranges, ranges_size * sizeof(struct range));
		}
		ranges[n_ranges++] = parse_range(lines[i]);
		if (errno != 0)
			return (printf("Error!\n"), 1);
	}

	fresh = calculate_total_fresh(ranges, n_ranges);

	// for (uint64_t i = 0; i < n_ranges; i++)
	// 	printf("low: %lu high: %lu\n", ranges[i].low, ranges[i].high);

	printf("total fresh: %lu\n", fresh);
	free_ptr_array((void **)lines, n_lines);
	free(ranges);
}
