import fftvm as ff

'''
/*
 *    first _    _ second
 *           |  |
 *    first _| -|- second
 *           |  |
 *    first -    - second
 *
*/ 
'''

NTasks = 1000
NodesPerStage = 3
Total = 0


class FirstStage(ff.SiSoNode):
    def __init__(self):
        super().__init__()

    def svc(self, task):
        for i in range(NTasks):
            self.ff_send_out(i)

        return ff.FFToken.EOS()

class SecondStage(ff.SiSoNode):
    def __init__(self):
        super().__init__()

    def svc_init(self):
        self.n = 0
        return 0

    def svc(self, task):
        self.n = self.n + 1
        return ff.FFToken.GO_ON()
    
    def eosnotify(self):
        global Total
        Total = Total + self.n

a2a = (
    ff.A2A()
       .add_firstset([FirstStage() for _ in range(NodesPerStage)])
       .add_secondset([SecondStage() for _ in range(NodesPerStage)])
)


a2a.run_and_wait_end()
a2a.run_and_wait_end()

assert Total == 2 * NTasks * NodesPerStage, "Something went wrong"
