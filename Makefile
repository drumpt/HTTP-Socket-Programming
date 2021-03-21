.PHONY: test test-all clean server-up server-down

all: client server

client: client.c
	gcc-9 -o client client.c

server: server.c
	gcc-9 -o server server.c


SERVER_PORT := 8080

TESTFILE_1 = test/file_4K.bin
TESTFILE_2 = test/file_2M.bin
TESTFILE_3 = test/file_0B.bin
TESTFILE_4 = test/file_512M.bin

TESTFILE := $(TESTFILE_1)


server_dir:
	@mkdir -p server_dir

SERVER_CMD=../server -p $(SERVER_PORT)

server-up: server server_dir
	@cd server_dir && $(SERVER_CMD) &

server-down:
	@pkill -x -f "$(SERVER_CMD)" || true

test: client server
	@make -s server-down
	@make -s server-up
	@echo Testing $(TESTFILE) ...
	@./client -P http://localhost:$(SERVER_PORT)/$(TESTFILE) <  $(TESTFILE)
	@diff -q $(TESTFILE) server_dir/$(TESTFILE) && echo "   $(TESTFILE) server OK"
	@./client -G http://localhost:$(SERVER_PORT)/$(TESTFILE) >  $(TESTFILE).download
	@diff -q $(TESTFILE) $(TESTFILE).download && echo "   $(TESTFILE) client OK"
	@make -s server-down

test-all:
	@make -s test TESTFILE=$(TESTFILE_1)
	@make -s test TESTFILE=$(TESTFILE_2)
	@make -s test TESTFILE=$(TESTFILE_3)
	@make -s test TESTFILE=$(TESTFILE_4)

clean:
	rm -f server client
	rm -f *.download
	rm -rf server_dir
