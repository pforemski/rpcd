# XXX: remove -lpthread in no-debugging versions
CFLAGS =
LDFLAGS = -lasn -ldl -lpthread

#OBJECTS=rpcd.o read.o write.o sh.o auth.o generic.o
#TARGETS=rpcd
OBJECTS=rpcd.o generic.o sh.o
TARGETS=librpcd.so

include rules.mk

default: all
all: $(TARGETS)

rpcd: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o rpcd

librpcd.so: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -shared -o librpcd.so

install: install-std
install-links: install-lns
