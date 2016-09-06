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
CFLAGS += -O3

BUILD := build
SRV := lisod
CLI := client
SRCS := $(shell find . -name "*.c")
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
SRV_OBJS := $(filter-out $(BUILD)/./$(CLI).o,$(OBJS))
CLI_OBJS := $(filter-out $(BUILD)/./$(SRV).o,$(OBJS))

RUN := run

all: $(SRV) $(CLI)

pre:
	@mkdir -p $(BUILD) $(RUN)

tag:
	@ctags -R --exclude=.git --exclude=$(BUILD) --exclude=$(RUN) \
		--languages=C

$(SRV): pre $(SRV_OBJS)
	@echo $(SRV) $(SRV_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SRV_OBJS)

$(CLI): pre $(CLI_OBJS)
	@echo $(CLI) $(CLI_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLI_OBJS)

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.h

run: all
	./$(SRV) 10032 443 $(RUN)/log $(RUN)/lock $(RUN)/www \
		$(RUN)/cgi $(RUN)/prv $(RUN)/cert

valgrind: all
	valgrind ./$(SRV) 10032 443 $(RUN)/log $(RUN)/lock $(RUN)/www \
		$(RUN)/cgi $(RUN)/prv $(RUN)/cert


echo: all
	./$(CLI) localhost 10032

handin: all clean
	cd .. && tar cvf longqic.tar 15-441-project-1 && cd -

sync: all clean
	cd .. && rsync -av 15-441-project-1 cmu-latedays:~/15-641/

test1: all
	test/test1.sh

clean:
	@rm -rf $(BUILD) $(RUN) $(SRV) $(CLI) tags *.dSYM

.PHONY: pre tag all clean run test*
