#!/usr/bin/python3

# testbox.py -- Copyright (C) 2013 Alex Iadicicco
# 
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject
# to the following conditions:
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import subprocess
from subprocess import PIPE
import json
import shlex
import sys, os

class TestBox:
    def __init__(self):
        self.tests_by_name = {}
        self.test_description = {}
        self.tests = []
        self.run = None
        self.outtype = None

    def load_tests(self, fname):
        try:
            with open(fname, 'r') as f:
                info = json.load(f)
        except (ValueError, FileNotFoundError) as e:
            return e.args[-1]

        try:
            self.run = info['run']
            if type(self.run) is not type([]):
                if type(self.run) is not type(''):
                    raise TypeError('"run" should be a string or array')
                self.run = shlex.split(self.run)

            self.outtype = info['output'].lower()

            if not self.outtype in ['exit', 'stdout']:
                raise TypeError('"output" should be "exit" or "stdout"')

            if type(info['tests']) is type({}):
                self.named_tests = True
                kv = sorted(info['tests'].items())
            elif type(info['tests']) is type([]):
                self.named_tests = False
                kv = [('test'+str(k),v) for k,v in enumerate(info['tests'])]
            else:
                raise TypeError('"tests" should be an array or object')

            for k,v in kv:
                if type(v) is not type([]) or len(v) != 3:
                    raise TypeError('tests should be 3 element arrays')
                self.add_test(k, *v)

        except (KeyError, TypeError) as e:
            return 'Parsing input failed: ' + e.args[-1]

        self.loaded = True
        return None

    def add_test(self, name, args, stdin, outv):
        if name in self.tests_by_name:
            raise KeyError('already have test with name ' + name)

        desc = name
        if not self.named_tests:
            desc = ' '.join(args + [stdin])
        desc = desc.replace('\n', '')

        # args validation
        if type(args) is not type([]):
            if type(args) is not type(''):
                raise TypeError('args for test ' + name + ' should be '
                                + 'a string or array')
            args = shlex.split(args)

        # stdin valdiation
        if type(stdin) is not type(''):
            raise TypeError('stdin for test ' + name + ' should be a string')

        # outv validation
        if self.outtype == 'exit':
            outv = int(outv) # will raise TypeError if can't convert
        elif self.outtype == 'stdout':
            if type(outv) is not type(''):
                raise TypeError('output for test ' + name + ' should be string')
        else:
            raise Exception('no outtype selected yet')

        test = name, args, stdin, outv
        self.tests_by_name[name] = test
        self.test_description[name] = desc
        self.tests.append(test)

    def reset_totals(self):
        self.tests_passed = []
        self.tests_failed = []
        self.tests_timedout = []

    def print_summary(self):
        tot = len(self.tests)
        np = len(self.tests_passed)
        nf = len(self.tests_failed)
        nt = len(self.tests_timedout)

        print('TEST SUMMARY:')
        print('     Passed {:4}/{}, {:5.1f}%'.format(np, tot, 100.0*np/tot))
        print('     Failed {:4}/{}, {:5.1f}%'.format(nf, tot, 100.0*nf/tot))
        print('  Timed out {:4}/{}, {:5.1f}%'.format(nt, tot, 100.0*nt/tot))

        def wrap(s, x, w=80, nl='  '):
            fresh = True
            for name in x:
                p = self.test_description[name]
                if len(s) + len(p) > w:
                    print(s)
                    s, fresh = nl, True
                s, fresh = s + ' ' + p, False
            if not fresh:
                print(s)

        if nf > 0:
            wrap('*** Failed tests:', self.tests_failed)
        if nt > 0:
            wrap('*** Timed out tests:', self.tests_timed_out)

    def run_tests(self):
        if len(self.tests) == 0:
            print('No tests to run!')
            return
        if None in [self.run, self.outtype]:
            print('Missing parameters!')
            return

        self.reset_totals()
        for test in self.tests:
            self.run_test(*test)

    # status: 1=pass, 0=fail, -1=timeout
    def test_status(self, name, status, loud=False, silent=False):
        desc = self.test_description[name]
        if status == 1:
            self.tests_passed.append(name)
            line = '[  PASS  ] {}'.format(desc)
        if status == 0:
            loud = True
            self.tests_failed.append(name)
            line = '[  FAIL  ] {}'.format(desc)
        if status == -1:
            loud = True
            self.tests_timedout.append(name)
            line = '[TIMEOUT!] {}'.format(desc)
        if loud and not silent:
            print(line)

    def run_test(self, name, args, stdin, outv):
        args = self.run + args

        sub = subprocess.Popen(args, stdin=PIPE, stdout=PIPE)
        stdout, stderr = sub.communicate(stdin.encode())

        if self.outtype == 'exit':
            status = 1 if sub.returncode == outv else 0
        else:
            status = 1 if stdout == outv.encode() else 0

        self.test_status(name, status)

if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] in ['--help', '-h']:
        print('Usage: {} INPUT'.format(sys.argv[0]))
        print('Runs the unit tests described by INPUT')
        sys.exit(1)
    box = TestBox()
    error = box.load_tests(sys.argv[1])
    if error is not None:
        print('Error loading {}: {}'.format(sys.argv[1], error))
        sys.exit(1)
    box.run_tests()
    box.print_summary()
    sys.exit(0)
else:
    print('testbox cannot be loaded as a module yet')
    sys.exit(1)
