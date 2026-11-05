#!/usr/bin/env python3
"""mkzip.py OUT.ZIP FILE... - reproducible zip (fixed timestamps, sorted)."""
import os, sys, zipfile
out, files = sys.argv[1], sys.argv[2:]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for f in sorted(files, key=os.path.basename):
        zi = zipfile.ZipInfo(os.path.basename(f), (1988, 6, 17, 0, 0, 0))
        zi.compress_type = zipfile.ZIP_DEFLATED
        zi.external_attr = 0o644 << 16
        z.writestr(zi, open(f, "rb").read())
print("%s: %d files" % (out, len(files)))
