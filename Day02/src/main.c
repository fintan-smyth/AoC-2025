#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

struct limits {
	uint64_t	low;
	uint64_t	high;
};

struct limits	parse_range(char *line)
{
	struct limits	lim = {};
	char			*dash = strchr(line, '-');

	errno = 0;
	if (dash == NULL)
		return (errno = 1, lim);
	*dash = '\0';

	char	*endptr;
	lim.low = strtol(line, &endptr, 10);
	if (*endptr != '\0' || errno != 0)
		return (lim.low = 0, lim);
	lim.high = strtol(dash + 1, &endptr, 10);
	if ((*endptr != '\n' && *endptr != ',') || errno != 0)
		return (lim.high = 0, lim);
	return (lim);
}

uint64_t	count_digits(uint64_t num)
{
	uint64_t	digits = 0;

	do {
		digits++;
		num /= 10;
	} while (num > 0);

	return (digits);
}

uint64_t	pow_int(uint64_t num ,uint64_t exp)
{
	uint64_t out = 1;

	while (exp-- > 0)
		out *= num;

	return (out);
}

bool	check_invalid(uint64_t id)
{
	uint64_t	high_seq;
	uint64_t	low_seq;
	uint64_t	digits = count_digits(id);
	uint64_t	div;

	if (digits % 2 != 0)
		return (false);

	div = pow_int(10, digits / 2);
	high_seq = id / div;
	low_seq = id % div;
	if (high_seq == low_seq)
		return (true);
	return (false);
}

uint64_t	total_invalid_in_range(struct limits lim)
{
	uint64_t	total = 0;
	uint64_t	id;

	id = lim.low;
	while (id <= lim.high)
	{
		if (check_invalid(id))
		{
			total += id;
			printf("id: %ld\n", id);
		}
		id++;
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

	char		*line;
	uint64_t	size;
	uint64_t	total = 0;

	while (getdelim(&line, &size, ',', fp) != -1)
	{
		// printf("%s\n", line);
		struct limits lim = parse_range(line);
		// printf("low: %ld high: %ld\n", lim.low, lim.high);
		total += total_invalid_in_range(lim);
	}

	printf("total: %ld\n", total);
	free(line);
	fclose(fp);
}
