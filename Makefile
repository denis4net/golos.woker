
CONTRACT := golos.workers

SRC := main.cpp
CXX := eosiocpp

all: $(CONTRACT).wast $(CONTRACT).abi

$(CONTRACT).wast: $(SRC)
	$(CXX) -o $@ $<

$(CONTRACT).abi: $(SRC)
	$(CXX) -g $@ $<

clean:
	rm -rf *.abi *.wast *.wasm
