program arithmetic;

class arithmetic
begin

  function arithmetic;
  var a : integer;
      b : integer;
      c : integer;
  begin

    a :=  12;
    b :=   5;

    { we need this if statement here to prevent the optimizer from reducing
      all the code after it into just constant assignments and prints }
    if true then
      a := a
    else
      a := a;

    c := (a + b) * (a - b) + b MOD 2;
    print c { should print 120 }

  end

end

.