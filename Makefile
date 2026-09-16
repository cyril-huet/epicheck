# Program name
NAME = epicheck

# C compiler configuration
CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -Wall -Wextra -Werror -pedantic -std=c11
LDFLAGS ?=
INCLUDES = -Iinclude
SANITIZE_FLAGS = -g -fno-omit-frame-pointer -fsanitize=address,undefined

# Installation directories
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Source files
SRC = src/main.c \
	  src/options.c \
	  src/util.c \
	  src/analyzer.c \
	  src/scanner.c \
	  src/report.c \
	  src/hook.c

OBJ = $(SRC:.c=.o)
HEADER = include/epicheck.h

# Build
all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $(NAME)

%.o: %.c $(HEADER)
	$(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Tests and formatting
test: $(NAME)
	sh tests/test.sh ./$(NAME)

format:
	clang-format -i $(SRC) $(HEADER)

check-format:
	clang-format --dry-run -Werror $(SRC) $(HEADER)

sanitize:
	$(MAKE) fclean
	$(MAKE) CFLAGS="$(CFLAGS) $(SANITIZE_FLAGS)" \
		LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined"

# Installation
install: $(NAME)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(NAME) $(DESTDIR)$(BINDIR)/$(NAME)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(NAME)

# Cleaning
clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all test format check-format sanitize install uninstall clean fclean re
