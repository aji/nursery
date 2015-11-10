/* lookup tables */
readline_keycode_to_local_table = [ /* lowercase ASCII, with fallthrough, -1 forces 0 */
	/* 0x00 1    2    3    4    5    6    7    8    9    a    b    c    d    e    f */
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  -1,  -1,  -1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,0x02,   0,0x06,   0,   0,   0,   0,   0,   0,0x04,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

	/* 0x40 */
	   0,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,   0,   0,   0,   0,   0,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

	/* 0x80 */
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,0x3b,0x3d,0x2c,0x2d,0x2e,0x2f,

	/* 0xc0 */
	0x60,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,0x5b,0x5c,0x5d,0x27,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
];
readline_local_to_control_table = [ /* no fallthrough */
	/* 0x00 1    2    3    4    5    6    7    8    9    a    b    c    d    e    f */
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

	/* 0x40 */
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,   0,   0,   0,   0,   0,
];
readline_local_to_uppercase_table = [ /* with fallthrough */
	/* 0x00 1    2    3    4    5    6    7    8    9    a    b    c    d    e    f */
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,0x22,   0,   0,   0,   0,0x3c,0x5f,0x3e,0x3f,
	0x29,0x21,0x40,0x23,0x24,0x25,0x5e,0x26,0x2a,0x28,   0,0x3a,   0,0x2b,   0,   0,

	/* 0x40 */
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,0x7b,0x7c,0x7d,   0,   0,
	0x7e,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
	0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,   0,   0,   0,   0,   0,
];
	
function readline_keycode_to_local(keycode) {
	var v = readline_keycode_to_local_table[keycode];
	return v == -1 ? 0 : (v ? v : keycode);
}
function readline_local_to_control(loc) {
	return readline_local_to_control_table[loc];
}
function readline_local_to_uppercase(loc) {
	var v = readline_local_to_uppercase_table[loc];
	return v ? v : loc;
}

function readline_htmlify(s) {
	return $('<div/>').text(s).html();
}

$.fn.readline = function(arg1, arg2) {
	var theprompt = '';
	var cb;
	var line = '';
	var kill = '';
	var cursor = 0;
	var entry_box = $(this[0]);
	var entry_cur;
	var keys = Array(256);
	var fn = Array(128);
	var history = [''];
	var history_pos = 0;
	var blinker;
	var blink_time = 400;

	if (arg2 == undefined) {
		cb = arg1;
	} else {
		theprompt = arg1;
		cb = arg2;
	}

	splice = function(s, to, from) {
		var before = line.substring(0, to);
		var after = line.substring(from);
		line = before + s + after;
	};

	fn_echo = function(k) {
		splice(String.fromCharCode(k), cursor, cursor);
		cursor++;
	};

	fn_enter = function(k) {
		history[history.length-1] = line;
		history_pos = history.length;
		history.push('');
		cb(line);
		line = '';
	};

	fn_history_move = function(delta) {
		return function(k) {
			history[history_pos] = line;
			history_pos += delta;
			if (history_pos < 0)
				history_pos = 0;
			if (history_pos >= history.length)
				history_pos = history.length - 1;
			line = history[history_pos];
			cursor = line.length;
		};
	};

	for (i=0x00; i<0x80; i++)
		fn[i] = function(k) {};
	for (i=0x20; i<0x7f; i++)
		fn[i] = fn_echo;

	fn[0x01] = function(k) { cursor = 0; }; /* ^A */
	fn[0x02] = function(k) { cursor--; }; /* ^B */
	fn[0x04] = function(k) { splice('', cursor, cursor + 1); }; /* ^D */
	fn[0x05] = function(k) { cursor = line.length; }; /* ^E */
	fn[0x06] = function(k) { cursor++; }; /* ^F */
	fn[0x08] = function(k) { splice('', cursor - 1, cursor); cursor--; }; /* ^H, or backspace */
	fn[0x09] = fn_echo; /* ^I, or tab TODO: completion? */
	fn[0x0a] = fn_enter; /* ^J */
	fn[0x0b] = function(k) { kill = line.substring(cursor); splice('', cursor, line.length); }; /* ^K */
	fn[0x0d] = fn_enter; /* ^M, or enter */
	fn[0x0e] = fn_history_move(1); /* ^N */
	fn[0x10] = fn_history_move(-1); /* ^P */
	fn[0x15] = function(k) { kill = line.substring(0, cursor); splice('', 0, cursor); }; /* ^U */
	fn[0x19] = function(k) { splice(kill, cursor, cursor); cursor += kill.length; }; /* ^Y */

	blink_up = function() {
		entry_cur.addClass('cursor');
		entry_cur.removeClass('cursor-blink');
		window.clearTimeout(blinker);
		blinker = window.setTimeout(blink_down, blink_time);
	};

	blink_down = function() {
		entry_cur.removeClass('cursor');
		entry_cur.addClass('cursor-blink');
		window.clearTimeout(blinker);
		blinker = window.setTimeout(blink_up, blink_time);
	};

	box_update = function() {
		var before = line;
		var cur = '&nbsp';
		var after = '';
		var p = $('<span/>').addClass('prompt').text(theprompt);

		if (cursor < line.length) {
			before = line.substring(0, cursor);
			cur = readline_htmlify(line.charAt(cursor));
			after = line.substring(cursor + 1);
		}

		before = readline_htmlify(before);
		after = readline_htmlify(after);
		entry_cur = $('<span/>').html(cur);
		blink_up();

		if (keys[0x10]) entry_cur.addClass('shift');
		if (keys[0x11]) entry_cur.addClass('ctrl');

		entry_box.empty();
		entry_box.append(p, before, entry_cur, after);
	};

	$('body').keydown(function(e) {
		var v = readline_keycode_to_local(e.which);
		keys[e.which] = true;

		if (keys[0x11]) { /* control */
			v = readline_local_to_control(v);
		} else if (keys[0x10]) { /* shift */
			v = readline_local_to_uppercase(v);
		}

		fn[v](v);
		if (cursor < 0) cursor = 0;
		if (cursor > line.length) cursor = line.length;

		e.preventDefault();
		box_update();
	});

	$('body').keyup(function(e) {
		keys[e.which] = false;
		box_update();
	});

	box_update();
};
