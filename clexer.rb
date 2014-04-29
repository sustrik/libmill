
# line 1 "clexer.rl"


# line 73 "clexer.rl"



# line 10 "clexer.rb"
class << self
	attr_accessor :_c_actions
	private :_c_actions, :_c_actions=
end
self._c_actions = [
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 10, 1, 11, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 2, 0, 8, 2, 0, 
	9
]

class << self
	attr_accessor :_c_key_offsets
	private :_c_key_offsets, :_c_key_offsets=
end
self._c_key_offsets = [
	0, 0, 3, 4, 7, 8, 9, 11, 
	17, 19, 22, 42, 44, 48, 50, 53, 
	59, 67
]

class << self
	attr_accessor :_c_trans_keys
	private :_c_trans_keys, :_c_trans_keys=
end
self._c_trans_keys = [
	10, 34, 92, 10, 10, 39, 92, 10, 
	10, 48, 57, 48, 57, 65, 70, 97, 
	102, 10, 42, 10, 42, 47, 10, 34, 
	39, 47, 48, 95, 33, 46, 49, 57, 
	58, 64, 65, 90, 91, 96, 97, 122, 
	123, 126, 42, 47, 46, 120, 48, 57, 
	48, 57, 46, 48, 57, 48, 57, 65, 
	70, 97, 102, 36, 95, 48, 57, 65, 
	90, 97, 122, 0
]

class << self
	attr_accessor :_c_single_lengths
	private :_c_single_lengths, :_c_single_lengths=
end
self._c_single_lengths = [
	0, 3, 1, 3, 1, 1, 0, 0, 
	2, 3, 6, 2, 2, 0, 1, 0, 
	2, 0
]

class << self
	attr_accessor :_c_range_lengths
	private :_c_range_lengths, :_c_range_lengths=
end
self._c_range_lengths = [
	0, 0, 0, 0, 0, 0, 1, 3, 
	0, 0, 7, 0, 1, 1, 1, 3, 
	3, 0
]

class << self
	attr_accessor :_c_index_offsets
	private :_c_index_offsets, :_c_index_offsets=
end
self._c_index_offsets = [
	0, 0, 4, 6, 10, 12, 14, 16, 
	20, 23, 27, 41, 44, 48, 50, 53, 
	57, 63
]

class << self
	attr_accessor :_c_trans_targs
	private :_c_trans_targs, :_c_trans_targs=
end
self._c_trans_targs = [
	1, 10, 2, 1, 1, 1, 3, 10, 
	4, 3, 3, 3, 10, 5, 13, 10, 
	15, 15, 15, 10, 8, 9, 8, 8, 
	9, 17, 8, 10, 1, 3, 11, 12, 
	16, 10, 14, 10, 16, 10, 16, 10, 
	10, 10, 5, 10, 6, 7, 14, 10, 
	13, 10, 6, 14, 10, 15, 15, 15, 
	10, 16, 16, 16, 16, 16, 10, 0, 
	10, 10, 10, 10, 10, 10, 10, 10, 
	10, 0
]

class << self
	attr_accessor :_c_trans_actions
	private :_c_trans_actions, :_c_trans_actions=
end
self._c_trans_actions = [
	1, 15, 0, 0, 1, 0, 1, 13, 
	0, 0, 1, 0, 38, 0, 0, 33, 
	0, 0, 0, 33, 1, 0, 0, 1, 
	0, 3, 0, 35, 0, 0, 9, 9, 
	0, 11, 9, 11, 0, 11, 0, 11, 
	17, 19, 0, 21, 0, 0, 9, 25, 
	0, 27, 0, 9, 25, 0, 0, 0, 
	29, 0, 0, 0, 0, 0, 23, 0, 
	31, 33, 33, 21, 25, 27, 25, 29, 
	23, 0
]

class << self
	attr_accessor :_c_to_state_actions
	private :_c_to_state_actions, :_c_to_state_actions=
end
self._c_to_state_actions = [
	0, 0, 0, 0, 0, 0, 0, 0, 
	5, 0, 5, 0, 0, 0, 0, 0, 
	0, 0
]

class << self
	attr_accessor :_c_from_state_actions
	private :_c_from_state_actions, :_c_from_state_actions=
end
self._c_from_state_actions = [
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 7, 0, 0, 0, 0, 0, 
	0, 0
]

class << self
	attr_accessor :_c_eof_trans
	private :_c_eof_trans, :_c_eof_trans=
end
self._c_eof_trans = [
	0, 0, 0, 0, 0, 65, 67, 67, 
	0, 0, 0, 68, 71, 70, 71, 72, 
	73, 0
]

class << self
	attr_accessor :c_start
end
self.c_start = 10;
class << self
	attr_accessor :c_error
end
self.c_error = 0;

class << self
	attr_accessor :c_en_c_comment
end
self.c_en_c_comment = 8;
class << self
	attr_accessor :c_en_main
end
self.c_en_main = 10;


# line 76 "clexer.rl"

def scan(data)

    curline = 1
    data = data.unpack("c*")
    eof = data.length
    identifiers = [] 

	
# line 173 "clexer.rb"
begin
	p ||= 0
	pe ||= data.length
	cs = c_start
	ts = nil
	te = nil
	act = 0
end

# line 85 "clexer.rl"
    
# line 185 "clexer.rb"
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
# line 219 "clexer.rb"
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
		cs = 10
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
te = p+1
 begin 
		#puts "symbol(#{curline}): #{data[ts..te-1].pack("c*")}"
	 end
		end
when 6 then
# line 34 "clexer.rl"
		begin
te = p+1
 begin 
        #puts "single_lit(#{curline}): #{data[ts..te-1].pack("c*")}"
	 end
		end
when 7 then
# line 40 "clexer.rl"
		begin
te = p+1
 begin 
		#puts "double_lit(#{curline}): #{data[ts..te-1].pack("c*")}"
	 end
		end
when 8 then
# line 45 "clexer.rl"
		begin
te = p+1
		end
when 9 then
# line 50 "clexer.rl"
		begin
te = p+1
		end
when 10 then
# line 52 "clexer.rl"
		begin
te = p+1
 begin  	begin
		cs = 8
		_trigger_goto = true
		_goto_level = _again
		break
	end
  end
		end
when 11 then
# line 21 "clexer.rl"
		begin
te = p
p = p - 1; begin 
		#puts "symbol(#{curline}): #{data[ts..te-1].pack("c*")}"
	 end
		end
when 12 then
# line 27 "clexer.rl"
		begin
te = p
p = p - 1; begin 
        #puts "ident(#{curline}): #{data[ts..te-1].pack("c*")}"
        identifiers << [ts, te-1]
	 end
		end
when 13 then
# line 56 "clexer.rl"
		begin
te = p
p = p - 1; begin 
		#puts "int(#{curline}): #{data[ts..te-1].pack("c*")}"
	 end
		end
when 14 then
# line 62 "clexer.rl"
		begin
te = p
p = p - 1; begin 
		#puts "float(#{curline}): #{data[ts..te-1].pack("c*")}"
	 end
		end
when 15 then
# line 68 "clexer.rl"
		begin
te = p
p = p - 1; begin 
		#puts "hex(#{curline}): #{data[ts..te-1].pack("c*")}"
	 end
		end
when 16 then
# line 21 "clexer.rl"
		begin
 begin p = ((te))-1; end
 begin 
		#puts "symbol(#{curline}): #{data[ts..te-1].pack("c*")}"
	 end
		end
when 17 then
# line 56 "clexer.rl"
		begin
 begin p = ((te))-1; end
 begin 
		#puts "int(#{curline}): #{data[ts..te-1].pack("c*")}"
	 end
		end
# line 407 "clexer.rb"
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
# line 427 "clexer.rb"
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

# line 86 "clexer.rl"

    return identifiers

end

