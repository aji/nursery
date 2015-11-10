
                        CSE 440, Project 3

                          Alex Iadicicco
                              shmibs


COMPILING

   Run `make' in the root directory. The output file will be called
   a.out.


USAGE

   The program accepts a program source in standard input, and outputs
   a compiled program as SPIM assembly to standard output. Debug
   information, such as a dump of the various CFGs, is printed to
   standard error.

   A shell session for compiling and running the 'print' feature test
   might look like this:

     $ ./a.out < test/feature/print.p > print.s
     
     printTest.printTest:
       block #0:
         a <- 3
         print 3
     
     $ spim -file print.s
     SPIM Version 8.0 of January 8, 2010
     Copyright 1990-2010, James R. Larus.
     All Rights Reserved.
     See the file README for a full copyright notice.
     Loaded: /usr/lib/spim/exceptions.s
     3
     $

   (Notice that although the source says 'print a', the optimizer has
   helpfully noticed that 3 will be printed, and rewrites the print
   instruction to do without 'a' entirely. This is deemed acceptable,
   however, as the purpose of this test is just to test that
   __builtin_print works correctly.)

   A helper script called mop_spim.sh, ("mop" for "mini-object pascal") has
   been provided which invokes the compiler and SPIM in a single step:

     $ ./mop_spim.sh test/feature/print.p
     
     printTest.printTest:
       block #0:
         a <- 3
         print 3
     
     SPIM Version 8.0 of January 8, 2010
     Copyright 1990-2010, James R. Larus.
     All Rights Reserved.
     See the file README for a full copyright notice.
     Loaded: /usr/lib/spim/exceptions.s
     3
     $


FEATURES

   This compiler supports, among other things, the following features:

     o  Arithmetic (+, -, *, /, MOD)

     o  Boolean logic (AND, OR, NOT, <, >, <=, >=, =, <>)

     o  Function-local variables

     o  Object variables

     o  Arrays, with arbitrary index ranges and nesting. Arrays of objects
        are also possible.

     o  Object constructors

     o  Class inheritance

     o  Pass-by-reference parameters (no passing local variables or
        arguments, or anything that might not have an address, however)

     o  Structural type equivalence

     o  Control flow (IF and WHILE)

     o  Rudimentary output (PRINT)

   See the tests in the test/combo/ directory for combination tests that
   exercise groups of these features all together.


TESTS

   test/feature/ - These tests are designed to test individual features
       of the compiler.

   test/combo/ - These tests are designed to test many features of the
       compiler at the same time by performing non-trivial tasks.

   test/old/ - These are old tests that were used in the past for testing
       various optimizer features.


