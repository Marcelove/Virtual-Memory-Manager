CC = gcc

objects = impl3.o

vm: $(objects)
	$(CC) -pthread -o vm $(objects)


.PHONY: clean
clean:
	rm $(objects) vm correct.txt
