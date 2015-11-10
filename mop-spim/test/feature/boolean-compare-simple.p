program comparison;

class comparison
begin

  function comparison;
  var a : boolean;
      b : boolean;
      x : integer;
      y : integer;
  begin

    a :=   true;
    b :=  false;

    x :=      3;
    y :=      7;

    { we need this if statement here to prevent the optimizer from reducing
      all the code after it into just constant assignments and prints }
    if true then
      a := a
    else
      a := a;

    if x = y then
      print x
    else
      print y;

    if x < y then
      print x
    else
      print y;

    if x > y then
      print x
    else
      print y;

    if x <= x then
      print x
    else
      print y;

    if x <= y then
      print x
    else
      print y;

    if x >= x then
      print x
    else
      print y;

    if x >= y then
      print x
    else
      print y

  end

end

.
