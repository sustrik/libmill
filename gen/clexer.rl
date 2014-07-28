################################################################################
# Begin generated clexer.rb (from clexer.rl).  This (generated) file is        #
# included verbatim by mill.dna to avoid an external dependency.               #
################################################################################

%%{
	machine c;

	newline = '\n' @{curline += 1;};
	any_count_line = any | newline;

	# Consume a C comment.
	c_comment := any_count_line* :>> '*/' @{fgoto main;};

	main := |*

	# Alpha numberic characters or underscore.
	alnum_u = alnum | '_';

	# Alpha charactres or underscore.
	alpha_u = alpha | '_';

    '->' {
        stack.last << [:arrow, ts, te - 1]
    };

	# Symbols. Upon entering clear the buffer. On all transitions
	# buffer a character. Upon leaving dump the symbol.
	( punct - [_'"] ) {
        token = data[ts..te-1].pack("c*")
        if token == "("
            stack << [[:lbrace, ts, ts]]
        elsif token == ")"
            last = stack.pop
            last << [:end, ts, ts]
            if last[0][0] != :lbrace
                $stderr.write "#{curline}: Mismatched braces!\n"
                exit
            end
            stack.last << [:braces, last[0][1], ts, last[1..-1]]
        elsif token == "{"
            stack << [[:lcbrace, ts, ts]]
        elsif token == "}"
            last = stack.pop
            last << [:end, ts, ts]
            if last[0][0] != :lcbrace
                $stderr.write "#{curline}: Mismatched braces!\n"
                exit
            end
            stack.last << [:cbraces, last[0][1], ts, last[1..-1]]
        elsif token == "["
            stack << [[:lsbrace, ts, ts]]
        elsif token == "]"
            last = stack.pop
            last << [:end, ts, ts]
            if last[0][0] != :lsbrace
                $stderr.write "#{curline}: Mismatched braces!\n"
                exit
            end
            stack.last << [:sbraces, last[0][1], ts, last[1..-1]]
        elsif token == ";"
            stack.last << [:semicolon, ts, te - 1]
        elsif token == ","
            stack.last << [:comma, ts, te - 1]
        elsif token == "="
            stack.last << [:eq, ts, te - 1]
        elsif token == "."
            stack.last << [:dot, ts, te - 1]
        else
            stack.last << [:cruft, ts, te - 1]
        end
	};

	# Identifier. Upon entering clear the buffer. On all transitions
	# buffer a character. Upon leaving, dump the identifier.
	alpha_u alnum_u* {
        token = data[ts..te-1].pack("c*")
        if token == "coroutine"
            stack.last << [:coroutine, ts, te - 1]
        elsif token == "out"
            stack.last << [:out, ts, te - 1]
        elsif token == "endvars"
            stack.last << [:endvars, ts, te - 1]
        elsif token == "go"
            stack.last << [:go, ts, te - 1]
        elsif token == "select"
            stack.last << [:select, ts, te - 1]
        elsif token == "return"
            stack.last << [:return, ts, te - 1]
        elsif token == "struct"
            stack.last << [:struct, ts, te - 1]
        elsif token == "typeof"
            stack.last << [:typeof, ts, te - 1]
        elsif token == "typedef"
            stack.last << [:typedef, ts, te - 1]
        else
            stack.last << [:identifier, ts, te - 1]
        end
	};

	# Single Quote.
	sliteralChar = [^'\\] | newline | ( '\\' . any_count_line );
	'\'' . sliteralChar* . '\'' {
        stack.last << [:cruft, ts, te - 1]
    };

	# Double Quote.
	dliteralChar = [^"\\] | newline | ( '\\' any_count_line );
	'"' . dliteralChar* . '"' {
        stack.last << [:cruft, ts, te - 1]
    };

	# Whitespace is standard ws, newlines and control codes.
	any_count_line - 0x21..0x7e;

    # A pre-processor directive.
    '#' [^\n]* newline {
        token = data[ts..te-1].pack("c*")
        if (token.strip[0..7] == "#include")
            stack.last << [:include, ts, te - 1]
        else
            stack.last << [:cruft, ts, te - 1]
        end
    };

	# Describe both c style comments and c++ style comments. The
	# priority bump on tne terminator of the comments brings us
	# out of the extend* which matches everything.
	'//' [^\n]* newline;

	'/*' { fgoto c_comment; };

	# Match an integer. We don't bother clearing the buf or filling it.
	# The float machine overlaps with int and it will do it.
	digit+ {
        stack.last << [:cruft, ts, te - 1]
    };

	# Match a float. Upon entering the machine clear the buf, buffer
	# characters on every trans and dump the float upon leaving.
	digit+ '.' digit+ {
        stack.last << [:cruft, ts, te - 1]
    };

	# Match a hex. Upon entering the hex part, clear the buf, buffer characters
	# on every trans and dump the hex on leaving transitions.
	'0x' xdigit+ {
        stack.last << [:cruft, ts, te - 1]
    };

	*|;
}%%

%% write data nofinal;

def parse(data)

    curline = 1
    data = data.unpack("c*")
    eof = data.length

    stack = [[]]

	%% write init;
    %% write exec;

    stack.last << [:end, data.length - 1, data.length - 1]

    if stack.size != 1
        $stderr.write "Missing brace at the end of the source file.\n"
        exit
    end

    return stack[0]

end

################################################################################
# End generated clexer.rb (from clexer.rl).                                    #
################################################################################
