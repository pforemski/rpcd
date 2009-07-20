CFLAGS =
LDFLAGS =

OBJECTS=rpcd.o read.o sh.o rep.o
TARGETS=rpcd

include rules.mk

rpcd: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o rpcd -lasn

install: install-std
