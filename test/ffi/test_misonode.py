import fftvm as ff

'''
# Test: MiSoNode Gathering
# Objective: Verify MiSoNode can gather tasks from multiple producers.
#
# Graph:
#  Producer 1 --- \
#                  \
#  Producer 2 ------> MiSoNode (Collector)
#                  /
#  Producer 3 --- /
'''

class Producer(ff.SiSoNode):
    def __init__(self, n):
        super().__init__()
        self.n = n
    def svc(self, task):
        for i in range(self.n):
            self.ff_send_out(1)
        return ff.FFToken.EOS()

class Collector(ff.MiSoNode):
    def svc_init(self):
        self.sum = 0
        return 0
    def svc(self, task):
        self.sum += task
        return ff.FFToken.GO_ON()

def run_test():
    p1 = Producer(10)
    p2 = Producer(20)
    p3 = Producer(30)
    coll = Collector()
    
    a2a = (
        ff.A2A()
            .add_firstset([p1, p2, p3])
            .add_secondset([coll])
    )
    
    a2a.run_and_wait_end()
    
    assert coll.sum == 60, f"Gather sum mismatch: {coll.sum} != 60"

if __name__ == "__main__":
    run_test()
    run_test()
