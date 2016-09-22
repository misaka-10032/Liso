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
CFLAGS += -g

BUILD := build
SRV := lisod
CLI := client
TEST := test_driver
MAIN_SRCS := ./$(SRV).c ./$(CLI).c ./$(TEST).c
SRCS := $(shell find . -path ./test -prune -o \
	      -maxdepth 1 -type f -name "*.c" -print)
DEP_SRCS := $(filter-out $(MAIN_SRCS),$(SRCS))
DEP_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(DEP_SRCS))
SRV_OBJS := $(BUILD)/$(SRV).o $(DEP_OBJS)
CLI_OBJS := $(BUILD)/$(CLI).o $(DEP_OBJS)
TEST_OBJS := $(BUILD)/$(TEST).o $(DEP_OBJS)

RUN := run

all: $(SRV) $(CLI) $(TEST)

pre:
	@mkdir -p $(BUILD) $(RUN)
	@rm -rf www
	@ln -sf test/cp2/www .

tags:
	@ctags -R --exclude=.git --exclude=$(BUILD) --exclude=$(RUN) \
		--languages=C

$(SRV): pre $(SRV_OBJS)
	@echo $(SRV) $(SRV_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SRV_OBJS)

$(CLI): pre $(CLI_OBJS)
	@echo $(CLI) $(CLI_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLI_OBJS)

$(TEST): pre $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS)

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.h

run: all
	./$(SRV) 10032 443 $(RUN)/log $(RUN)/lock www \
		$(RUN)/cgi $(RUN)/prv $(RUN)/cert

# TODO: valgrind gets stuck
#valgrind: all
#	valgrind ./$(SRV) 10032 443 $(RUN)/log $(RUN)/lock $(RUN)/www \
#		$(RUN)/cgi $(RUN)/prv $(RUN)/cert

echo: all
	./$(CLI) localhost 10032

handin: all clean
	cd .. && tar cvf longqic.tar 15-441-project-1 && cd -

sync: all clean
	cd .. && rsync -av 15-441-project-1 cmu:~/15-641/

test0: all
	./$(TEST)

test1: all
	test/test1.sh

test2: all
	test/cp2/grader1cp2.py localhost 10032

test: test0 test1

clean:
	@rm -rf $(BUILD) $(RUN)/log www $(SRV) $(CLI) $(TEST) \
		tags *.dSYM

.PHONY: pre tags all clean run test*
