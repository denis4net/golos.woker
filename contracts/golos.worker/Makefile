
CONTRACT := golos.worker

SRC := main.cpp
CXX := eosiocpp

all: $(CONTRACT).wast

$(CONTRACT).wast: $(SRC)
	$(CXX) -o $@ $<

clean:
	rm -rf *.wast *.wasm
