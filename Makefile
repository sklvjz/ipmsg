CFLAGS = -g
objects = main.o users.o utils.o send_receive.o coms.o encode.o

ipmsg: $(objects)
	gcc $(CFLAGS) -o ipmsg $(objects) -lpthread

main.o: main.c coms.h users.h ipmsg.h send_receive.h
	gcc $(CFLAGS) -c $^ 

users.o: users.c users.h coms.h
	gcc $(CFLAGS) -c $^

utils.o: utils.c coms.h
	gcc $(CFLAGS) -c $^

send_receive.o: send_receive.c send_receive.h coms.h ipmsg.h users.h
	gcc $(CFLAGS) -c $^

coms.o: coms.c ipmsg.h coms.h
	gcc $(CFLAGS) -c $^

encode.o: encode.c
	gcc $(CFLAGS) -c $^

.PHONY: clean
clean:
	rm -f ipmsg $(objects) *.gch
cgch:
	rm -f *.gch
