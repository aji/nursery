program arrays;

class arrays
begin

  function arrays;
  var m : array [1..10] of array [1..10] of integer;
      i, j : integer;
  begin

    { m is a multiplication table! }

    i := 1;
    while i <= 10 do
    begin
      j := 1;
      while j <= 10 do
      begin
        m[i][j] := i * j;
        j := j + 1
      end;
      i := i + 1
    end;

    { print some multiplications! }

    print m[2][7];
    print m[3][5];
    print m[1][8]

  end

end

.
