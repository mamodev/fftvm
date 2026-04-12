import fftvm as ff

'''
# Test: Farm with A2A Workers
# Objective: Verify complex graph with Farm, A2A, Router, and Collectors.
#
# Graph:
#  Generator -> Router[0-2] --\ /-- Odd
#                            X      |
#                           / \-- Even
#                                   |
#                                 Sink
'''

class Generator(ff.SiSoNode):
    def svc(self, task):
        tasks = [13, 17, 14, 27, 31, 6, 8, 4, 26, 31, 105, 238, 47, 48]
        for t in tasks:
            self.ff_send_out(t)
        return ff.FFToken.EOS()

class Sink(ff.SiSoNode):
    def svc_init(self):
        self.sum = 0
        return 1
    def svc(self, s):
        self.sum += s
        return ff.FFToken.GO_ON()

class Router(ff.SiMoNode):
    def svc(self, t):
        if t % 2 == 0:
            self.ff_send_out_to(t, 0)
        else:
            self.ff_send_out_to(t, 1)
        return ff.FFToken.GO_ON()

class Even(ff.SiSoNode):
    def svc_init(self):
        self.sum = 0
        return 0
    def svc(self, t):
        self.sum += t
        return ff.FFToken.GO_ON()
    def eosnotify(self):
        self.ff_send_out(self.sum)

class Odd(ff.SiSoNode):
    def svc_init(self):
        self.sum = 0
        return 0
    def svc(self, t):
        self.sum += t
        return ff.FFToken.GO_ON()
    def eosnotify(self):
        self.ff_send_out(self.sum)

def run_test():
    sink = Sink()
    wf = (
        ff.Farm()
            .add_workers([
                ff.A2A()
                    .add_firstset([Router() for _ in range(3)])
                    .add_secondset([Even(), Odd()])
                ])
            .add_emitter(Generator())
            .add_collector(sink)
    )
    wf.run_and_wait_end()
    assert sink.sum == 271 + 344, f"Mismatch: {sink.sum} != {344 + 271}"

if __name__ == "__main__":
    run_test()
    run_test()
