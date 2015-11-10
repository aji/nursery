program logical;

class logical
begin

  function logical;
  var a : boolean;
      b : boolean;
      c : boolean;
  begin

    a :=   true;
    b :=  false;

    { we need this if statement here to prevent the optimizer from reducing
      all the code after it into just constant assignments and prints }
    if true then
      a := a
    else
      a := a;

    c := a AND b;
    print c;

    c := a OR b;
    print c;

    c := NOT b;
    print c

  end

end

.
