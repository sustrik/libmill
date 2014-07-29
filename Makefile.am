dist_bin_SCRIPTS = mill

include_HEADERS = mill.h stdmill.h

lib_LIBRARIES = libstdmill.a
libstdmill_a_SOURCES = stdmill.c mill.c

stdmill.c: stdmill.mc stdmill.h mill
	./mill --system $<

stdmill.h: stdmill.mh mill
	./mill --system $<

check_PROGRAMS = tests/tcpsocket
check_CLEANFILES = tests/tcpsocket.c
TESTS = $(check_PROGRAMS)
LDADD = libstdmill.a

.mc.c: mill
	./mill $<

if GEN

mill: gen/mill.dna gen/clexer.rb
	ribosome --rna $< >$@
	chmod +x $@

gen/clexer.rb: gen/clexer.rl
	ragel -R $< -o $@

CLEANFILES = mill gen/clexer.rb stdmill.h stdmill.c $(check_CLEANFILES)

else

mill: gen/mill.dna gen/clexer.rb
	@echo You have modified Mill source files in gen/, reconfigure with --enable-gen and rebuild. && false

gen/clexer.rb:

CLEANFILES = stdmill.h stdmill.c $(check_CLEANFILES)

endif
