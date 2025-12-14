import tvm_ffi
import os
import glob
import inspect

current_dir = os.path.dirname(os.path.realpath(__file__))

found_libs = glob.glob(os.path.join(current_dir, "_lib*.so"))

if not found_libs:
    raise FileNotFoundError(f"Could not find '_lib*.so' in {current_dir}")

lib_path = found_libs[0]

__libfftvm = tvm_ffi.load_module(lib_path)

@tvm_ffi.register_object("fftvm.FFToken")
class FFToken(tvm_ffi.Object):
    def __init__(self):
        self.__ffi_init__()

class _baseNodeMixin:
    @staticmethod
    def _create_safe_wrapper(instance, method):
        if not inspect.isfunction(method) and not inspect.ismethod(method):
            return lambda _, *args: method(instance, *args)
            
        try:
            sig = inspect.signature(method)
            param_count = 0
            for name, param in sig.parameters.items():
                if name == 'self':
                    continue
                if param.kind in (inspect.Parameter.POSITIONAL_ONLY, inspect.Parameter.POSITIONAL_OR_KEYWORD):
                    param_count += 1
                elif param.kind in (inspect.Parameter.VAR_POSITIONAL, inspect.Parameter.VAR_KEYWORD):
                    # If *args or **kwargs is present, stop counting and allow all arguments
                    param_count = float('inf')
                    break
            
            max_args = param_count
            
            if max_args == float('inf'):
                return lambda _, *args: method(instance, *args)
            else:
                return lambda _, *args: method(instance, *args[:max_args])

        except ValueError:
            return lambda _, *args: method(instance, *args)
        
    def __init__(self, svc=None, svc_init=None, svc_end=None, eosnotify=None):
            if svc is None:
                if not hasattr(self, 'svc'):
                    raise ValueError("When subclassing SiSoNode, you must define a 'svc' method.")
                
                assert(not svc_init)
                assert(not svc_end)
                cls = type(self)

                svc         = _baseNodeMixin._create_safe_wrapper(self, cls.svc)
                svc_init    = _baseNodeMixin._create_safe_wrapper(self, cls.svc_init) if hasattr(self, 'svc_init') else None
                svc_end     = _baseNodeMixin._create_safe_wrapper(self, cls.svc_end) if hasattr(self, 'svc_end') else None
                eosnotify     = _baseNodeMixin._create_safe_wrapper(self, cls.eosnotify) if hasattr(self, 'eosnotify') else None


            self.__ffi_init__(svc, svc_init, svc_end, eosnotify)

@tvm_ffi.register_object("fftvm.SiSoNode")
class SiSoNode(_baseNodeMixin, tvm_ffi.Object):
    def __init__(self, svc=None, svc_init=None, svc_end=None, eosnotify=None):
        # Explicitly call the Mixin's init to ensure it runs
        _baseNodeMixin.__init__(self, svc, svc_init, svc_end, eosnotify)
 
@tvm_ffi.register_object("fftvm.SiMoNode")
class SiMoNode(_baseNodeMixin, tvm_ffi.Object):
    def __init__(self, svc=None, svc_init=None, svc_end=None, eosnotify=None):
        # Explicitly call the Mixin's init to ensure it runs
        _baseNodeMixin.__init__(self, svc, svc_init, svc_end, eosnotify)

@tvm_ffi.register_object("fftvm.MiSoNode")
class MiSoNode(_baseNodeMixin, tvm_ffi.Object):
    def __init__(self, svc=None, svc_init=None, svc_end=None, eosnotify=None):
        # Explicitly call the Mixin's init to ensure it runs
        _baseNodeMixin.__init__(self, svc, svc_init, svc_end, eosnotify)

@tvm_ffi.register_object("fftvm.MiMoNode")
class MiMoNode(_baseNodeMixin, tvm_ffi.Object):
    def __init__(self, svc=None, svc_init=None, svc_end=None, eosnotify=None):
        # Explicitly call the Mixin's init to ensure it runs
        _baseNodeMixin.__init__(self, svc, svc_init, svc_end, eosnotify)


@tvm_ffi.register_object("fftvm.Pipeline")
class Pipeline(tvm_ffi.Object):
    def __init__(self):
        self.__ffi_init__()


@tvm_ffi.register_object("fftvm.Farm")
class Farm(tvm_ffi.Object):
    def __init__(self):
        self.__ffi_init__()


@tvm_ffi.register_object("fftvm.A2A")
class A2A(tvm_ffi.Object):
    def __init__(self):
        self.__ffi_init__()


@tvm_ffi.register_object("fftvm.Sink")
class Sink(tvm_ffi.Object):
    def __init__(self, fn):
        self.__ffi_init__(fn)


@tvm_ffi.register_object("fftvm.Processor")
class Processor(tvm_ffi.Object):
    def __init__(self, fn):
        self.__ffi_init__(fn)


@tvm_ffi.register_object("fftvm.Source")
class Source(tvm_ffi.Object):
    def __init__(self, fn):
        self.__ffi_init__(fn)
