#!/usr/bin/env python
#
# MIT License
#
# Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from optparse import OptionParser
import sys
import os
import subprocess
import ninja_syntax

def get_files(path, ext, excludes=[]):
    return [os.path.join(path, filename) for filename in os.listdir(path) if os.path.isfile(os.path.join(path, filename)) and os.path.splitext(filename)[1] == ext and os.path.basename(filename) not in excludes]

def obj(platform, src):
    if not isinstance(src, list):
        src = [src]
    return [ os.path.join('obj', platform.name(), os.path.basename(os.path.splitext(filename)[0] + '.o')) for filename in src ]

def dat(src):
    if not isinstance(src, list):
        src = [src]
    return [ os.path.join('bin/assets', os.path.basename(filename) + '.dat') for filename in src ]

def compile(platform, src, deps=[]):
    for filename in src:
        writer.build(outputs=obj(platform, filename), rule='cc', inputs=filename, implicit=deps, variables=[('cc', platform.compiler()), ('cflags', platform.cflags())])

def link(platform, output, obj):
    writer.build(outputs=output, rule='ld', inputs=obj, variables=[('ld', platform.compiler()), ('libs', platform.libs())])

def pkg_cflags(name):
    return [ s.decode('ascii') for s in subprocess.check_output('pkg-config --cflags %s' % name, shell=True).split() ]

def pkg_libs(name):
    return [ s.decode('ascii') for s in subprocess.check_output('pkg-config --libs %s' % name, shell=True).split() ]

class Platform:
    EMSCRIPTEN='emscripten'
    LINUX='linux'
    WIN='win'
    MACOSX='macosx'

    def __init__(self, target=None, cc='gcc'):
        self._target = target
        self._host = sys.platform
        self._cc = cc
        self._cflags = []
        self._libs = []

        if self._host.startswith('linux'):
            self._host = Platform.LINUX
        elif self._host.startswith('win'):
            self._host = Platform.WIN
        elif self._host.startswith('darwin'):
            self._host = Platform.MACOSX
        else:
            raise NameError('Unsupported host platform [%s]' % self._host)

        if not self._target:
            self._target = self._host

        if self._target not in Platform.known_targets():
            raise NameError('Unsupported target platform [%s]' % self._target)

    def is_linux(self):
        return self._target == Platform.LINUX

    def is_emscripten(self):
        return self._target == Platform.EMSCRIPTEN

    def is_win(self):
        return self._target == Platform.WIN

    def is_macosx(self):
        return self._target == Platform.MACOSX

    def name(self):
        return self._target

    def add_cflags(self, flags):
        if isinstance(flags, list):
            self._cflags += flags
        else:
            self._cflags.append(flags)

    def cflags(self):
        return ' '.join(self._cflags)

    def add_libs(self, libs):
        if isinstance(libs, list):
            self._libs += libs
        else:
            self._libs.append(libs)

    def libs(self):
        return ' '.join(self._libs)

    def compiler(self):
        cc = self._cc
        
        if self.is_emscripten():
            cc = 'emcc'
        if self.is_win() and self._host == Platform.LINUX:
            cc = 'x86_64-w64-mingw32-gcc'

        return cc

    def known_targets():
        return [Platform.EMSCRIPTEN, Platform.LINUX, Platform.WIN, Platform.MACOSX]

parser = OptionParser()
parser.add_option('--verbose', action='store_true', help='enable verbose build output')
parser.add_option('--target', help='target platform (%s)' % ','.join(Platform.known_targets()), choices=Platform.known_targets())
parser.add_option('--debug', action='store_true', help='enable debug build')
parser.add_option('--use_llvm', action='store_true', help='use clang as compiler')

(options, args) = parser.parse_args()

if args:
    print('ERROR: unsupported arguments:', args)
    sys.exit(-1)

writer = ninja_syntax.Writer(open('build.ninja', 'w'))

writer.comment('DO NOT EDIT THIS FILE. IT IS GENERATED BY ' + os.path.basename(__file__) + '.')
writer.newline()
writer.variable('ninja_required_version', '1.8.2')
writer.newline()
writer.rule('cc', command='$cc -MMD -MF $out.d $cflags -c -o $out $in', depfile='$out.d', deps='gcc')
writer.newline()
writer.rule('ld', command='$ld -o $out $in $libs')
writer.newline()
writer.rule('bin2txt', command='bin/bin2txt $in $out -z')
writer.newline()

# general options
includes = ['-Isrc/ext', '-Iinclude/lua', '-Iinclude/gif', '-Iinclude/zlib', '-Isrc/ext/gif', '-Iinclude/sdl2', '-Iinclude/tic80', '-Isrc/ext/net']
cflags = ['-Wall', '-std=c99'] # TODO: fix warnings and add -Werror
cc = options.use_llvm and 'clang' or 'gcc'

if options.debug:
    cflags.append('-g')
else:
    cflags += ['-O2', '-DNDEBUG']

# platform specific options
target = Platform(target=options.target, cc=cc)
host = Platform(cc=cc)

target.add_cflags(cflags + includes)
host.add_cflags(cflags + includes)

if target.is_win():
    target.add_cflags('-Ibuild/windows/include')
    target.add_libs(['-Llib/mingw64', '-lmingw32', '-lSDL2main', '-lSDL2', '-llua', '-lz', '-lws2_32', '-lcomdlg32', '-mwindows'])
elif target.is_linux():
    target.add_cflags('-D_GNU_SOURCE')
    target.add_libs(['-lSDL2', '-llua', '-lz', '-lm', '-lpthread'])
    target.add_cflags(pkg_cflags('gtk+-3.0'))
    target.add_libs(pkg_libs('gtk+-3.0'))

if host.is_win():
    host.add_cflags('-Ibuild/windows/include')
    host.add_libs(['-lz'])
elif host.is_linux():
    host.add_cflags(pkg_cflags('gtk+-3.0'))
    host.add_cflags('-D_GNU_SOURCE')
    host.add_libs(['-lz'])
    host.add_libs(pkg_libs('gtk+-3.0'))

# source files
tic_headers = get_files('include/tic80', '.h') + get_files('src', '.h')
tic_src = get_files('src', '.c')
ext_src = get_files('src/ext', '.c')
duk_src = get_files('src/ext/duktape', '.c')
net_src = get_files('src/ext/net', '.c')
gif_src = get_files('src/ext/gif', '.c')
lpeg_src = get_files('src/ext/lpeg', '.c')
bin2txt_src = get_files('tools/bin2txt', '.c')
assets_src = get_files('demos', '.tic')

if target.is_emscripten():
    assets_src += ['build/html/index.html', 'build/html/tic.js']

# build targets
for src in assets_src:
    writer.build(dat(src), 'bin2txt', src, 'bin/bin2txt')

compile(target, duk_src)
compile(target, net_src)
compile(target, gif_src)
compile(target, lpeg_src)
compile(host, bin2txt_src)
compile(target, ext_src)
compile(target, tic_src, dat(assets_src))

link(host, 'bin/bin2txt', obj(host, bin2txt_src))
link(target, 'bin/tic', obj(target, tic_src + duk_src + net_src + ext_src + gif_src + lpeg_src))

