
CONTRACT := golos.workers
CONTRACT_ACCOUNT ?= golos.workers
WALLET_ENDPOINT ?= http://localhost:8889
TEST_HASH ?= $(shell date | sha256sum | sed -re 's/([\s^]*)\s.*/\1/g')
SRC := main.cpp
CXX := eosiocpp

all: $(CONTRACT).wast $(CONTRACT).abi

$(CONTRACT).wast: $(SRC)
	$(CXX) -o $@ $<

$(CONTRACT).abi: $(SRC)
	$(CXX) -g $@ $<

deploy:
	cleos --wallet-url "$(WALLET_ENDPOINT)" set contract $(CONTRACT_ACCOUNT) $(PWD)

clean:
	rm -rf *.abi *.wast *.wasm
