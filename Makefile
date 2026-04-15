.PHONY: all clean test deadline1 deadline2 deadline3

all: deadline3

deadline1:
	$(MAKE) -C Deadline1

deadline2:
	$(MAKE) -C Deadline2

deadline3:
	$(MAKE) -C Deadline3/upload

test:
	$(MAKE) -C Deadline3/upload test
	QUILL_WORKERS=4 Deadline3/upload/nqueens 8
	QUILL_WORKERS=4 Deadline3/upload/iterative_averaging 4096 4

clean:
	$(MAKE) -C Deadline1 clean
	$(MAKE) -C Deadline2 clean
	$(MAKE) -C Deadline3/upload clean
