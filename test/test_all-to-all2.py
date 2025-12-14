import fftvm as ff

'''
 *
 * |<----------------------- farm ------------------->|            
 *                    |<---- all-to-all ---->|
 * 
 *             ------> Router-->
 *             |                |---> Even ->
 *             |                |            |
 * Generator --|-----> Router-->|            | ---> Sink
 *             |                |            |
 *             |                |--->  Odd ->
 *             ------> Router-->
 *
 * 
 * NOTE: another version can be obtained by using all building-blocks:
 *     pipe(Generator, A2A(Router, Even), Sink)
 *
'''

class Generator(ff.SiSoNode):
    def __init__(self):
        super().__init__()

    def svc(self, task):
        self.ff_send_out(13)
        self.ff_send_out(17)
        self.ff_send_out(14)       
        self.ff_send_out(27)
        self.ff_send_out(31)
        self.ff_send_out(6)
        self.ff_send_out(8)
        self.ff_send_out(4)
        self.ff_send_out(26)
        self.ff_send_out(31)
        self.ff_send_out(105)
        self.ff_send_out(238)
        self.ff_send_out(47)
        self.ff_send_out(48)
        return ff.FFToken.EOS()

class Sink(ff.SiSoNode):
    def __init__(self):
        super().__init__()

    def svc_init(self):
        self.sum = 0
        return 1
    
    def svc(self, s):
        self.sum += s
        return ff.FFToken.GO_ON()
    
    def svc_end(self):
        assert self.sum == 271 + 344, f"something went wrong total sum: {self.sum} != {344 + 271}"

    
class Router(ff.SiMoNode):
    def __init__(self):
        super().__init__()

    def svc(self, t):
        if t % 2 == 0:
            self.ff_send_out_to(t, 0)
        else:
            self.ff_send_out_to(t, 1)

        return ff.FFToken.GO_ON()

class Even(ff.SiSoNode):
    def __init__(self):
        super().__init__()
    
    def svc_init(self):
        self.sum = 0
        return 0
    
    def svc(self, t):
        self.sum += t
        return ff.FFToken.GO_ON()
    
    def eosnotify(self):
        assert self.sum == 344, f"something went wrong even sum: {self.sum} != 344"
        self.ff_send_out(self.sum)

class Odd(ff.SiSoNode):
    def __init__(self):
        super().__init__()
    
    def svc_init(self):
        self.sum = 0
        return 0
    
    def svc(self, t):
        self.sum += t
        return ff.FFToken.GO_ON()
    
    def eosnotify(self):
        assert self.sum == 271, f"something went wrong odd sum: {self.sum} != 271"
        self.ff_send_out(self.sum)
 
wf = (
    ff.Farm()
        .add_workers([
            ff.A2A()
                .add_firstset([Router() for _ in range(3)])
                .add_secondset([Odd(), Even()])
            ])
        .add_emitter(Generator())
        .add_collector(Sink())
)

for _ in range(200):
    wf.run_and_wait_end()
