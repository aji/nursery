program boringClass;

class subClass
begin

  var  x : integer;
       y : fancyClass;

end

class fancyClass
begin

  var  x : integer;
       y : boolean;
       z : array [1..5] of array [1..6] of subClass;
       w : subClass;

  function fancyFunction(a: subClass; b: integer): integer;
  var tmp : integer;
  begin
    x := 2 * a.x;
    tmp := 3
  end

end

class boringClass
begin

  var  a : fancyClass;
       b : integer;

  function boringClass;
  begin
    b := 3
  end

end

.
