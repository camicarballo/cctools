#Copyright (C) 2020- The University of Notre Dame
#This software is distributed under the GNU General Public License.
#See the file COPYING for details.

import socket
import json
from time import sleep
from subprocess import Popen

class WorkQueueServer:

    def __init__(self, workers):
        self.socket = socket.socket()
        self.id = 1
        self.num_workers = workers
        self.wq = None

    def send_recv(self, request):
        request = json.dumps(request)
        request += "\n"
        self.send(request)

        response = self.recv()

        self.id += 1

        return response

    def connect(self, address, server_port, wq_port):
        args = ['./work_queue_server', "%d" % server_port, "%d" % wq_port, '1>', '/dev/null', '2>&1']
        self.server = Popen(args)

        #self.worker = []
        #args = ["work_queue_worker", "127.0.0.1", "%d" % wq_port]

        #for _ in range(self.num_workers):
        #    self.worker.append(Popen(args))

        args = ["work_queue_factory", "127.0.0.1", "%d" % wq_port, "-T", "local", "-w", "1", "-W", "%d" % self.num_workers]
        self.factory = Popen(args)

        i = 1
        while True:
            try:
                self.socket.connect((address, server_port))
                break
            except:
                sleep(0.1*i)
                i *= 2

    def send(self, msg):
        length = len(msg)

        total = 0
        sent = 0

        #self.socket.send(length)

        while total < length:
            sent = self.socket.send(msg[sent:])
            if sent == 0:
                print("connection closed")
            total += sent

    def recv(self):
        response = self.socket.recv(4096)
        length = ''
        for t in response:
            if t != '{':
                length += t
            else:
                break

        response = response[len(length):]
        length = int(length)

        while len(response) < length:
            response += self.socket.rec(4096)
        
        return response

    def submit(self, task):
        request = {
            "jsonrpc" : "2.0",
            "method" : "submit",
            "id" : self.id,
            "params" : task
        }

        return self.send_recv(request)

    def wait(self, timeout):
        request = {
            "jsonrpc" : "2.0",
            "method" : "wait",
            "id" : self.id,
            "params" : timeout
        }

        return self.send_recv(request)

    def remove(self, taskid):
        request = {
            "jsonrpc" : "2.0",
            "method" : "remove",
            "id" : self.id,
            "params" : taskid
        }

        return self.send_recv(request)

    def disconnect(self):
        self.socket.close()
        Popen.terminate(self.server)

        #for i in range(self.num_workers):
        #    Popen.terminate(self.worker[i])

        Popen.terminate(self.factory)


    def get_wq_stats(self):
        request = {
            "jsonrpc" : "2.0",
            "method" : "stats",
            "id" : self.id,
            "params" : ""
        }

        response = self.send_recv(request)
        response = json.loads(response)
        self.wq = json.loads(response["result"]) 

    def wq_empty(self):
        done = self.wq["tasks_done"]
        failed = self.wq["tasks_failed"]
        submitted = self.wq["tasks_submitted"]
        cancelled = self.wq["tasks_cancelled"]

        finished = done + failed + cancelled

        if (submitted - finished) > 0:
            return False
        else:
            return True
        

