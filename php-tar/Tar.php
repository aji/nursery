<?php

/*
   php-tar is a single PHP class that abstracts a tar archive (gzipped
   tar archives are supported as well, but only as far as PHP's
   Zlib functions). Documentation is scattered throughout the source
   file. Scoot around and have a good time.
 */

$TAR_HDR_UNPACK_FORMAT =
	'a100name/' . /* 'name' => file name */
	'a8mode/' . /* 'mode' => file mode */
	'a8uid/' . /* 'uid' => numeric uid */
	'a8gid/' . /* 'gid' => numeric gid */
	'a12size/' . /* 'size' => size in bytes */
	'a12mtime/' . /* 'mtime' => modification time */
	'a8checksum/' . /* 'checksum' => checksum */
	'a1type/' . /* 'type' => type indicator */
	'a100link/' . /* 'link' => name of linked file */
	'a6ustar/' . /* 'ustar' => UStar indicator */
	'a2uver/' . /* 'uver' => UStar version */
	'a32owner/' . /* 'owner' => owner name */
	'a32group/' . /* 'group' => group name */
	'a8major/' . /* 'major' => device major */
	'a8minor/' . /* 'minor' => device minor */
	'a155nameprefix/' /* 'nameprefix' => file name prefix */
	;
$TAR_HDR_PACK_FORMAT =
	'a100' . 'a8' . 'a8' . 'a8' . /* name, mode, uid, gid */
	'a12' . 'a12' . 'a8' . 'a1' . /* size, mtime, checksum, type */
	'a100' . 'a6' . 'a2' . 'a32' . /* link, ustar, uver, owner */
	'a32' . 'a8' . 'a8' . 'a155' /* group, major, minor, nameprefix */
	;

class TarIOPlain {

	public function __construct($filename) { $this->filename = $filename; $this->f = NULL; }

	/* low-level */

	function open($f, $m) { return fopen($f, $m); }
	function close($f) { fclose($f); }
	function read($f, $n) { return fread($f, $n); }
	function write($f, $s) { fwrite($f, $s); }

	/* high-level */

	public function start_load() {
		$this->f = $this->open($this->filename, "rb");
		return $this->f !== NULL;
	}

	public function start_save() {
		$this->f = $this->open($this->filename, "wb");
		return $this->f !== NULL;
	}

	public function end() {
		$this->close($this->f);
	}

	public function record_load() {
		return $this->read($this->f, 512);
	}

	public function record_save($r) {
		$len = strlen($r);
		$n = 0x200;

		if ($len > 0)
			$n = (($len - 1) & ~0x1ff) + 0x200;

		$this->write($this->f, str_pad($r, $n, "\0"));
	}
}

class TarIOGzip extends TarIOPlain {

	function open($f, $m) { return gzopen($f, $m); }
	function close($f) { return gzclose($f); }
	function read($f, $n) { return gzread($f, $n); }
	function write($f, $s) { return gzwrite($f, $s); }

}

class Tar {

	public function __construct() {
		$this->files = array();
	}

	public function compress_normalize($compress) {
		switch ($compress) {
		case '.gz':
			return 'TarIOGzip';
		default:
			return 'TarIOPlain';
		}
	}

	public function load($filename=NULL, $compress='.gz') {
		$compress = $this->compress_normalize($compress);
		$f = new $compress($filename === NULL ? $this->filename : $filename);

		if (!$f->start_load()) {
			trigger_error("Tar::load: Could not open $f->filename for loading");
			return FALSE;
		}

		while (($c = $this->load_one($f)) > 0) {
			/* no-op */
		}

		if ($c < 0) {
			trigger_error("Tar::load: Error when unpacking");
			$f->end();
			return FALSE;
		}

		$f->end();
		return TRUE;
	}

	public function header_sum_check($data, $refsum) {
		$sum_unsigned = 0;
		$sum_signed = 0;

		$refsum = (int)$refsum;

		for ($i=0; $i<0x200; $i++) {
			$c = ord(substr($data, $i, 1));
			if ($i >= 148 && $i < 156)
				$c = 32; /* ASCII space */
			$sum_unsigned += $c;
			$sum_signed += ($c > 127 ? $c - 256 : $c); /* XXX? */
		}

		$ok = (($refsum == $sum_unsigned) || ($refsum == $sum_signed));

		return $ok;
	}

	public function header_sum_calc(&$data) {
		$sum = 0;

		for ($i=0; $i<0x200; $i++) {
			$c = ord(substr($data, $i, 1));
			if ($i >= 148 && $i < 156)
				$c = 32; /* ASCII space */
			$sum += $c;
		}

		$data = substr($data, 0, 148) . sprintf("%06o\0 ", $sum) . substr($data, 156, 356);
	}

	public function header_read($hdr_data) {
		global $TAR_HDR_UNPACK_FORMAT;

		$hdr_info = array('ustar' => '', 'uver' => '00',
				'owner' => '', 'group' => '', 'major' => 0,
				'minor' => 0, 'nameprefix' => '');

		$hdr_data = str_pad($hdr_data, 512, "\0");

		$hdr_unpacked = unpack($TAR_HDR_UNPACK_FORMAT, $hdr_data);

		$hdr_info['checksum'] = octdec(trim($hdr_unpacked['checksum'], "\0 "));
		if (!$this->header_sum_check($hdr_data, $hdr_info['checksum'])) {
			trigger_error("Bad checksum, file maybe named {$hdr_unpacked['name']}");
			return FALSE;
		}

		/* TODO: array_map() n stuff */
		$hdr_info['name'] = trim($hdr_unpacked['name'], "\0");
		$hdr_info['mode'] = octdec($hdr_unpacked['mode']);
		$hdr_info['uid'] = octdec($hdr_unpacked['uid']);
		$hdr_info['gid'] = octdec($hdr_unpacked['gid']);
		$hdr_info['size'] = octdec($hdr_unpacked['size']);
		$hdr_info['mtime'] = octdec($hdr_unpacked['mtime']);
		/*$hdr_info['checksum'] = (we already did this) */
		$hdr_info['type'] = trim($hdr_unpacked['type'], "\0");
		$hdr_info['link'] = trim($hdr_unpacked['link'], "\0");
		$hdr_info['ustar'] = trim($hdr_unpacked['ustar'], "\0");

		if ($hdr_info['ustar'] == 'ustar') {
			$hdr_info['uver'] = trim($hdr_unpacked['uver'], "\0");
			$hdr_info['owner'] = trim($hdr_unpacked['owner'], "\0");
			$hdr_info['group'] = trim($hdr_unpacked['group'], "\0");
			$hdr_info['major'] = octdec($hdr_unpacked['major']);
			$hdr_info['minor'] = octdec($hdr_unpacked['minor']);
			$hdr_info['nameprefix'] = trim($hdr_unpacked['nameprefix'], "\0");
		}

		return $hdr_info;
	}

	public function header_pack($hdr_info) {
		global $TAR_HDR_PACK_FORMAT;

		$hdr_data = str_pad(pack($TAR_HDR_PACK_FORMAT,
				$hdr_info['name'],
				sprintf('%07o', $hdr_info['mode']),
				sprintf('%07o', $hdr_info['uid']),
				sprintf('%07o', $hdr_info['gid']),
				sprintf('%011o', $hdr_info['size']),
				sprintf('%011o', $hdr_info['mtime']),
				'        ',
				$hdr_info['type'],
				$hdr_info['link'],
				$hdr_info['ustar'],
				$hdr_info['uver'],
				$hdr_info['owner'],
				$hdr_info['group'],
				sprintf('%08o', $hdr_info['major']),
				sprintf('%08o', $hdr_info['minor']),
				$hdr_info['nameprefix']),
			512, "\0");

		$this->header_sum_calc($hdr_data);

		return $hdr_data;
	}

	public function load_data($f, $size) {
		$s = '';

		while ($size > 0) {
			$rec = $f->record_load();
			if ($size < 512)
				$s .= substr($rec, 0, $size);
			else
				$s .= $rec;
			$size -= 512;
		}

		return $s;
	}

	/* 0 indicates finish, <0 indicates error, >0 indicates continue */
	public function load_one($f) {
		$hdr_data = $f->record_load();
		if (strlen($hdr_data) < 0x200 || $hdr_data == str_repeat("\0\0\0\0\0\0\0\0", 64))
			return 0;

		$file = $this->header_read($hdr_data);
		if ($file === FALSE)
			return -1;

		if ($file['type'] != 0)
			return 1;

		$file['data'] = $this->load_data($f, $file['size']);
		unset($file['size']); /* size is implicit in data */
		$this->files[$file['name']] = $file;
		unset($file['name']); /* name is array key */

		return 1;
	}

	public function save($filename, $compress='.gz') {
		$compress = $this->compress_normalize($compress);
		$f = new $compress($filename === NULL ? $this->filename : $filename);

		if (!$f->start_save()) {
			trigger_error("Tar::save: Could not open $f->filename for saving");
			return FALSE;
		}
	
		foreach ($this->files as $name => $file) {
			$c = $this->save_one($f, $name, $file);

			if ($c < 0) {
				trigger_error("Tar::save: Error when saving. $f->filename is probably jacked up");
				$f->end();
				return FALSE;
			}
		}

		$f->record_save('');
		$f->record_save('');

		$f->end();
		return TRUE;
	}

	/* nonzero indicates error */
	public function save_one($f, $name, $file) {
		$file['size'] = strlen($file['data']);
		$file['name'] = $name;
		$hdr_data = $this->header_pack($file);
		if ($hdr_data === FALSE)
			return -1;
		unset($file['size']); /* size is implicit in data */
		unset($file['name']); /* name is array key */

		$f->record_save($hdr_data);
		$f->record_save($file['data']);

		return 0;
	}

}

if (isset($argv)) {
	$tar = new Tar();

	if (count($argv) > 1)
		$tar->load($argv[1]);
	else
		$tar->load("/dev/stdin");

	foreach ($tar->files as $name => $file)
		echo $name . "\n";

	$tar->save("somefile.tar.gz", ".gz");
}

?>
