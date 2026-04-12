import fftvm as ff

'''
# Test: Farm Scaling
# Objective: Verify Farm can scale with multiple workers.
#
# Graph:
#  Emitter -> Worker[0] -> Collector
#          -> Worker[1] ->
#          -> Worker[2] ->
'''

class Emitter(ff.SiSoNode):
    def svc(self, task):
        for i in range(100):
            self.ff_send_out(1)
        return ff.FFToken.EOS()

class Worker(ff.SiSoNode):
    def svc(self, task):
        return task * 2

class Collector(ff.SiSoNode):
    def svc_init(self):
        self.sum = 0
        return 0
    def svc(self, task):
        self.sum += task
        return ff.FFToken.GO_ON()

def run_test():
    coll = Collector()
    wf = (
        ff.Farm()
            .add_workers([Worker() for _ in range(4)])
            .add_emitter(Emitter())
            .add_collector(coll)
    )
    
    wf.run_and_wait_end()
    assert coll.sum == 200, f"Farm sum mismatch: {coll.sum} != 200"

if __name__ == "__main__":
    run_test()
    run_test()
