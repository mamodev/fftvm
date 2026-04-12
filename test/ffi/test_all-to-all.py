import fftvm as ff

'''
# Test: FastFlow All-to-All (1:1)
# Objective: Verify basic A2A data flow across multiple stages.
#
# Graph:
#  FirstStage[0] --\ /-- SecondStage[0]
#                   X
#  FirstStage[1] --/ \-- SecondStage[1]
#
'''

NTasks = 1000
NodesPerStage = 3

class FirstStage(ff.SiSoNode):
    def svc(self, task):
        for i in range(NTasks):
            self.ff_send_out(i)
        return ff.FFToken.EOS()

class SecondStage(ff.SiSoNode):
    def svc_init(self):
        self.n = 0
        return 0
    def svc(self, task):
        self.n += 1
        return ff.FFToken.GO_ON()

def run_test():
    first_set = [FirstStage() for _ in range(NodesPerStage)]
    second_set = [SecondStage() for _ in range(NodesPerStage)]
    a2a = (
        ff.A2A()
           .add_firstset(first_set)
           .add_secondset(second_set)
    )
    a2a.run_and_wait_end()
    
    total = sum(node.n for node in second_set)
    assert total == NTasks * NodesPerStage, f"Mismatch: {total} != {NTasks * NodesPerStage}"

if __name__ == "__main__":
    run_test()
    run_test()
