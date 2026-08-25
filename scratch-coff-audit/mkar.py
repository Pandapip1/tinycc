#!/usr/bin/env python3
"""Build an ar archive (no symbol index) from member files, so tcc_load_archive
walks the members and hands each to tcc_load_member()."""
import sys, os
out, members = sys.argv[1], sys.argv[2:]
b = b'!<arch>\n'
for m in members:
    d = open(m, 'rb').read()
    nm = (os.path.basename(m) + '/').ljust(16)[:16]
    b += (nm + '0'.ljust(12) + '0'.ljust(6) + '0'.ljust(6) + '644'.ljust(8)
          + str(len(d)).ljust(10) + '`\n').encode()
    b += d + (b'\n' if len(d) % 2 else b'')
open(out, 'wb').write(b)
