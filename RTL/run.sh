#!/bin/bash

# Need $RISCV, directory of riscv-gnu-tool installed

pushd riscv-tests > /dev/null
	autoconf
	./configure --prefix=`pwd`
	make
popd > /dev/null

pushd riscv-fesvr > /dev/null
	./configure --prefix=$RISCV
	make
	sudo make install
popd > /dev/null
