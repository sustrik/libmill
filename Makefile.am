#
#   Copyright (c) 2014 Martin Sustrik  All rights reserved.
#
#   Permission is hereby granted, free of charge, to any person obtaining a copy
#   of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom
#   the Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
#   IN THE SOFTWARE.
#

dist_bin_SCRIPTS = mill

include_HEADERS = stdmill.h

lib_LIBRARIES = libstdmill.a
libstdmill_a_SOURCES = stdmill.c

check_PROGRAMS = \
    tests/tcpsocket \
    tests/pending \
    tests/cancel \
    tests/getaddressinfo
check_CLEANFILES = \
    tests/tcpsocket.c \
    tests/pending.c \
    tests/cancel.c \
    tests/getaddressinfo.c

TESTS = $(check_PROGRAMS)
LDADD = libstdmill.a

.mc.c: mill
	./mill $<

################################################################################
# Following part of the build process is used only by Mill developers.         #
# Files generated here are added to git repository so that Mill users don't    #
# have to insall all the code generation tools (ragel, ribosome).              #
################################################################################

if GEN

mill: gen/mill.dna gen/clexer.rb
	ribosome --rna $< >$@
	chmod +x $@

gen/clexer.rb: gen/clexer.rl
	ragel -R $< -o $@

stdmill.c: gen/stdmill.mc mill
	./mill --system $<
	mv gen/stdmill.c .

stdmill.h: gen/stdmill.mh mill
	./mill --system $<
	mv gen/stdmill.h .

CLEANFILES = mill gen/clexer.rb stdmill.h stdmill.c $(check_CLEANFILES)

else

mill: gen/mill.dna gen/clexer.rb
	@echo You have modified Mill source files in gen/, reconfigure with --enable-gen and rebuild. && false

gen/clexer.rb:

CLEANFILES = $(check_CLEANFILES)

endif

