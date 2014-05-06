
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

	# Symbols. Upon entering clear the buffer. On all transitions
	# buffer a character. Upon leaving dump the symbol.
	( punct - [_'"] ) {
        token = data[ts..te-1].pack("c*")

        if phase == :phase1
            if token == "("
                stack << [[:lbrace, ts]]
            elsif token == ")"
                last = stack.pop
                if last[0][0] != :lbrace
                    $stderr.write "#{curline}: Mismatched braces!\n"
                    exit
                end
                stack.last << [:braces, last[0][1], ts, last[1..-1]]
            elsif token == "{"
                stack << [[:lcbrace, ts]]
            elsif token == "}"
                last = stack.pop
                if last[0][0] != :lcbrace
                    $stderr.write "#{curline}: Mismatched braces!\n"
                    exit
                end
                stack.last << [:cbraces, last[0][1], ts, last[1..-1]]
            elsif token == ":"
                stack.last << [:colon, ts, te - 1]
            else
                stack.last << [:cruft]
            end
        elsif phase == :phase3
            if token == ","
                tokens << [ts, te - 1, :comma, token]
            end
        elsif phase == :phase4
            if token == ";"
                tokens << [ts, te - 1, :semicolon, token]
            end
        elsif phase == :phase5
            if token == "("
                tokens << [ts, te - 1, :lbrace, token]
            end
        end
	};

	# Identifier. Upon entering clear the buffer. On all transitions
	# buffer a character. Upon leaving, dump the identifier.
	alpha_u alnum_u* {
        token = data[ts..te-1].pack("c*")

        if phase == :phase1
            if token == "coroutine"
                stack.last << [:coroutine, ts, te - 1]
            elsif token == "body"
                stack.last << [:body, ts, te - 1]
            elsif token == "cleanup"
                stack.last << [:cleanup, ts, te - 1]
            else
                stack.last << [:cruft]
            end
        elsif phase == :phase3
            tokens << [ts, te - 1, :identifier, token]
        elsif phase == :phase4
            tokens << [ts, te - 1, :identifier, token]
        elsif phase == :phase5
            tokens << [ts, te - 1, :identifier, token]
        end
	};

	# Single Quote.
	sliteralChar = [^'\\] | newline | ( '\\' . any_count_line );
	'\'' . sliteralChar* . '\'' {
        if phase == :phase1
            stack.last << [:cruft]
        end
    };

	# Double Quote.
	dliteralChar = [^"\\] | newline | ( '\\' any_count_line );
	'"' . dliteralChar* . '"' {
        if phase == :phase1
            stack.last << [:cruft]
        end
    };

	# Whitespace is standard ws, newlines and control codes.
	any_count_line - 0x21..0x7e;

    # A pre-processor directive.
    '#' [^\n]* newline {
        if phase == :phase1
            stack.last << [:cruft]
        elsif phase == :phase2
            token = data[ts..te-1].pack("c*")
            if (token.strip[0..7] == "#include")
                start = token.index(/[<\"]/)
                stop = token.index(/[>\"]/, start + 1)
                tokens << [ts + start + 1, ts + stop - 1, token[start + 1..stop - 1]]
            end
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
        if phase == :phase1
            stack.last << [:cruft]
        end
    };

	# Match a float. Upon entering the machine clear the buf, buffer
	# characters on every trans and dump the float upon leaving.
	digit+ '.' digit+ {
        if phase == :phase1
            stack.last << [:cruft]
        end
    };

	# Match a hex. Upon entering the hex part, clear the buf, buffer characters
	# on every trans and dump the hex on leaving transitions.
	'0x' xdigit+ {
        if phase == :phase1
            stack.last << [:cruft]
        end
    };

	*|;
}%%

%% write data nofinal;

def parse(phase, data)

    curline = 1
    data = data.unpack("c*")
    eof = data.length

    if phase == :phase1
        stack = [[]]
    else
        tokens = []
    end

	%% write init;
    %% write exec;

    if phase == :phase1
        if stack.size != 1
            $stderr.write "#{curline}: Missing brace at the end of the source file.\n"
            exit
        end
        return stack[0]
    else
        return tokens
    end

end

