proj2: proj2.c
	gcc -pthread -lpthread -lrt -std=gnu99 -Wall -Wextra -Werror -pedantic -o proj2 proj2.c

debug: proj2.c
	gcc -pthread -lpthread -lrt -std=gnu99 -Wall -Wextra -pedantic -o proj2 -g proj2.c

clean: proj2
	rm proj2