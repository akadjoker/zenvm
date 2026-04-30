CC = cc
CFLAGS = -std=c99 -Wall -Wextra -g

SRC = main.c vm.c
OBJ = $(SRC:.c=.o)

all: div_replica

div_replica: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) div_replica

.PHONY: all clean
