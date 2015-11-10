# Paren

**Paren** is a very simple lisp-like scripting language, written as an exercise
in scripting language implementation. It borrows heavily from common lisp and
emacs lisp, but is still very much its own dialect. Written in C, bindings from
most languages are possible.

## Language

### Syntax

Being lisp-based, it should come as no surprise that Paren uses S-expressions
to denote code and data. **S-expressions** are parenthesis-bounded lists using
spaces to separate the elements. The following code is a list of the first 6
Fibonacci numbers:

    (1 1 2 3 5 8)

And an example piece of code to (inefficiently) compute the Fibonacci sequence
might look as follows:

    (defun fib (n)
      (if (<= n 1) 1
        (+ (fib (- n 1))
           (fib (- n 2)))))

As you can see, both code and data are written using the same list syntax. With
very few exceptions, these are the only syntactic components of the language.
One piece of syntactic sugar is provided, the quote. Writing `'foo` is
equivalent to writing `(quote foo)`.

Lists can contain **atoms** or **numbers**. An "atom" is what other languages
might call an "identifier", however the definition is a bit looser to allow for
things like symbols. A "number" is exactly what it sounds like.

Comments are to-end-of-line and start with a semicolon.

### Evaluation

At the heart of the language is evaluation. What it means to "evaluate" an
object defines how the language works. In many cases, evaluating an object
simply results in the same object. For example, evaluating the number `35` will
result in `35` again.

Evaluating an atom will perform a symbol lookup for that atom. For example,
evaluation of the atom `+` will (in most cases) result in the built-in function
that performs addition.

Evaluating a list is where the real action is. The first element of the list is
evaluated. If it is a "special form", the special form is invoked with the rest
of the list untouched as its arguments. Otherwise, the rest of the list is
evaluated in turn, and the first argument is called with this new list as its
arguments.

As an example, we will work through the evaluation of the list `(+ 2 x)`.
First, `+` is evaluated. This results in a pointer to the built-in function for
addition. Since this is not a special form, evaluation continues. `2` is
evaluated, resulting in `2` again. Finally, `x` is evaluated. Let's pretend
that `x` has a value of `5` in this scope. Then, the built-in function is
called with the arguments `(2 5)`. It adds them, and `7` is returned.

### Special Forms

A **special form** is an internal language extension that receives its
arguments untouched. This allows correct implementation of certain language
features, listed below:

#### `quote`

Simply returns its argument. For example, `(quote 3)` is just `3`, and `(quote
y)` is just `y`. This is useful if you want to pass a raw atom or raw list to a
function, without them being evaluated as identifiers or function invocations.

#### `let`

Defines a new lexical scope. Can be understood as assigning local variables.
Syntactically, it looks like this:

    (let ((name val)
          (name val)
          ...)
      body...)

The `val` are expressions that will be evaluated in the containing scope (e.g.
not affected by the current sequence of assignments). In the new scope, the
result of these evaluations will be bound to the given names.

The `body` is a sequence of expressions to be evaluated in the new scope.

#### `if`

Tests a condition, and then evaluates one of its arguments for a particular
case. Syntactically, it looks like this:

    (if cond
      true-branch
      false-branch)

`cond` is evaluated. If it has a value of `nil`, then `false-branch` is
evaluated. Otherwise, `true-branch` is evaluated.

#### `setq`

`(setq name val)` is equivalent to `(set (quote name) val)`.

#### `lambda`

Creates a closure, which is a callable object that captures ("closes over") its
containing scope. Syntactically, it looks like this:

    (lambda (args...)
       body...)

Where `args` is a list of variable names for arguments to the lambda to be
bound to, and `body` is a sequence of expressions to be evaluated in the new
scope. The last expression's result becomes the return value of the closure.

As a simple example, the following code

    ((lambda (n) (+ n 2)) 3)

will evaluate to `5`. As a less simple example, the following code

    (defun make-adder (a)
      (lambda (b)
        (+ a b)))

    (let ((plus2 (make-adder 2)))
      (print (plus2 3)))

will print `5`.

#### `defun`

`(defun name (args..) body..)` is equivalent to
`(set (quote name) (lambda (args..) body..))`

#### `progn`

Evaluates each of its arguments in sequence, and evaluates to the value of the
last one.

### Built-in Functions

* `(print ...)` prints its arguments, one to a line.

* `(set NAME VAL)` assigns `VAL` to `NAME` in the global scope.

* `(cons A B)` creates a cons cell from `A` and `B`.

* `(car CELL)` and `(cdr CELL)` fetch the car and cdr fields respectively from
  the given cons cell, or `nil` if the value is not a cons cell.  Compositions
  up to 4 levels are provided as well, where application is from right to left.
  For example, `(caddr n)` is equivalent to `(car (cdr (cdr n)))`

* `(list ...)` creates a list from its arguments.

* `(map FN LIST)` creates a new list generated by applying `FN` to each element
  of `LIST`.

* `(filter FN LIST)` creates a new list containing only those elements of
  `LIST` for which `FN` returns non-nil.

* `(reduce FN LIST VAL)` returns the result of repeatedly applying `FN` to
  some reduction value and elements of list, in a left-to-right order.
  For example, `(reduce + '(1 2 3) 0)` will compute `(+ 0 1)` to obtain 1,
  then `(+ 1 2)` to obtain 3, then `(+ 3 3)` to obtain 6, which becomes the
  result of the whole function. If `VAL` is omitted, then `FN` is called
  without any arguments to determine the initial accumulator value.

* `(apply FN ARGS)` call the function `FN` with the elements of the list `ARGS`
  as its arguments.

* `(nilp N)` and `(listp N)` test whether `N` is nil or a list, respectively.

* `(eq A B)` performs a permissive equality test on the two objects (using the
  structure of the objects rather than their identity). For example,
  `(eq '(1 2) '(1 2))` will be `t` because the lists have the same elements
  despite not being the same object in memory.

* `(not X)` negates the logical sense of `X` (equivalent to testing if `X` is
  nil).

* `(or ITEMS..)` and `(and ITEMS..)` perform logical "or" and logical "and"
  on all of their arguments.

* `+` and `*` perform addition and multiplication respectively. With no
  arguments, these return the additive and multiplicative identities
  respectively, i.e. 0 and 1.

* `-` performs unary negation with 1 argument, and subtraction of its arguments
  in all other cases. Note that the case `(- a b c)` will result in "a - b -
  c".

* `>`, `<`, `>=`, and `<=` perform numeric comparison of their arguments.

* `hash-new`, `hash-put`, and `hash-get` allow the creation and use of hash
  tables. Quoted atoms should be provided as keys, e.g. `(hash-put tab 'key
  val)`, or `(hash-get tab 'key)`.

## C API

While `paren.h` is the most up-to-date specification of the API, a short
outline is provided here.

### Reference counting

Imagine a world where nobody had more than one copy of a value, and functions
sent values around by either handing off their last copy if they were done with
it, or making a new copy. This is what reference counting attempts to simulate,
by allowing multiple references to the same object, but keeping track of the
count so as to simulate clones and deletions. The lack of mutation operations
makes this analogy work.

`INCREF` and `DECREF` macros are provided to help with reference counting.
The rules are,

* Values passed as arguments have 1 reference associated with the argument.
  This means that if an argument goes out of scope within the function, it
  should be `DECREF`'d. This also means that if you are passing a value to
  a function that you intend to retain a reference to, you should wrap the
  parameter in an `INCREF` to create a new reference for the callee.

* Values returned from functions have 1 reference associated with the return.
  This applies to constructors as well. This means if you are discarding a
  result you should `DECREF` it. This also means that if you are returning
  a value you intend to hold on to past the end of the function (such as in
  some sort of global context) you should `INCREF` it to create a new
  reference for a caller.

* Err on the side of extra references for now. Eventually a garbage collector
  that determines reachability will be added as well. It's better to create
  references that go out of scope than to delete references that are still
  in use.

> *Note that the `CAR` and `CDR` accessor macros do not obey this protocol. (As
> a result, neither do their compositions.) To alleviate this, `XCAR` and
> `XCDR` macros have been provided, as well as compositions (`XCADR`, `XCDDAR`,
> etc).  These discard the reference to their argument and create a reference
> for the return value. It is a kind of "destructive" access, akin to Rust's
> concept of "moving" a value.*

A good analogy for the reference protocol across function calls might be
a function for making a pizza:

    Node *make_pizza(Node *dough, Node *sauce, Node *cheese);

The idea here is that the ingredients are consumed in the process. If you have
some cheese you want to make a pizza with, but don't want it to be destroyed,
you can make a copy of it beforehand and give `make_pizza` the copy. Naturally
this will end up with a lot of copying and deleting.

In this way, `INCREF` functions like "alloc/clone", and `DECREF` functions like
"delete". It takes a bit of getting used to, but you can mnemonically think of
the above rules like `strdup` and `free`:

* Every argument to your function will be `strdup`'d and you are responsible
  for holding on to it or `free`ing it.

* Return values from your function will eventually be `free`'d by the caller,
  so don't give them a value unless you have `strdup`'d it.

### `Node`

The main "object" type. Nodes are either atoms, cons cells, or
some internally-relevant object (such as pointers to built-in functions or
closures)

Global constants `NIL` and `T` correspond to their counterparts in the
language. The `COND` macro can be used to convert a boolean expression to `T`
or `NIL`.

Functions `INTERNAL`, `BUILTIN`, `SPECIAL`, `ATOM`, `INT`, and `CONS` are
provided to make constructing nodes easier. `LIST1`, `LIST2`, etc. all the way
to `LIST6` assist in list construction. A generic `LIST` function is provided
as well which takes a variable amount of arguments and constructs a list from
them. `NIL` or `NULL` should be used to signal the end.

`IS_NIL` and `IS_CONS` provide simple ways to test nodes.

`CAR` and `CDR` can be used to safely retrieve the car and cdr fields of a cons
cell, returning `nil` otherwise. Compositions of these functions exist as well,
e.g. `CADDR` and `CDDAAR`, up to 4 levels. The operations are applied from
right to left, just as with mathematical function composition notation. For
example, `CADR(n)` means `CAR(CDR(n))`, or `(CAR ° CDR)(n)` where `°` denotes
function composition.

### `ReReadContext`

The re-entrant reader context. The re-entrant reader is specially designed to
put the programmer in control of how input is loaded.

Use `pa_reread_init` to initialize a context.

Use `pa_reread_put` to put data into the buffer, and `pa_reread_eof` to signal
the end of the input stream.

`pa_reread_empty` and `pa_reread_finished` can be used to check whether the
read stack is empty (indicating we have no more open structures waiting to be
finished), and whether the reader has reached EOF and is not waiting for any
more data.

`pa_reread` returns the next node it finds in the input buffer, or `NULL` if it
has reached the end of the input.

### `HashMap`

A map of string keys to `Node` values. Originally this was a generic hash
table, but restricting values to `Node` makes the data structure safer, and
allows better integration with the rest of the language.

Use `pa_hash_new`, `pa_hash_put`, and `pa_hash_get` to interface with the hash
table. These functions should do exactly what their names imply, and shouldn't
be a surprise to anybody who has used a hash table before.

### `EvalContext`

Evaluation context. This concerns things like the global variable table,
exception handling (eventually), and so on.

Use `pa_eval_init` to initialize your evaluation context. On the first run,
this also initializes the global evaluation context (things like
`pa_builtins`), but this work is not repeated on subsequent runs.

Use `pa_apply` to call a callable object with the given argument list. This is
the preferred way to invoke user callbacks.

Use `pa_eval` to evaluate a node. What "evaluate" means depends on the language
semantics. This is the heart of the language.

### Miscellaneous

`pa_print` will print a `Node`'s representation to `stdout`, without a trailing
newline. Useful for debugging, but not much else.
