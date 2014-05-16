
# line 1 "clexer.rl"


# line 144 "clexer.rl"



# line 10 "clexer.rb"
class << self
	attr_accessor :_c_actions
	private :_c_actions, :_c_actions=
end
self._c_actions = [
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 7, 1, 8, 1, 
	9, 1, 10, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 2, 0, 10, 2, 0, 
	11, 2, 0, 12, 2, 4, 5, 2, 
	4, 6
]

class << self
	attr_accessor :_c_key_offsets
	private :_c_key_offsets, :_c_key_offsets=
end
self._c_key_offsets = [
	0, 0, 3, 4, 5, 8, 9, 10, 
	12, 18, 20, 23, 45, 46, 48, 52, 
	54, 57, 63, 70
]

class << self
	attr_accessor :_c_trans_keys
	private :_c_trans_keys, :_c_trans_keys=
end
self._c_trans_keys = [
	10, 34, 92, 10, 10, 10, 39, 92, 
	10, 10, 48, 57, 48, 57, 65, 70, 
	97, 102, 10, 42, 10, 42, 47, 10, 
	34, 35, 39, 47, 48, 64, 95, 33, 
	46, 49, 57, 58, 63, 65, 90, 91, 
	96, 97, 122, 123, 126, 10, 42, 47, 
	46, 120, 48, 57, 48, 57, 46, 48, 
	57, 48, 57, 65, 70, 97, 102, 95, 
	48, 57, 65, 90, 97, 122, 0
]

class << self
	attr_accessor :_c_single_lengths
	private :_c_single_lengths, :_c_single_lengths=
end
self._c_single_lengths = [
	0, 3, 1, 1, 3, 1, 1, 0, 
	0, 2, 3, 8, 1, 2, 2, 0, 
	1, 0, 1, 0
]

class << self
	attr_accessor :_c_range_lengths
	private :_c_range_lengths, :_c_range_lengths=
end
self._c_range_lengths = [
	0, 0, 0, 0, 0, 0, 0, 1, 
	3, 0, 0, 7, 0, 0, 1, 1, 
	1, 3, 3, 0
]

class << self
	attr_accessor :_c_index_offsets
	private :_c_index_offsets, :_c_index_offsets=
end
self._c_index_offsets = [
	0, 0, 4, 6, 8, 12, 14, 16, 
	18, 22, 25, 29, 45, 47, 50, 54, 
	56, 59, 63, 68
]

class << self
	attr_accessor :_c_trans_targs
	private :_c_trans_targs, :_c_trans_targs=
end
self._c_trans_targs = [
	1, 11, 2, 1, 1, 1, 11, 3, 
	4, 11, 5, 4, 4, 4, 11, 6, 
	15, 11, 17, 17, 17, 11, 9, 10, 
	9, 9, 10, 19, 9, 11, 1, 12, 
	4, 13, 14, 18, 18, 11, 16, 11, 
	18, 11, 18, 11, 11, 11, 3, 11, 
	6, 11, 7, 8, 16, 11, 15, 11, 
	7, 16, 11, 17, 17, 17, 11, 18, 
	18, 18, 18, 11, 0, 11, 11, 11, 
	11, 11, 11, 11, 11, 11, 11, 11, 
	0
]

class << self
	attr_accessor :_c_trans_actions
	private :_c_trans_actions, :_c_trans_actions=
end
self._c_trans_actions = [
	1, 15, 0, 0, 1, 0, 38, 0, 
	1, 13, 0, 0, 1, 0, 41, 0, 
	0, 31, 0, 0, 0, 31, 1, 0, 
	0, 1, 0, 3, 0, 35, 0, 9, 
	0, 9, 9, 44, 47, 11, 9, 11, 
	47, 11, 47, 11, 17, 38, 0, 19, 
	0, 21, 0, 0, 9, 23, 0, 25, 
	0, 9, 23, 0, 0, 0, 27, 47, 
	47, 47, 47, 33, 0, 29, 29, 31, 
	31, 21, 21, 23, 25, 23, 27, 33, 
	0
]

class << self
	attr_accessor :_c_to_state_actions
	private :_c_to_state_actions, :_c_to_state_actions=
end
self._c_to_state_actions = [
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 5, 0, 5, 0, 0, 0, 0, 
	0, 0, 0, 0
]

class << self
	attr_accessor :_c_from_state_actions
	private :_c_from_state_actions, :_c_from_state_actions=
end
self._c_from_state_actions = [
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 7, 0, 0, 0, 0, 
	0, 0, 0, 0
]

class << self
	attr_accessor :_c_eof_trans
	private :_c_eof_trans, :_c_eof_trans=
end
self._c_eof_trans = [
	0, 0, 0, 71, 0, 0, 71, 73, 
	73, 0, 0, 0, 75, 75, 78, 77, 
	78, 79, 80, 0
]

class << self
	attr_accessor :c_start
end
self.c_start = 11;
class << self
	attr_accessor :c_error
end
self.c_error = 0;

class << self
	attr_accessor :c_en_c_comment
end
self.c_en_c_comment = 9;
class << self
	attr_accessor :c_en_main
end
self.c_en_main = 11;


# line 147 "clexer.rl"

def parse(data)

    curline = 1
    data = data.unpack("c*")
    eof = data.length

    stack = [[]]

	
# line 177 "clexer.rb"
begin
	p ||= 0
	pe ||= data.length
	cs = c_start
	ts = nil
	te = nil
	act = 0
end

# line 157 "clexer.rl"
    
# line 189 "clexer.rb"
begin
	_klen, _trans, _keys, _acts, _nacts = nil
	_goto_level = 0
	_resume = 10
	_eof_trans = 15
	_again = 20
	_test_eof = 30
	_out = 40
	while true
	_trigger_goto = false
	if _goto_level <= 0
	if p == pe
		_goto_level = _test_eof
		next
	end
	if cs == 0
		_goto_level = _out
		next
	end
	end
	if _goto_level <= _resume
	_acts = _c_from_state_actions[cs]
	_nacts = _c_actions[_acts]
	_acts += 1
	while _nacts > 0
		_nacts -= 1
		_acts += 1
		case _c_actions[_acts - 1]
			when 3 then
# line 1 "NONE"
		begin
ts = p
		end
# line 223 "clexer.rb"
		end # from state action switch
	end
	if _trigger_goto
		next
	end
	_keys = _c_key_offsets[cs]
	_trans = _c_index_offsets[cs]
	_klen = _c_single_lengths[cs]
	_break_match = false
	
	begin
	  if _klen > 0
	     _lower = _keys
	     _upper = _keys + _klen - 1

	     loop do
	        break if _upper < _lower
	        _mid = _lower + ( (_upper - _lower) >> 1 )

	        if data[p].ord < _c_trans_keys[_mid]
	           _upper = _mid - 1
	        elsif data[p].ord > _c_trans_keys[_mid]
	           _lower = _mid + 1
	        else
	           _trans += (_mid - _keys)
	           _break_match = true
	           break
	        end
	     end # loop
	     break if _break_match
	     _keys += _klen
	     _trans += _klen
	  end
	  _klen = _c_range_lengths[cs]
	  if _klen > 0
	     _lower = _keys
	     _upper = _keys + (_klen << 1) - 2
	     loop do
	        break if _upper < _lower
	        _mid = _lower + (((_upper-_lower) >> 1) & ~1)
	        if data[p].ord < _c_trans_keys[_mid]
	          _upper = _mid - 2
	        elsif data[p].ord > _c_trans_keys[_mid+1]
	          _lower = _mid + 2
	        else
	          _trans += ((_mid - _keys) >> 1)
	          _break_match = true
	          break
	        end
	     end # loop
	     break if _break_match
	     _trans += _klen
	  end
	end while false
	end
	if _goto_level <= _eof_trans
	cs = _c_trans_targs[_trans]
	if _c_trans_actions[_trans] != 0
		_acts = _c_trans_actions[_trans]
		_nacts = _c_actions[_acts]
		_acts += 1
		while _nacts > 0
			_nacts -= 1
			_acts += 1
			case _c_actions[_acts - 1]
when 0 then
# line 5 "clexer.rl"
		begin
curline += 1;		end
when 1 then
# line 9 "clexer.rl"
		begin
	begin
		cs = 11
		_trigger_goto = true
		_goto_level = _again
		break
	end
		end
when 4 then
# line 1 "NONE"
		begin
te = p+1
		end
when 5 then
# line 21 "clexer.rl"
		begin
act = 1;		end
when 6 then
# line 66 "clexer.rl"
		begin
act = 2;		end
when 7 then
# line 21 "clexer.rl"
		begin
te = p+1
 begin 
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
        elsif token == ":"
            stack.last << [:colon, ts, te - 1]
        elsif token == ";"
            stack.last << [:semicolon, ts, te - 1]
        elsif token == ","
            stack.last << [:comma, ts, te - 1]
        else
            stack.last << [:cruft, ts, te - 1]
        end
	 end
		end
when 8 then
# line 95 "clexer.rl"
		begin
te = p+1
 begin 
        stack.last << [:cruft, ts, te - 1]
     end
		end
when 9 then
# line 101 "clexer.rl"
		begin
te = p+1
 begin 
        stack.last << [:cruft, ts, te - 1]
     end
		end
when 10 then
# line 106 "clexer.rl"
		begin
te = p+1
		end
when 11 then
# line 109 "clexer.rl"
		begin
te = p+1
 begin 
        token = data[ts..te-1].pack("c*")
        if (token.strip[0..7] == "#include")
            stack.last << [:include, ts, te - 1]
        else
            stack.last << [:cruft, ts, te - 1]
        end
     end
		end
when 12 then
# line 121 "clexer.rl"
		begin
te = p+1
		end
when 13 then
# line 123 "clexer.rl"
		begin
te = p+1
 begin  	begin
		cs = 9
		_trigger_goto = true
		_goto_level = _again
		break
	end
  end
		end
when 14 then
# line 21 "clexer.rl"
		begin
te = p
p = p - 1; begin 
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
        elsif token == ":"
            stack.last << [:colon, ts, te - 1]
        elsif token == ";"
            stack.last << [:semicolon, ts, te - 1]
        elsif token == ","
            stack.last << [:comma, ts, te - 1]
        else
            stack.last << [:cruft, ts, te - 1]
        end
	 end
		end
when 15 then
# line 127 "clexer.rl"
		begin
te = p
p = p - 1; begin 
        stack.last << [:cruft, ts, te - 1]
     end
		end
when 16 then
# line 133 "clexer.rl"
		begin
te = p
p = p - 1; begin 
        stack.last << [:cruft, ts, te - 1]
     end
		end
when 17 then
# line 139 "clexer.rl"
		begin
te = p
p = p - 1; begin 
        stack.last << [:cruft, ts, te - 1]
     end
		end
when 18 then
# line 21 "clexer.rl"
		begin
 begin p = ((te))-1; end
 begin 
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
        elsif token == ":"
            stack.last << [:colon, ts, te - 1]
        elsif token == ";"
            stack.last << [:semicolon, ts, te - 1]
        elsif token == ","
            stack.last << [:comma, ts, te - 1]
        else
            stack.last << [:cruft, ts, te - 1]
        end
	 end
		end
when 19 then
# line 127 "clexer.rl"
		begin
 begin p = ((te))-1; end
 begin 
        stack.last << [:cruft, ts, te - 1]
     end
		end
when 20 then
# line 1 "NONE"
		begin
	case act
	when 1 then
	begin begin p = ((te))-1; end

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
        elsif token == ":"
            stack.last << [:colon, ts, te - 1]
        elsif token == ";"
            stack.last << [:semicolon, ts, te - 1]
        elsif token == ","
            stack.last << [:comma, ts, te - 1]
        else
            stack.last << [:cruft, ts, te - 1]
        end
	end
	when 2 then
	begin begin p = ((te))-1; end

        token = data[ts..te-1].pack("c*")
        if token == "coroutine"
            stack.last << [:coroutine, ts, te - 1]
        elsif token == "endvars"
            stack.last << [:endvars, ts, te - 1]
        elsif token == "call"
            stack.last << [:call, ts, te - 1]
        elsif token == "wait"
            stack.last << [:wait, ts, te - 1]
        elsif token == "@who"
            stack.last << [:who, ts, te - 1]
        elsif token == "@tag"
            stack.last << [:tag, ts, te - 1]
        elsif token == "@err"
            stack.last << [:err, ts, te - 1]
        elsif token == "return"
            stack.last << [:return, ts, te - 1]
        elsif token == "struct"
            stack.last << [:struct, ts, te - 1]
        elsif token == "tcpsocket_init"
            stack.last << [:tcpsocket_init, ts, te - 1]
        else
            stack.last << [:identifier, ts, te - 1]
        end
	end
end 
			end
# line 618 "clexer.rb"
			end # action switch
		end
	end
	if _trigger_goto
		next
	end
	end
	if _goto_level <= _again
	_acts = _c_to_state_actions[cs]
	_nacts = _c_actions[_acts]
	_acts += 1
	while _nacts > 0
		_nacts -= 1
		_acts += 1
		case _c_actions[_acts - 1]
when 2 then
# line 1 "NONE"
		begin
ts = nil;		end
# line 638 "clexer.rb"
		end # to state action switch
	end
	if _trigger_goto
		next
	end
	if cs == 0
		_goto_level = _out
		next
	end
	p += 1
	if p != pe
		_goto_level = _resume
		next
	end
	end
	if _goto_level <= _test_eof
	if p == eof
	if _c_eof_trans[cs] > 0
		_trans = _c_eof_trans[cs] - 1;
		_goto_level = _eof_trans
		next;
	end
end
	end
	if _goto_level <= _out
		break
	end
	end
	end

# line 158 "clexer.rl"

    stack.last << [:end, data.length - 1, data.length - 1]

    if stack.size != 1
        $stderr.write "Missing brace at the end of the source file.\n"
        exit
    end

    return stack[0]

end

