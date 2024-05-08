SHELL := /bin/bash
.PHONY: all prepare run clean

all: prepare run clean

run:
	source /opt/nfs/config/source_mpich420.sh; \
	source /opt/nfs/config/source_cuda121.sh; \
    /opt/nfs/config/station204_name_list.sh 1 16 > nodes; \
    mpicc main.cpp -o main; \
	mpiexec -f nodes -n 225 ./main matrix_A15.txt matrix_B15.txt 15

clean:
	rm -f nodes main
