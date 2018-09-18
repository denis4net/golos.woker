#!/bin/sh
docker run -ti --rm -w /build -v $(pwd):/build eosio/eos-dev:v1.1.1 make
