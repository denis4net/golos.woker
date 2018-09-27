
CONTRACT := golos.token

SRC := golos.token.cpp
CXX := eosiocpp

all: $(CONTRACT).wast $(CONTRACT).abi

$(CONTRACT).wast: $(SRC)
	$(CXX) -o $@ $<

clean:
	rm -rf *.wast *.wasm
