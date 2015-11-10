program control;

class control
begin

  function control;
  var a : integer;
  begin

    a := 0;

    { prints everything in range 1 to 10 that is divisible by either 3 or 5 }

    while a < 10 do
    begin
      a := a + 1;

      if a MOD 3 = 0 then
        print a
      else
        if a MOD 5 = 0 then
          print a
        else
          a := a

    end

  end

end

.
