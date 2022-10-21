CFLAGS := -std=c11 -O3 -pipe -march=native -flto -fpic -pie
EXES := envsearcher

prefix := $(HOME)/.local

.PHONY: all clean install

all: $(EXES)

install: all
	cp $(EXES) $(prefix)/bin/

clean:
	rm -f $(EXES)
