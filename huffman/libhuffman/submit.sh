#!/bin/sh
export GOMP_CPU_AFFINITY=0-67



export OMP_PROC_BIND=true
#qsub -v PR_PATH=`readlink -f ./huffman`, DATA_PATH=`readlink -f ~/enwiki-20170501-page.sql`
$PR_PATH -i $DATA_PATH -t $NUM_T -p
