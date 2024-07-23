#!/bin/bash

set -euo pipefail

cd riscv-isa-sim
mkdir obj-aflgo; mkdir obj-aflgo/temp
export SUBJECT=$PWD; export TMP_DIR=$PWD/obj-aflgo/temp
export CC=$AFLGO/instrument/aflgo-clang; export CXX=$AFLGO/instrument/aflgo-clang++ #; export AR=$AFLGO/instrument/llvm_tools/build/bin/llvm-ar
export ADDITIONAL="-targets=$TMP_DIR/BBtargets.txt -outdir=$TMP_DIR -flto -fuse-ld=gold -Wl,-plugin-opt=save-temps"
export LDFLAGS="-lpthread -flto -fuse-ld=gold -Wl,-plugin-opt=save-temps"
cp $SUBJECT/BBtargets.txt $TMP_DIR/BBtargets.txt
cd obj-aflgo; CFLAGS="$ADDITIONAL" CXXFLAGS="$ADDITIONAL" ../configure --prefix=$RISCV
make clean; make
cat $TMP_DIR/BBnames.txt | rev | cut -d: -f2- | rev | sort | uniq > $TMP_DIR/BBnames2.txt && mv $TMP_DIR/BBnames2.txt $TMP_DIR/BBnames.txt
cat $TMP_DIR/BBcalls.txt | sort | uniq > $TMP_DIR/BBcalls2.txt && mv $TMP_DIR/BBcalls2.txt $TMP_DIR/BBcalls.txt
#export LD_LIBRARY_PATH=`pwd`:$LD_LIBRARY_PATH
$AFLGO/distance/gen_distance_orig.sh $SUBJECT/obj-aflgo $TMP_DIR spike
CFLAGS="-distance=$TMP_DIR/distance.cfg.txt" CXXFLAGS="-distance=$TMP_DIR/distance.cfg.txt" ../configure --prefix=$RISCV
make clean; make
set +e
mkdir in; cp $AFLGO/RTL/riscv-tests/benchmarks/*.riscv in
$AFLGO/afl-2.57b/afl-fuzz -m none -z exp -c 45m -i in -o out ./spike @@
