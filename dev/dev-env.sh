#!/bin/sh
exec docker run -ti --rm -w /workspace -v $(pwd):/workspace distributex/eos-test bash
