run:
	gcc -Wall -o ludo ludo.c
	gcc -Wall -o board board.c
	gcc -Wall -o players players.c
	
clean:
	rm -f ludo board players
	-ipcrm -a 2>/dev/null
