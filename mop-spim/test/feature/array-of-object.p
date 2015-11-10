program arrays;

class widget
begin
  var  even : boolean;
      thing : integer;
end

class arrays
begin

  function arrays;
  var w : array [0..3] of widget;
      i : integer;
  begin

    i := 0;
    while i < 4 do
    begin
      w[i] := new widget;
      i := i + 1
    end;

    w[0].thing := 1;
    i := 1;
    while i < 4 do
    begin
      w[i].thing := w[i - 1].thing + 3;
      w[i].even := (w[i].thing MOD 2) = 0;
      i := i + 1
    end;

    i := 0;
    while i < 4 do
    begin
      print w[i].even;
      print w[i].thing;
      i := i + 1
    end

  end

end

.
