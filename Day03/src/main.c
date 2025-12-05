#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

uint64_t	pow_int(uint64_t num ,uint64_t exp)
{
	uint64_t out = 1;

	while (exp-- > 0)
		out *= num;

	return (out);
}

uint64_t	get_joltage(char *line, int32_t n_digits)
{
	int32_t		*digits = calloc(n_digits, sizeof(uint32_t));
	int32_t		line_len = strlen(line);

	for (int32_t i = 0; i < n_digits; i++)
		digits[i] = -1;

	if (line[line_len - 1] == '\n')
	{
		line[line_len - 1] = '\0';
		line_len--;
	}

	int32_t	dig_idx = 0;
	int32_t	last_idx = -1;
	for (int32_t i = n_digits; i > 0; i--, dig_idx++)
	{
		for (int32_t j = last_idx + 1; j < line_len + 1 - i; j++)
		{
			int32_t digit = line[j] - '0';
			if (digit > digits[dig_idx])
			{
				digits[dig_idx] = digit;
				last_idx = j;
			}
		}
	}

	uint64_t	joltage = 0;

	for (int32_t i = 0; i < n_digits; i++)
	{
		joltage += digits[i] * pow_int(10, n_digits - i - 1);
	}

	printf("joltage: %12ld %s\n", joltage, line);
	return  (joltage);
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
	uint64_t		total = 0;

	while (getline(&line, &size, fp) != -1)
	{
		total += get_joltage(line, 12);
	}

	printf("\nTotal: %lu\n", total);
	free(line);
	fclose(fp);
}
