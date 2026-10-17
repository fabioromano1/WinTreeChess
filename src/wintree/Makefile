# M1: nucleo WinTree generico + test (spec §9.1-9.3). `make test` esegue tutto (< 1 min, output riproducibile).
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -g
JAVA     ?= java
GRAPHS   ?= 10000
SEED     ?= 1
JAVADIR   = ../../tests/wintree-java

.PHONY: test clean

test: build/wintree_test build/java-p1fix.txt
	./build/wintree_test $(GRAPHS) $(SEED)
	./build/wintree_test --java build/java-p1fix.txt

build/wintree_test: test.cpp wintree.h dfpn.h graph.h
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ test.cpp

# esiti del Java con P1 corretto sugli stessi semi (source launcher di OpenJDK, nessun javac)
build/java-p1fix.txt: $(JAVADIR)/Cmp.java $(JAVADIR)/Gen.java $(JAVADIR)/Test.java $(JAVADIR)/enginep1/Engine.java
	mkdir -p build
	cd $(JAVADIR) && $(JAVA) Cmp.java $(GRAPHS) $(SEED) > $(abspath $@)

clean:
	rm -rf build
