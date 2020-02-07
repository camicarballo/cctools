#!/usr/bin/python

#Copyright (C) 2020- The University of Notre Dame
#This software is distributed under the GNU General Public License.
#See the file COPYING for details.

import os
import sys
import json
from json_server import WorkQueueServer
from time import time
from create_splits import create_splits

tasks = []
inputs = ["bwa", "ref.fastq", "ref.fastq.sa", "ref.fastq.pac", "ref.fastq.amb", "ref.fastq.ann", "ref.fastq.bwt"]

def define_tasks(nsplits):

    for i in range(nsplits):
        task = {}
        task["command_line"] = "./bwa mem ref.fastq query.fastq.%d > query.fastq.%d.sam" % (i,i)
        task["output_files"] = []
        task["input_files"] = []

        #output files
        out = {}
        out["local_name"] = "query.fastq.%d.sam" % i
        out["remote_name"] = "query.fastq.%d.sam" % i
        
        flags = {}
        flags["cache"] = False
        flags["watch"] = False

        out["flags"] = flags
            
        task["output_files"].append(out)

        #input files
        for name in inputs:
            input_file = {}
            input_file["local_name"] = name
            input_file["remote_name"] = name
        
            flags = {}
            flags["cache"] = True
            flags["watch"] = False

            input_file["flags"] = flags
            task["input_files"].append(input_file)

        q = {}
        q["local_name"] = "query.fastq.%d" % i
        q["remote_name"] = "query.fastq.%d" % i

        flags = {}
        flags["cache"] = False
        flags["watch"] = False
    
        q["flags"] = flags
        task["input_files"].append(q)

        tasks.append(task)

def main():
    
    if len(sys.argv) < 3:
        print("USAGE: ./wq_bwa_json.py <nsplits> <nworkers>")
        sys.exit(0)

    start = time()

    #generate tasks
    #nsplits = create_splits("query.fastq")
    define_tasks(int(sys.argv[1]))

    q = WorkQueueServer(int(sys.argv[2]))

    #connect to server
    q.connect('127.0.0.1', 2345, 1234)
  
    #submit tasks
    for t in tasks:
        t = json.dumps(t)
        response = q.submit(t)
        print(response)
        
    #submit wait requests
    q.get_wq_stats()
    while not q.wq_empty():
        response = q.wait(10)
        print(response)
        q.get_wq_stats()

    #disconnect
    q.disconnect()

    end = time()

    start = float(start)
    end = float(end)

    print("time: {}".format(end-start-1))

    os.system("rm -f query.fastq.*.sam")

if __name__ == "__main__":
    main()
