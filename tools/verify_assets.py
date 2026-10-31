#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Verify all five exact runtime files without modifying them."""
import argparse
from pathlib import Path
from prepare_assets import verify

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        verify(args.directory)
        print('PASS: all five sizes and SHA-256 values match')
        return 0
    except (OSError, ValueError) as error:
        parser.exit(1, 'FAILED: ' + str(error) + '\n')

if __name__ == '__main__':
    raise SystemExit(main())
