#!/usr/bin/bash

#Copyright (C) 2020- The University of Notre Dame
#This software is distributed under the GNU General Public License.
#See the file COPYING for details.

./fastq_generate.pl 100000 1000 > ref.fastq
#./fastq_generate.pl 10000000 10 > query.fastq      #5 splits
./fastq_generate.pl 100000000 10 > query.fastq      #50 splits
#./fastq_generate.pl 1000000000 10 > query.fastq    #500 splits

./bwa index ref.fastq

./create_splits.py

gzip query.fastq.*
