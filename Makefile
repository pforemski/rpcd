# XXX: remove -lpthread in no-debugging versions
CFLAGS =
LDFLAGS = -lasn -ldl -lpthread

TARGETS=librpcd.so rpcd
OBJECTS=rpcd.o generic.o sh.o
OBJECTS2=rpcd.o daemon.o read.o write.o sh.o auth.o generic.o

include rules.mk

default: all
all: $(TARGETS)

rpcd: $(OBJECTS2)
	$(CC) $(LDFLAGS) $(OBJECTS2) -o rpcd

librpcd.so: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -shared -o librpcd.so

install: install-std
install-links: install-lns
