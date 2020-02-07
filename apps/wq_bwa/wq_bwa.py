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

    q = WorkQueue(port = 0, debug_log = "wq_bwa.log")

    print("listening on port %d..." % q.port)

    # Generate tasks and submit them
    for i in range(nsplits):
        infile = "query.fastq.%d" % i
        outfile = "query.fastq.%d.sam" % i

        command = "./bwa mem ref.fastq %s > %s" % (infile, outfile)

        t = Task(command)

        t.specify_file("bwa", "bwa", WORK_QUEUE_INPUT, cache=True)
        t.specify_file("ref.fastq", "ref.fastq", WORK_QUEUE_INPUT, cache=True)
        for post in postfixes:
            t.specify_file("ref.fastq.%s" % post, "ref.fastq.%s" % post, WORK_QUEUE_INPUT, cache=True)
        t.specify_file(infile, infile, WORK_QUEUE_INPUT, cache=False)
        t.specify_file(outfile, outfile, WORK_QUEUE_OUTPUT, cache=False)

        taskid = q.submit(t)
        print("submitted task (id# %d): %s" % (taskid, t.command))


    # Start work_queue_worker on localhost and q.port

    #args = ["work_queue_factory", "127.0.0.1", "%d" % q.port, "-T", "local", "-w", "1", "-W", "%d" % num_workers]
    #factory = Popen(args)

    #args = ["work_queue_worker", "127.0.0.1", "%d" % q.port, "-d", ""]
    #worker = []
    #for i in range(num_workers):
    #    worker.append(Popen(args))

    # Wait for tasks to complete
    print("waiting for tasks to complete...")
    while not q.empty():
        t = q.wait(10)
        if t:
            print("task %d complete: %s (return code %d)" % (t.id, t.command, t.return_status))

    print("all tasks complete!")
    
    #Popen.terminate(factory)

    #for i in range(num_workers):
    #    Popen.terminate(worker[i])

    end = time()

    print("time: %d seconds" % (end-start))

    os.system("rm -f query.fastq.*.sam")

if __name__ == "__main__":
    main()

        
