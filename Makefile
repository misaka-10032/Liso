################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for server and client.        #
#                                                                              #
# Author: Longqi Cai <longqic@andrew.cmu.edu>                                  #
#                                                                              #
################################################################################

CC := gcc
CFLAGS := -Wall -Werror
#CFLAGS += -g
CFLAGS += -O3
LDFLAGS := -lssl -lcrypto -lpthread

BUILD := build
SRV := lisod
CLI := client
TEST := test_driver
MAIN_SRCS := ./$(SRV).c ./$(CLI).c ./$(TEST).c
SRCS := $(shell find . -maxdepth 1 -name "*.c")
DEP_SRCS := $(filter-out $(MAIN_SRCS),$(SRCS))
DEP_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(DEP_SRCS))
SRV_OBJS := $(BUILD)/$(SRV).o $(DEP_OBJS)
CLI_OBJS := $(BUILD)/$(CLI).o $(DEP_OBJS)
TEST_OBJS := $(BUILD)/$(TEST).o $(DEP_OBJS)

RUN := run
#HOST := longqic.ddns.net
HOST := localhost
HTTP_PORT := 10032
HTTPS_PORT := 10443
CGI_SCRIPT := flaskr/flaskr.py

all: $(SRV) $(CLI) $(TEST)

%.c: %.h

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SRV): pre $(SRV_OBJS)
	@echo $(SRV) $(SRV_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SRV_OBJS)

$(CLI): pre $(CLI_OBJS)
	@echo $(CLI) $(CLI_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(CLI_OBJS)

$(TEST): pre $(TEST_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJS)

.PHONY: pre tags all clean run stop test* stress siege*

pre:
	@mkdir -p $(BUILD) $(RUN)
	@rm -rf www
	@ln -sf test/cp2/www .
	#@FLASK_APP=flaskr.flaskr flask initdb

tags:
	@ctags -R --exclude=.git                \
		--exclude=$(BUILD) --exclude=$(RUN) \
		--languages=C

run: all
	./$(SRV) $(HTTP_PORT) $(HTTPS_PORT) \
		$(RUN)/log $(RUN)/lock www      \
		$(CGI_SCRIPT) sslkey.key sslcrt.crt

stop:
	killall $(SRV)

# run it by hand
#valgrind: all
#	valgrind --leak-check=full --trace-children=yes \
#		--log-file=run/valgrind.log \
#		./$(SRV) $(HTTP_PORT) $(HTTPS_PORT) \
#		$(RUN)/log $(RUN)/lock www flaskr/flaskr.py \
#		sslkey.key sslcrt.crt

siege-static:
	siege -b -t60S http://$(HOST):$(HTTP_PORT)/index.html > $(RUN)/siege.log

siege-dynamic:
	siege -b -t60S http://$(HOST):$(HTTP_PORT)/cgi/ > $(RUN)/siege.log

echo: all
	./$(CLI) $(HOST) $(HTTP_PORT)

handin: all clean
	cd .. && tar cvf longqic.tar 15-441-project-1 && cd -

sync: all clean
	cd .. && rsync -av 15-441-project-1 cmu:~/15-641/

test0: all
	./$(TEST)

test1: all
	test/test1.sh

test2: all
	test/cp2/grader1cp2.py $(HOST) $(HTTP_PORT)

test3: all
	cd grader && ./grader1cp3.py

stress: all
	python test/stress.py $(HOST) $(HTTP_PORT) $(HTTPS_PORT) \
		$(shell pwd)/signer.crt

clean:
	@rm -rf $(BUILD) $(RUN) www $(SRV) $(CLI) $(TEST) \
		flaskr/flaskr.db tags *.dSYM lisod.lock lisod.log

