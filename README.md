# FFTVM

This projects aims to bring fastflow power to python!
The project is in the setup phase, so for the moment there will be no exaustive doc provided!
if you are interested contact me: marco.morozzi2002@gmail.com


### Installation

easyest method to obtain fftvm is to installit trought pip:
```bash
pip install fftvm
```

to manually install it, dowload the source code and run
```bash
pip install . -v
```

inside fftvm repo folder. This will build and install fftvm inside your current environment.


### Example
```python
from fftvm import Pipeline, Farm, Processor, Source, Sink, A2A
import fftvm

print(f"Loaded libfftvm: {fftvm.__libfftvm}")


def source():
    if not hasattr(source, "count"):
        source.count = 0
    else:
        source.count += 1

    return None if source.count > 10 else source.count


def node(x):
    return x + 1


p = (
    Pipeline()
    .add_stage(Source(source))
    .add_stage(
        Farm().add_workers([Processor(n) for n in [node] * 2])
        # .add_collector(None)
        .debug_info()
    )
    .add_stage(
        A2A()
        .add_firstset([Processor(n) for n in [node] * 2])
        .add_secondset([Processor(n) for n in [node] * 2])
        .debug_info()
    )
    .add_stage(
        Farm()
        .add_workers([Processor(n) for n in [node] * 2])
        .add_collector(None)
        .debug_info()
    )
    .add_stage(Sink(lambda x: print(x)))
)

p.run_and_wait_end()
```