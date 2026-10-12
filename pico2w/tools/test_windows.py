"""User-run Windows XInput smoke test. No serial ports, no device flashing."""
import argparse
import ctypes as c
import sys
import time

class Gamepad(c.Structure):
    _fields_ = [('buttons', c.c_uint16), ('lt', c.c_uint8), ('rt', c.c_uint8),
                ('lx', c.c_int16), ('ly', c.c_int16), ('rx', c.c_int16), ('ry', c.c_int16)]

class State(c.Structure):
    _fields_ = [('packet', c.c_uint32), ('pad', Gamepad)]

class Vibration(c.Structure):
    _fields_ = [('left', c.c_uint16), ('right', c.c_uint16)]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--index', type=int, choices=range(4), default=0)
    p.add_argument('--seconds', type=float, default=20)
    p.add_argument('--rumble', action='store_true', help='Also test each motor for 0.7 seconds')
    args = p.parse_args()
    if sys.platform != 'win32': p.error('Windows required')
    if not 0 < args.seconds <= 120: p.error('--seconds must be 0..120')
    dll = c.WinDLL('xinput1_4.dll')
    dll.XInputGetState.argtypes = [c.c_uint32, c.POINTER(State)]
    dll.XInputGetState.restype = c.c_uint32
    dll.XInputSetState.argtypes = [c.c_uint32, c.POINTER(Vibration)]
    dll.XInputSetState.restype = c.c_uint32
    for index in range(4):
        state = State(); result = dll.XInputGetState(index, c.byref(state))
        print(f'XInput slot {index}: {"connected" if result == 0 else f"unavailable ({result})"}')
    state = State()
    if dll.XInputGetState(args.index, c.byref(state)):
        p.error('Selected slot unavailable; check USB driver binding / controller index')
    try:
        if args.rumble:
            for label, pair in [('strong', (20000, 0)), ('weak', (0, 20000))]:
                print(f'Testing {label} motor for 0.7s')
                value = Vibration(*pair)
                result = dll.XInputSetState(args.index, c.byref(value))
                if result: raise RuntimeError(f'XInputSetState failed: {result}')
                time.sleep(0.7)
                dll.XInputSetState(args.index, c.byref(Vibration()))
                time.sleep(0.3)
        previous = None
        end = time.monotonic() + args.seconds
        print('Press buttons, move sticks, vary LT/RT; Ctrl+C stops.')
        while time.monotonic() < end:
            result = dll.XInputGetState(args.index, c.byref(state))
            if result: raise RuntimeError(f'XInputGetState failed: {result}')
            g = state.pad
            current = (g.buttons, g.lt, g.rt, g.lx, g.ly, g.rx, g.ry)
            if current != previous:
                print(f'buttons={g.buttons:04X} LT={g.lt:3} RT={g.rt:3} L=({g.lx:6},{g.ly:6}) R=({g.rx:6},{g.ry:6})')
                previous = current
            time.sleep(0.02)
    except KeyboardInterrupt:
        pass
    finally:
        if args.rumble: dll.XInputSetState(args.index, c.byref(Vibration()))

if __name__ == '__main__': main()
