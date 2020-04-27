include ../common.mk

MAIN_OBJ=do_producer.o
MAIN_SRC=do_producer.c
MAIN_EXE=do_producer

MAIN_OBJ2=do_director.o
MAIN_SRC2=do_director.c
MAIN_EXE2=do_director

all: bins

binaries:
	mkdir -p $(BDIR)

bins: binaries
	$(CC) $(CFLAGS) $(MAIN_SRC) -c $(LDFLAGS)
	$(CC) $(CFLAGS) $(MAIN_OBJ) $(ODIR)/*.o -o $(BDIR)/$(MAIN_EXE) $(LDFLAGS)

	$(CC) $(CFLAGS) $(MAIN_SRC2) -c $(LDFLAGS)
	$(CC) $(CFLAGS) $(MAIN_OBJ2) $(ODIR)/*.o -o $(BDIR)/$(MAIN_EXE2) $(LDFLAGS)

clean:
	rm -rf *.o *~ *#*
	rm -rf $(BDIR)
