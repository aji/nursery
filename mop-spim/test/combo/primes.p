program primes;

class primes
begin

  function primes;
  var   a, i : integer;
       found : boolean;
  begin

    { prints all prime numbers up to 100 }

    a := 2;

    while a < 100 do
    begin
      i := 2;
      found := false;

      while (i < a) AND NOT found do
      begin
        if a MOD i = 0 then
          found := true
        else
          i := i + 1
      end;

      if NOT found then
        print a
      else
        a := a;

      a := a + 1
    end

  end

end

.
