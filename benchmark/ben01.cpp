#include <ff/allocator.hpp>
#include <ff/ff.hpp>


/*
 * Master-worker pattern repeated multiple times.
 *
 *
 *         ------------------------                  
 *        |              -----     |
 *        |        -----|     |    |
 *        v       |     |  W  |----
 *      -----     |      -----
 *     |  E  |----|        .
 *     |     |    |        .
 *      -----     |        .
 *        ^       |      -----
 *        |        -----|  W  | ---
 *        |             |     |    |
 *        |              -----     |
 *         ------------------------    
 *
 *
 */      

#include <vector>
#include <iostream>
#include <ff/ff.hpp>

using namespace ff;

struct Worker: ff_node_t<long> {
  long *svc(long *in) {

      volatile size_t i=0;
      while(i<1000) {
          __asm__("nop");
          i = i +1;
      }
      
      return in;
  }
};

struct Emitter: ff_node_t<long> {
    Emitter(const size_t size): size(size) {}
    int svc_init() {
        received = 0;
        return 0;
    }
    long *svc(long *in) {
        if (in == nullptr) {
            for(size_t i=0;i<size;++i) ff_send_out((long*)(i+1));
            return GO_ON;
        }
        ++received;
        if (received >= size) return EOS;
        return GO_ON;
  }
  const size_t size=0;
    size_t received = 0;
};

int main(int argc, char *argv[]) {
    size_t nw = 4;
    size_t size = 1000000;
    
    if (argc > 1) {
        if (argc != 3)  {
            std::cerr << "use: " << argv[0] << " workers size\n";
            return -1;
        }
        nw = std::stol(argv[1]);
        size = std::stol(argv[2]);        
    }
    
    Emitter E(size);
    
    ff_Farm<> farm([&]() {
            std::vector<std::unique_ptr<ff_node> > W;
            for(size_t i=0;i<nw;++i) 
                W.push_back(make_unique<Worker>());
            return W;
        } (), E);
    farm.remove_collector();
    farm.wrap_around();

    for(int i=0;i<10;++i) {
        unsigned long start = ffTime(START_TIME);
        farm.run_then_freeze();
        farm.wait_freezing();
        unsigned long end   = ffTime(STOP_TIME);
        std::cout << "Time (ms): " << end-start << "\n";
    }

    return 0;
}

