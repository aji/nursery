program ArrayOfClass;

class Widget
begin
  var x : integer;
      y : integer;

  function doThing(a: integer): integer;
  begin
    a := x + y
  end
end

class ArrayOfClass
begin
  var x : integer;
      widgets : array [1..3] of Widget;

  function ArrayOfClass;
  begin
    x := widgets[1].doThing(3)
  end
end

.
