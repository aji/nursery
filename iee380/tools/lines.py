#!/usr/bin/python
# lines.py -- collecting line counts
# Copyright (C) 2013 Alex Iadicicco

import getopt
import subprocess
import sys, os
import re
import shlex

FMT_LINECOUNT = '{:>50} {:>8} {:>8} {:>8}'

def error(line):
    sys.stderr.write(line+'\n')
def do_print(line):
    print(line)
def do_silent(line):
    pass
info = do_print
debug = do_silent

def system(cmd):
    return subprocess.check_output(shlex.split(cmd))

class FileStats:
    RE_COMMENT = re.compile('/\\*.*?\\*/', re.S)
    RE_EOLCOMMENT = re.compile('\s*//.*\n') # lone to-eol comments
    RE_BLANKS = re.compile('\n\\s*')
    RE_LONEBRACE = re.compile('\n[ \n\t{}]*')

    def __init__(self, fname):
        self.fname = fname
        with open(self.fname, encoding='latin_1') as f:
            all_text = f.read()

        all_lines = all_text.split('\n')

        all_source = FileStats.RE_COMMENT.sub('', all_text)
        all_source = FileStats.RE_EOLCOMMENT.sub('', all_source)
        all_source = FileStats.RE_BLANKS.sub('\n', all_source)
        all_source = FileStats.RE_LONEBRACE.sub('\n', all_source)
        all_sloc = all_source.split('\n')

        all_comments = '\n'.join(FileStats.RE_COMMENT.findall(all_text))
        all_comments += '\n'.join(FileStats.RE_EOLCOMMENT.findall(all_text))
        all_comments = all_comments.split('\n')

        self.line_count = len(all_lines)
        self.sloc_count = len(all_sloc)
        self.comment_count = len(all_comments)

        debug(FMT_LINECOUNT.format(self.fname.decode(), self.line_count,
              self.sloc_count, self.comment_count))

def stats_init(stats, repo):
    if not os.path.isdir(repo):
        error(repo + ' is not a directory!')
        return False

    if repo[-1] == '/':
        repo = repo[:-1]

    stats['repo_path'] = repo
    stats['repo_name'] = os.path.basename(repo)
    os.chdir(repo)

    if not os.path.exists('.git'):
        error(repo + ' is not a git repository!')
        return False

    return True

def stats_collect(repo):
    stats = {}

    if not stats_init(stats, repo):
        return False

    info('{} ({}):'.format(stats['repo_name'], stats['repo_path']))

    files = system('git ls-files -z').split(b'\0')

    stats['total_lines'] = 0
    stats['total_sloc'] = 0
    stats['total_comment'] = 0

    debug(FMT_LINECOUNT.format('NAME', 'LINES', 'SLOC', 'COMMENT'))
    for fname in files:
        if not fname.endswith(b'.c') and not fname.endswith(b'.h'):
            continue
        f = FileStats(fname)
        stats['total_lines'] += f.line_count
        stats['total_sloc'] += f.sloc_count
        stats['total_comment'] += f.comment_count
    debug(FMT_LINECOUNT.format('TOTAL', stats['total_lines'],
          stats['total_sloc'], stats['total_comment']))

    info('** Lines of code: {}'.format(stats['total_lines']))
    info('** Source lines of code: {}'.format(stats['total_sloc']))
    info('** Comment lines: {}'.format(stats['total_comment']))

    print('{repo_name},{total_lines},{total_sloc},{total_comment}'.format(**stats))

    return True

def usage(text=None, err=1, f=sys.stderr, argv0=sys.argv[0]):
    line = lambda s: f.write(s+'\n')
    if text is not None:
        line(text)
        line('use {} -h for more info'.format(argv0))
    else:
        line('usage: {} [-q] -r <repo>'.format(argv0))
        line('options:')
        line('  -h         show this help')
        line('  -q         be quiet (only print stats at the end)')
        line('  -v         be verbose')
        line('  -r <repo>  collect stats on <repo> (must be git)')
    return err

def main(argv):
    global info, debug
    repo = None

    try:
        opts, args = getopt.gnu_getopt(argv, 'hqvr:')
    except getopt.GetoptError as err:
        return usage(str(err))

    if len(args) > 1:
        return usage('extra arguments')

    for opt, arg in opts:
        if opt == '-q':
            info = do_silent
            debug = do_silent
        if opt == '-v':
            info = do_print
            debug = do_print
        if opt == '-r':
            repo = arg
        if opt == '-h':
            return usage(None, 0, sys.stdout)

    if repo is None:
        return usage('missing required option -r')

    if not stats_collect(repo):
        return 1

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
