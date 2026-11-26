ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

ROOT := $(abspath ../..)
CNEXT := $(ROOT)/cnext$(EXE)
APP := jsonfmt$(EXE)
SRC := src/main.cn

.PHONY: all test clean

all: $(APP)

$(CNEXT):
	$(MAKE) -C $(ROOT)

$(APP): $(SRC) $(CNEXT)
	$(CNEXT) build $(SRC) -o $(APP)

test: $(APP)
	python tests/test_cli.py

clean:
	rm -f $(APP)
