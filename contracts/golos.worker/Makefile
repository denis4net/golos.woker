
CONTRACT := golos.worker

SRC := main.cpp
CXX := eosiocpp

all: $(CONTRACT).wast $(CONTRACT).abi

$(CONTRACT).wast: $(SRC)
	$(CXX) -o $@ $<

clean:
	rm -rf *.wast *.wasm
