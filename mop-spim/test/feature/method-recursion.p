program method;

class method
begin

  function factorial(n: integer): integer;
  begin
    if n < 2 then
      factorial := 1
    else
      factorial := n * this.factorial(n - 1)
  end;

  function method;
  var x : integer;
  begin
    x := factorial(4);
    print x;

    x := factorial(3);
    print x
  end

end

.
