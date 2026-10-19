#!/usr/bin/env python3
"""Update the pinned SDK's stale CTS assertion for queried framebuffer bits.

The runtime already compares actual GL attachment sizes with EGL configuration
attributes. Keep requiring that validation rather than the removed literal.
No runtime code or test invocation is removed.
"""
from pathlib import Path
import sys


def patch(root):
    path = root / "tests/ps5/verify_gl33_capability_audit.py"
    source = path.read_text()
    old = '"PixelFormat(8, 8, 8, 8), 32, 8, 0" in CTS_PLATFORM and'
    new = '''"PixelFormat(red, green, blue, alpha)" in CTS_PLATFORM and
        "depth, stencil, samples)" in CTS_PLATFORM and
        "GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE" in CTS_PLATFORM and
        "GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE" in CTS_PLATFORM and
        "depth != attribute(EGL_DEPTH_SIZE)" in CTS_PLATFORM and
        "stencil != attribute(EGL_STENCIL_SIZE)" in CTS_PLATFORM and
        "EGL config and actual default framebuffer disagree" in CTS_PLATFORM and'''
    if source.count(old) != 1:
        raise RuntimeError("Pinned OpenGL audit changed; inspect before applying")
    path.write_text(source.replace(old, new))


if __name__ == "__main__":
    patch(Path(sys.argv[1]))
