#!/usr/bin/env python3
"""Compile/link every patched UI shader pair as real GLSL 330."""
from pathlib import Path
import re,subprocess,sys,tempfile
root=Path(sys.argv[1])
display=(root/'src/platform/ps5/display_egl.cpp').read_text()
assert 'EGL_CONTEXT_MAJOR_VERSION_KHR,\n    3,\n    EGL_CONTEXT_MINOR_VERSION_KHR,\n    3,' in display
assert 'gl_major < 3 || (gl_major == 3 && gl_minor < 3)' in display
assert 'gl_major < 4' not in display and 'context below 4.6' not in display
assert '#version 330 core' in (root/'src/gfx/gl_program.cpp').read_text()
with tempfile.TemporaryDirectory() as directory:
 for file,pairs in [('gl_batch.cpp',[('kVertex','kFragment'),('kMeshVertex','kMeshFragment')]),('renderer.cpp',[('kBlurVertex','kBlurFragment')]),('backdrop.cpp',[('kVertex','kFragment')])]:
  source=(root/'src/gfx'/file).read_text()
  assert not re.search(r'layout\(location\s*=\s*\d+\)\s+uniform',source)
  assert not re.search(r'glUniform\w+\(\d+,',source)
  shaders=dict(re.findall(r'constexpr const char \*(\w+) = R"\((.*?)\)";',source,re.S))
  for vertex,fragment in pairs:
   paths=[]
   for name,suffix in [(vertex,'vert'),(fragment,'frag')]:
    p=Path(directory)/(file+'-'+name+'.'+suffix)
    p.write_text('#version 330 core\n'+shaders[name]);paths.append(str(p))
   subprocess.run(['glslangValidator','-l',*paths],check=True)
print('PASS: OpenGL 3.3 context, named uniforms, all four GLSL 330 shader programs compile/link')
