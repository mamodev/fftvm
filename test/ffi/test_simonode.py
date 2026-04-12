import fftvm as ff

'''
# Test: SiMoNode Routing
# Objective: Verify SiMoNode can route even/odd numbers to different stages.
#
# Graph:
#  Generator -> SiMoNode (Router) -> EvenStage (id 0) -> Sink
#                               \-> OddStage  (id 1) /
'''

class Generator(ff.SiSoNode):
    def svc(self, task):
        for i in range(100):
            self.ff_send_out(i)
        return ff.FFToken.EOS()

class Router(ff.SiMoNode):
    def svc(self, task):
        if task % 2 == 0:
            self.ff_send_out_to(task, 0)
        else:
            self.ff_send_out_to(task, 1)
        return ff.FFToken.GO_ON()

class Collector(ff.SiSoNode):
    def svc_init(self):
        self.count = 0
        return 0
    def svc(self, task):
        self.count += 1
        return ff.FFToken.GO_ON()
    def eosnotify(self):
        self.ff_send_out(self.count)

class Sink(ff.MiSoNode):
    def svc_init(self):
        self.total = 0
        return 0
    def svc(self, task):
        self.total += task
        return ff.FFToken.GO_ON()

def run_test():
    even = Collector()
    odd = Collector()
    sink = Sink()
    
    # Using A2A as a simple way to connect SiMo to multiple SiSo
    a2a = (
        ff.A2A()
            .add_firstset([Router()])
            .add_secondset([even, odd])
    )
    
    pipe = (
        ff.Pipeline()
            .add_stage(Generator())
            .add_stage(a2a)
            .add_stage(sink)
    )
    
    pipe.run_and_wait_end()
    
    assert even.count == 50, f"Even count mismatch: {even.count}"
    assert odd.count == 50, f"Odd count mismatch: {odd.count}"
    assert sink.total == 100, f"Total count mismatch: {sink.total}"

if __name__ == "__main__":
    run_test()
    run_test()
