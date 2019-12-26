#!/usr/bin/env python3

import re
import sys


def main():

    if len(sys.argv) != 2:
        print("Usage: python3 decode.py DATA", file=sys.stderr)
        sys.exit(1)

    data = sys.argv[1].lower()

    if not re.match(r'^[0-9a-f]{10}$', data):
        print("Invalid DATA provided", file=sys.stderr)
        sys.exit(1)

    voltage = int(data[0:2], 16) / 10.0 if data[0:2] != 'ff' else None
    temperature = int(data[2:6], 16) / 10.0 if data[2:6] != '7fff' else None
    motion_count = int(data[6:10], 16)

    print('Bat. voltage =', voltage, 'V')
    print('Temperature  =', temperature, '°C')
    print('Motion count =', motion_count)


if __name__ == '__main__':
    main()
