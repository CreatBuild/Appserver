local_dir := $(shell pwd)

include_dir := -I$(local_dir)
include_dir += -I$(local_dir)/zdb/
objects = main.o DatabaseProc.o MsgProc.o Socket.o log.o SessionMng.o


Appserver : $(objects)
	cc -o Appserver $(include_dir) $(objects) -lzdb -lmysqlclient -lpthread -lrt
	
 

main.o : main.c DatabaseProc.h MsgProc.h Socket.h erron.h
	cc -c main.c
DatabaseProc.o : DatabaseProc.c erron.h log.h
	cc -c DatabaseProc.c $(include_dir)
MsgProc.o : MsgProc.c MsgProc.h
	cc -c MsgProc.c $(include_dir)
Socket.o : Socket.c MsgProc.h erron.h
	cc -c Socket.c
log.o : log.c log.h
	cc -c log.c
SessionMng.o : SessionMng.c SessionMng.h log.h
	cc -c SessionMng.c

clean :
	rm Appserver $(objects)
