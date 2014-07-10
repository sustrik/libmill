dist_bin_SCRIPTS = mill

mill: mill.dna
	ribosome --rna $< >$@
	chmod +x $@

include_HEADERS = mill.h stdmill.h

lib_LIBRARIES = libstdmill.a
libstdmill_a_SOURCES = stdmill.c mill.c

stdmill.c: stdmill.mc stdmill.h mill
	./mill --system $<

stdmill.h: stdmill.mh mill
	./mill --system $<
