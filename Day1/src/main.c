#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

int64_t	parse_line(char *line)
{
	char	*endptr;
	int64_t	num;
	int64_t	sign;
	char	dir = tolower(line[0]);

	if (dir == 'l')
		sign = -1;
	else if (dir == 'r')
		sign = 1;
	else
		return (errno = 1, 0);

	errno = 0;
	num = strtol(&line[1], &endptr, 10);
	if (*endptr != '\n' && *endptr != '\0')
		return (errno = 1, 0);
	if (errno != 0)
		return (errno = 1, 0);

	num *= sign;
	return (num);
}

int32_t	turn_dial(int64_t *pos, int64_t change)
{
	int64_t	start_pos = *pos;
	int32_t	zeros = 0;

	printf("Starting pos: %2ld\tchange: %ld\t", *pos, change);
	zeros += labs(change) / 100;
	change = change % 100;
	*pos += change;
	if (change > 0 && *pos >= 100)
		zeros++;
	else if (change < 0 && *pos <= 0 && start_pos != 0)
		zeros++;
	*pos = (*pos % 100 + 100) % 100;
	printf("ending pos: %ld\tzeros: %d\n", *pos, zeros);
	return (zeros);
}

int	main(int argc, char **argv)
{
	if (argc != 2)
		return (printf("No file provided\n"), 1);

	FILE *fp = fopen(argv[1], "r");
	if (fp == NULL)
		return (printf("Failed to open file\n"), 1);

	int64_t		pos = 50;
	char		*line = NULL;
	uint64_t	size;
	uint32_t	n_zeros = 0;
	int32_t		lineno = 0;
	int64_t		change;

	while (getline(&line, &size, fp) > 0)
	{
		errno = 0;
		lineno++;
		change = parse_line(line);
		n_zeros += turn_dial(&pos, change);
		if (errno != 0)
			return (printf("Error parsing line no. %d\n", lineno), 1);
	}
	printf("%u\n", n_zeros);
	free(line);
	fclose(fp);
}
