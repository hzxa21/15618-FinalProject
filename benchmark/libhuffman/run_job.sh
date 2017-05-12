qsub -v PR_PATH=`readlink -f ./huffman`,DATA_PATH=`readlink -f ~/enwiki-20170501-page.sql`,NUM_T=$1 -q cmu-15418 submit.sh
