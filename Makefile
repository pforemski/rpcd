# XXX: remove -lpthread in no-debugging versions
CFLAGS =
LDFLAGS = -lasn -ldl -lpthread

OBJECTS=rpcd.o read.o write.o sh.o auth.o generic.o
TARGETS=rpcd

include rules.mk

rpcd: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o rpcd

install: install-std
install-links: install-lns
