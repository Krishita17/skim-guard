# SkimGuard — developer convenience targets.
# Author / sole contributor: Krishita Sanjay Choksi.

PYTHON ?= python3
CC     ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -O2

.PHONY: help all samples eval figures diagrams test test-c test-py fap clean

help:
	@echo "SkimGuard make targets:"
	@echo "  make samples   - regenerate synthetic sample traces (data/synthetic)"
	@echo "  make eval      - run the measurement study (figures + eval/results.json)"
	@echo "  make diagrams  - regenerate architecture + UI-mockup figures"
	@echo "  make figures   - eval + diagrams (all figures)"
	@echo "  make test      - run C and Python tests"
	@echo "  make fap       - build the Flipper .fap with ufbt"
	@echo "  make all       - samples + figures + tests"
	@echo "  make clean     - remove build artifacts"

all: samples figures test

samples:
	$(PYTHON) -m sim.generate_samples

eval:
	$(PYTHON) -m eval.run_eval --seed 7

diagrams:
	$(PYTHON) -m eval.make_diagrams

figures: eval diagrams

test: test-c test-py

test-c:
	$(CC) $(CFLAGS) -DSKIMGUARD_SIM -Isrc -o sg_test tests/test_detector.c -lm
	./sg_test

test-py:
	$(PYTHON) -m pytest tests/test_python.py -q

# Requires ufbt (pip install ufbt). See build/BUILD.md.
fap:
	ufbt

clean:
	rm -f sg_test *.o
	rm -rf .ufbt dist __pycache__ */__pycache__ .pytest_cache
