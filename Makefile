CFLAGS =
LDFLAGS = -lasn -ldl

OBJECTS=rpcd.o read.o write.o sh.o
TARGETS=rpcd

include rules.mk

rpcd: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o rpcd

install: install-std
