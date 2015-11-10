program method;

class method
begin

  var property : integer;

  function triple(var x: integer): integer;
  begin
    x := 3 * x
  end;

  function method;
  var a : array [0..1] of integer;
      ign : integer;
  begin
    a[0] := 4;
    ign := triple(a[0]);
    print a[0];

    property := 6;
    ign := triple(property);
    print property
  end

end

.
