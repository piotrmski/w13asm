appname := w16asm
CFLAGS  := -std=c23

srcfiles := $(shell find . -name "*.c")
objects  := $(patsubst %.c, %.o, $(srcfiles))

all: $(appname)

$(appname): $(objects)
	$(CC) $(CFLAGS) -o $(appname) $(objects)

depend: .depend

.depend: $(srcfiles)
	rm -f ./.depend
	$(CC) -MM $^>>./.depend;

clean:
	rm -f $(objects)

dist-clean: clean
	rm -f *~ .depend

include .depend