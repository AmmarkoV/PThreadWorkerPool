CC      := gcc
CCFLAGS := 
LDFLAGS := -pthread -lm

TARGETS:= example
MAINS  := $(addsuffix .o, $(TARGETS) )
OBJ    := $(MAINS)
DEPS   := pthreadWorkerPool.h

.PHONY: all clean test

all: $(TARGETS)

clean:
	rm -f $(TARGETS) $(OBJ)

$(OBJ): %.o : %.c $(DEPS)
	$(CC) -c -D_GNU_SOURCE -o $@ $< $(CCFLAGS)

$(TARGETS): % : $(filter-out $(MAINS), $(OBJ)) %.o
	$(CC) -D_GNU_SOURCE -o $@ $(LIBS) $^ $(CCFLAGS) $(LDFLAGS)

test: $(TARGETS)
	./example --threads 8 --iterations 64
