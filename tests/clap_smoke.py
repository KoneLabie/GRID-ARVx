#!/usr/bin/env python3
"""Minimal CLAP host: scan, automation/state round-trip, and exact audio pass-through.

Uses the public CLAP C ABI through ctypes, not a DAW-specific test harness.
On headless Linux, run under xvfb-run so JUCE can initialize its message system.
"""
import ctypes as C
from pathlib import Path
import sys
import threading

P = C.c_void_p
F = C.CFUNCTYPE
class Version(C.Structure): _fields_ = [("major", C.c_uint32), ("minor", C.c_uint32), ("revision", C.c_uint32)]
class Entry(C.Structure): _fields_ = [("version", Version), ("init", P), ("deinit", P), ("factory", P)]
class Host(C.Structure):
    _fields_ = [("version", Version), ("data", P), ("name", C.c_char_p), ("vendor", C.c_char_p),
                ("url", C.c_char_p), ("host_version", C.c_char_p), ("extension", P), ("restart", P), ("process", P), ("callback", P)]
class Factory(C.Structure): _fields_ = [("count", P), ("descriptor", P), ("create", P)]
class Descriptor(C.Structure):
    _fields_ = [("version", Version)] + [(name, C.c_char_p) for name in
        ["id", "name", "vendor", "url", "manual", "support", "plugin_version", "description"]] + [("features", P)]
class Plugin(C.Structure):
    _fields_ = [(name, P) for name in ["descriptor", "data", "init", "destroy", "activate", "deactivate",
        "start", "stop", "reset", "process", "extension", "main"]]
class Buffer(C.Structure):
    _fields_ = [("data32", C.POINTER(C.POINTER(C.c_float))), ("data64", P), ("channels", C.c_uint32),
                ("latency", C.c_uint32), ("constant", C.c_uint64)]
class Process(C.Structure):
    _fields_ = [("time", C.c_int64), ("frames", C.c_uint32), ("transport", P), ("inputs", C.POINTER(Buffer)),
                ("outputs", C.POINTER(Buffer)), ("input_count", C.c_uint32), ("output_count", C.c_uint32), ("events_in", P), ("events_out", P)]
class Events(C.Structure): _fields_ = [("context", P), ("size", P), ("get", P)]
class OutputEvents(C.Structure): _fields_ = [("context", P), ("push", P)]
class Params(C.Structure): _fields_ = [(name, P) for name in ["count", "info", "value", "to_text", "from_text", "flush"]]
class ParamInfo(C.Structure):
    _fields_ = [("id", C.c_uint32), ("flags", C.c_uint32), ("cookie", P), ("name", C.c_char * 256),
                ("module", C.c_char * 1024), ("minimum", C.c_double), ("maximum", C.c_double), ("default", C.c_double)]
class Header(C.Structure): _fields_ = [("size", C.c_uint32), ("time", C.c_uint32), ("space", C.c_uint16), ("type", C.c_uint16), ("flags", C.c_uint32)]
class ParamEvent(C.Structure):
    _fields_ = [("header", Header), ("id", C.c_uint32), ("cookie", P), ("note", C.c_int32),
                ("port", C.c_int16), ("channel", C.c_int16), ("key", C.c_int16), ("value", C.c_double)]
class State(C.Structure): _fields_ = [("save", P), ("load", P)]
class Stream(C.Structure): _fields_ = [("context", P), ("callback", P)]
def pointer(callback): return C.cast(callback, P)
def call(address, result, *arguments): return F(result, *arguments)(address)

def main(path):
    path = Path(path).resolve()
    binary = next((path / "Contents/MacOS").iterdir()) if path.is_dir() else path
    library = C.CDLL(str(binary)); entry = Entry.in_dll(library, "clap_entry")
    assert call(entry.init, C.c_bool, C.c_char_p)(str(path).encode())
    no_extension = F(P, P, C.c_char_p)(lambda *_: None)
    no_request = F(None, P)(lambda *_: None)
    host = Host(Version(1, 2, 0), None, b"GRID-ARVx test host", b"Kone Labie", b"", b"1.0",
        pointer(no_extension), pointer(no_request), pointer(no_request), pointer(no_request))
    factory_ptr = call(entry.factory, P, C.c_char_p)(b"clap.plugin-factory")
    assert factory_ptr; factory = C.cast(factory_ptr, C.POINTER(Factory)).contents
    assert call(factory.count, C.c_uint32, P)(factory_ptr) == 1
    descriptor_ptr = call(factory.descriptor, P, P, C.c_uint32)(factory_ptr, 0)
    descriptor = C.cast(descriptor_ptr, C.POINTER(Descriptor)).contents
    assert descriptor.id == b"com.konelabie.gridarvx" and descriptor.name == b"GRID-ARVx"
    plugin_ptr = call(factory.create, P, P, P, C.c_char_p)(factory_ptr, C.byref(host), descriptor.id)
    assert plugin_ptr; plugin = C.cast(plugin_ptr, C.POINTER(Plugin)).contents
    assert call(plugin.init, C.c_bool, P)(plugin_ptr)
    extension = call(plugin.extension, P, P, C.c_char_p)
    params = C.cast(extension(plugin_ptr, b"clap.params"), C.POINTER(Params)).contents
    assert call(params.count, C.c_uint32, P)(plugin_ptr) == 4
    sensitivity = None
    for index in range(4):
        info = ParamInfo(); assert call(params.info, C.c_bool, P, C.c_uint32, P)(plugin_ptr, index, C.byref(info))
        if info.name == b"Sensitivity": sensitivity = info
    assert sensitivity is not None
    zero_size = F(C.c_uint32, P)(lambda *_: 0)
    zero_get = F(P, P, C.c_uint32)(lambda *_: None)
    event_size = F(C.c_uint32, P)(lambda *_: 1)
    accept = F(C.c_bool, P, P)(lambda *_: True)
    outgoing = OutputEvents(None, pointer(accept))
    def set_sensitivity(value):
        event = ParamEvent(Header(C.sizeof(ParamEvent), 0, 0, 5, 0), sensitivity.id, sensitivity.cookie, -1, -1, -1, -1, value)
        get = F(P, P, C.c_uint32)(lambda *_: C.addressof(event))
        events = Events(None, pointer(event_size), pointer(get))
        call(params.flush, None, P, P, P)(plugin_ptr, C.byref(events), C.byref(outgoing))
    def value():
        result = C.c_double(); assert call(params.value, C.c_bool, P, C.c_uint32, P)(plugin_ptr, sensitivity.id, C.byref(result)); return result.value
    set_sensitivity(2.0); assert abs(value() - 2.0) < 0.001
    state = C.cast(extension(plugin_ptr, b"clap.state"), C.POINTER(State)).contents
    saved = bytearray()
    write = F(C.c_int64, P, P, C.c_uint64)(lambda _, data, size: (saved.extend(C.string_at(data, size)), size)[1])
    stream = Stream(None, pointer(write)); assert call(state.save, C.c_bool, P, P)(plugin_ptr, C.byref(stream))
    assert saved; set_sensitivity(0.5); offset = 0
    def read(_, destination, requested):
        nonlocal offset
        data = bytes(saved[offset:offset + requested]); C.memmove(destination, data, len(data)); offset += len(data); return len(data)
    reader = F(C.c_int64, P, P, C.c_uint64)(read); stream = Stream(None, pointer(reader))
    assert call(state.load, C.c_bool, P, P)(plugin_ptr, C.byref(stream)) and abs(value() - 2.0) < 0.001
    assert call(plugin.activate, C.c_bool, P, C.c_double, C.c_uint32, C.c_uint32)(plugin_ptr, 48000.0, 1, 512)
    inputs = [(C.c_float * 512)(*[((i % 73) - 36) / 80 for i in range(512)]) for _ in range(2)]
    outputs = [(C.c_float * 512)() for _ in range(2)]
    input_pointers = (C.POINTER(C.c_float) * 2)(*inputs); output_pointers = (C.POINTER(C.c_float) * 2)(*outputs)
    input_buffer = Buffer(input_pointers, None, 2, 0, 0); output_buffer = Buffer(output_pointers, None, 2, 0, 0)
    empty = Events(None, pointer(zero_size), pointer(zero_get))
    processing = Process(0, 512, None, C.pointer(input_buffer), C.pointer(output_buffer), 1, 1, C.addressof(empty), C.addressof(outgoing))
    errors = []
    def process_audio():
        try:
            assert call(plugin.start, C.c_bool, P)(plugin_ptr)
            assert call(plugin.process, C.c_int32, P, P)(plugin_ptr, C.byref(processing)) != 0
            call(plugin.stop, None, P)(plugin_ptr)
        except Exception as error: errors.append(error)
    thread = threading.Thread(target=process_audio); thread.start(); thread.join()
    assert not errors, errors
    for source, output in zip(inputs, outputs): assert bytes(source) == bytes(output), "Audio was modified"
    call(plugin.deactivate, None, P)(plugin_ptr); call(plugin.destroy, None, P)(plugin_ptr); call(entry.deinit, None)()
    print("PASS: CLAP identity, automation, state restoration, activation, and bit-exact stereo pass-through")
if __name__ == "__main__": main(sys.argv[1])
