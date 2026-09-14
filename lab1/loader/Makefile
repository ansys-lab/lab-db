
LIBS=		-lrt -lsqlite3 -lpthread -lm
RPATH=
INC=		-I.
SUFFIX=	-sqlite
DB_NAME=	tpcc-sqlite.db

CFLAGS=		-w -O3 -g -std=gnu11 -fcommon -DDB_PATH='"$(DB_NAME)"'

.SUFFIXES:
.SUFFIXES: .o .c

all: tpcc_load$(SUFFIX)
.c.o:
	$(CC) $(CFLAGS) $(INC) $(DEFS) -c $*.c

all: tpcc_load$(SUFFIX)

tpcc_load$(SUFFIX) : load.o support.o
	$(CC) load.o support.o $(LIBS) $(RPATH) -o $@

load: tpcc_load$(SUFFIX)
	rm -f $(DB_NAME) $(DB_NAME)-wal $(DB_NAME)-shm $(DB_NAME)-journal \
		&& sqlite3 $(DB_NAME) < create_table_sqlite.sql \
		&& ./tpcc_load$(SUFFIX) -w $(or $(WAREHOUSES),1)

clean :
	rm -f *.o
