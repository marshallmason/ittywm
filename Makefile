ittywm: ittywm.c
	$(CC) -Wall -Werror -pedantic -o $@ $< -lxcb

clean:
	rm -f ittywm

.PHONY: clean
