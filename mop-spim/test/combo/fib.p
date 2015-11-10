program fib;

{ prints first 10 fibonacci numbers }

{ this test case covers:
    basic control
    arrays
    function calls
    pass-by-reference }

class fib
begin

  function fibStep(var a, b, c: integer): boolean;
  var t : integer;
  begin
    c := a + b;

    fibStep := true
  end;

  function fib;
  var   f : array [0..10] of integer;
        i : integer;
      ign : boolean;
  begin

    f[1] := 0;
    f[2] := 1;

    i := 1;
    while i < 9 do
    begin
      ign := fibStep(f[i], f[i+1], f[i+2]);
      i := i + 1
    end;

    i := 1;
    while i <= 10 do
    begin
      print f[i];
      i := i + 1
    end

  end

end

.
