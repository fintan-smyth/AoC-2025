CC = gcc

CFLAGS = -Wall -Wextra #-O0

DBG_FLAGS =		-g3 \
				-fsanitize=address,undefined,bounds-strict \

SRC_DIR := ./src
BUILD_DIR:= ./build

SRC = $(SRC_DIR)/main.c \

OBJ = $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(SRC:.c=.o))

NAME = solve

all: $(NAME)

$(NAME): $(BUILD_DIR) $(OBJ)
	$(CC) $(CFLAGS) $(DBG_FLAGS) $(OBJ) -o $(NAME) -lm

$(OBJ): $(BUILD_DIR)%.o: $(SRC_DIR)%.c
	$(CC) $(CFLAGS) $(DBG_FLAGS) -c $^ -o $@

$(BUILD_DIR):
	@mkdir -p $@

clean:
	rm -rf build/

fclean: clean
	rm -rf $(NAME)

re: fclean all
.PHONY: all clean fclean re
