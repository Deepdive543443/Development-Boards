#!/usr/bin/env python3
"""Set the color of the CH32V203 SK9822 LED strip over USB.

The board shows up as a USB serial port (CDC-ACM). The color is saved in the
board's flash ~2 seconds after the last change, so it survives a power cycle.

Examples:
    python set_color.py FF8000          # hex
    python set_color.py "#FF8000"       # hex with # (quote it in the shell)
    python set_color.py 255 128 0       # R G B
    python set_color.py                 # show the current color
    python set_color.py default         # forget the saved color
    python set_color.py FF0000 --port /dev/ttyACM1

Requires pyserial: pip install pyserial
"""

import argparse
import sys

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    # "pip install serial" installs an unrelated package that is also imported as "serial"
    sys.exit("pyserial not found. Fix with:\n"
             "    pip uninstall serial\n"
             "    pip install pyserial")

USB_VID = 0x16C0
USB_PID = 0x27DD
PRODUCT = "CH32V203 SK9822"


def find_port():
    """Return the serial port of the board, or None."""
    candidates = [p for p in list_ports.comports() if p.vid == USB_VID and p.pid == USB_PID]
    # VID/PID are shared with other CDC devices: prefer a matching product string
    # (Windows does not always report it, so fall back to VID/PID alone)
    named = [p for p in candidates if p.product and PRODUCT in p.product]
    ports = named or candidates
    if len(ports) > 1:
        sys.exit("Several matching boards found, pick one with --port: "
                 + ", ".join(p.device for p in ports))
    return ports[0].device if ports else None


def parse_color(args):
    """Turn the command line color into the command sent to the board."""
    if not args:
        return "?"
    if len(args) == 1 and args[0].lower() == "default":
        return "DEFAULT"
    if len(args) == 3:
        try:
            rgb = [int(v) for v in args]
        except ValueError:
            sys.exit("R G B must be numbers 0..255")
        if not all(0 <= v <= 255 for v in rgb):
            sys.exit("R G B must be numbers 0..255")
        return "#{:02X}{:02X}{:02X}".format(*rgb)
    if len(args) == 1:
        value = args[0].lstrip("#")
        try:
            int(value, 16)
        except ValueError:
            value = ""
        if len(value) == 6:
            return "#" + value.upper()
    sys.exit("Color must be RRGGBB (hex), R G B (0..255), or 'default'")


def main():
    parser = argparse.ArgumentParser(description="Set the color of the SK9822 LED strip over USB.")
    parser.add_argument("color", nargs="*", help="RRGGBB, R G B, or 'default' (empty: show current color)")
    parser.add_argument("--port", help="serial port (default: find the board automatically)")
    args = parser.parse_args()

    command = parse_color(args.color)
    port = args.port or find_port()
    if not port:
        sys.exit("Board not found: is it plugged in and running the sk9822 firmware?")

    # The baudrate is ignored by the board (USB is always full speed)
    with serial.Serial(port, 115200, timeout=1) as ser:
        ser.reset_input_buffer()
        ser.write((command + "\n").encode())
        reply = ser.readline().decode(errors="replace").strip()

    if not reply:
        sys.exit("No reply from the board on " + port)
    if reply == "ERR":
        sys.exit("Board rejected the command: " + command)
    print(reply)


if __name__ == "__main__":
    main()
