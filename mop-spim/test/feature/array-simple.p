program arrays;

class arrays
begin

  function arrays;
  var a : array [3..8] of integer;
      i : integer;
  begin

    a[3] := 3;

    i := 3;
    while i < 8 do
    begin
      a[i+1] := a[i] * 2;
      i := i + 1
    end;

    i := 3;
    while i <= 8 do
    begin
      print a[i];
      i := i + 1
    end

  end

end

.
