#!/usr/bin/env python3
"""Adapt the native UI template to the pinned OpenGL 3.3 SDK."""
from pathlib import Path
import re,sys

def patch(root):
    p=root/'src/platform/ps5/display_egl.cpp'
    s=p.read_text();old='EGL_CONTEXT_MAJOR_VERSION_KHR,\n    4,\n    EGL_CONTEXT_MINOR_VERSION_KHR,\n    6,'
    assert s.count(old)==1,'UI EGL context declaration changed'
    p.write_text(s.replace(old,'EGL_CONTEXT_MAJOR_VERSION_KHR,\n    3,\n    EGL_CONTEXT_MINOR_VERSION_KHR,\n    3,'))
    p=root/'src/gfx/gl_program.cpp';s=p.read_text();assert '#version 460 core' in s
    p.write_text(s.replace('#version 460 core','#version 330 core'))
    mappings={
      'gl_batch.cpp':{0:('program','u_viewport'),1:('program','u_surface'),2:('program_','u_texture'),3:('program_','u_fonts[0]')},
      'renderer.cpp':{0:('blur_program_','u_texture'),1:('blur_program_','u_step')},
      'backdrop.cpp':{0:('program_','u_resolution'),1:('program_','u_time'),2:('program_','u_mode'),3:('program_','u_colors[0]'),7:('program_','u_params'),8:('program_','u_opacity')},
    }
    for name,mapping in mappings.items():
      p=root/'src/gfx'/name;s=p.read_text()
      s=re.sub(r'layout\(location\s*=\s*\d+\)\s+uniform\b','uniform',s)
      def uniform(m):
        program,name=mapping[int(m[2])]
        return m[1]+'glGetUniformLocation('+program+', "'+name+'"),'
      s=re.sub(r'(glUniform\w+\()(\d+),',uniform,s)
      assert not re.search(r'glUniform\w+\(\d+,',s)
      p.write_text(s)
    # Preserve early graphics errors even before the first frame is available.
    p=root/'src/platform/ps5/system.cpp';s=p.read_text()
    s=s.replace('    std::fputs(line, stdout);','    std::fputs(line, stdout);\n    if (FILE* f=std::fopen("/data/ThemeChanger/startup.log", "ab")) { std::fputs(line,f); std::fclose(f); }')
    p.write_text(s)

if __name__=='__main__':patch(Path(sys.argv[1]))
