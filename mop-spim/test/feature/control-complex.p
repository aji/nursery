program control;

class control
begin

  function control;
  var a : integer;
      i : integer;
  begin

    { prints all prime numbers up to 100 }

    a := 2;

    while a < 100 do
    begin
      i := 2;

      while i < a do
      begin
        if a MOD i = 0 then
          i := a + 1
        else
          i := i + 1
      end;

      if i = a then
        print a
      else
        a := a;

      a := a + 1
    end

  end

end

.
