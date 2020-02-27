#!/usr/bin/python

#Copyright (C) 2020- The University of Notre Dame
#This software is distributed under the GNU General Public License.
#See the file COPYING for details.

import sys, os
from work_queue import *
from subprocess import Popen
from create_splits import create_splits
from time import time

postfixes = ["sa", "amb", "ann", "bwt", "pac"]

def main():
    
    if len(sys.argv) < 3:
        print("USAGE: ./wq_bwa.py <nsplits> <nworkers>")
        sys.exit(0)

    start = time()

    #nsplits = create_splits("query.fastq")
    nsplits = int(sys.argv[1])
    num_workers = int(sys.argv[2])

    q = WorkQueue(port = 0, name = "wq_bwa")
#    q.enable_monitoring()

    print("listening on port %d..." % q.port)

    # Start workers via condor
    os.system("condor_submit_workers --cores 2 --memory 4000 --disk 10000 -M wq_bwa %d" % num_workers)

    # Generate tasks and submit them
    for i in range(nsplits):
        infile = "query.fastq.%d.gz" % i
        outfile = "query.fastq.%d.sam" % i

        command = "./bwa mem ref.fastq %s | gzip > %s" % (infile, outfile)

        t = Task(command)

        t.specify_file("bwa", "bwa", WORK_QUEUE_INPUT, cache=True)
        t.specify_file("ref.fastq", "ref.fastq", WORK_QUEUE_INPUT, cache=True)
        t.specify_file("/usr/bin/gzip", "gzip", WORK_QUEUE_INPUT, cache=True)
        t.specify_file("/usr/bin/gunzip", "gunzip", WORK_QUEUE_INPUT, cache=True)
        for post in postfixes:
            t.specify_file("ref.fastq.%s" % post, "ref.fastq.%s" % post, WORK_QUEUE_INPUT, cache=True)
        t.specify_file(infile, infile, WORK_QUEUE_INPUT, cache=False)
        t.specify_file(outfile, outfile, WORK_QUEUE_OUTPUT, cache=False)

        t.specify_cores(2)
        t.specify_memory(1000)
        t.specify_disk(1000)

        taskid = q.submit(t)
        print("submitted task (id# %d): %s" % (taskid, t.command))

    #worker = []
    #for i in range(num_workers):
    #    args = ["work_queue_worker", "127.0.0.1", "%d" % q.port, "-d", "all", "-o", "worker.out.%d" % i]
    #    worker.append(Popen(args))

    # Wait for tasks to complete
    print("waiting for tasks to complete...")
    while not q.empty():
        t = q.wait(10)
        if t:
            print("task %d complete: %s (return code %d)" % (t.id, t.command, t.return_status))
#            print(t.output)
#            if t.resources_measured:
#                r = t.resources_measured
#                print("cores: %d | memory: %d | disk: %d" % (r.cores, r.memory, r.disk))

    print("all tasks complete!")
    
    #for i in range(num_workers):
    #    Popen.terminate(worker[i])

    end = time()

    print("time: %d seconds" % (end-start))

    os.system("condor_rm ccarball")

    os.system("rm -f query.fastq.*.sam")

if __name__ == "__main__":
    main()

        
