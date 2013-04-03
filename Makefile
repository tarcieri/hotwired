CC=./compile
OBJS=Account.o AccountManager.o Collection.o Config.o ConnectionManager.o HashTable.o IDM.o MQueue.o Multiplexer.o Permissions.o RCL.o Stack.o HThread.o ThreadManager.o Transaction.o TransferManager.o connection_handler.o fileops.o helper_thread.o listener.o log.o main.o output.o password.o socketops.o tracker.o transaction_factories.o transaction_handler.o transaction_replies.o transfer_handler.o util.o xmalloc.o

.c.o:
	$(CC) -c $<

all: scripts machdep.h Multiplexer.c hotwired

scripts: compile link

compile:
	./make-compile.sh

link:
	./make-link.sh

machdep.h:
	./machdep.h.sh > machdep.h

Multiplexer.c:
	./Multiplexer.c.sh

hotwired: $(OBJS)
	./link -o hotwired $(OBJS)
	strip hotwired

clean:
	rm -f hotwired Multiplexer.c machdep.h compile link *.o 
