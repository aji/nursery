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

    c := NOT ((a AND b) OR (a OR b));
    print c;

    c := NOT ((a AND b) AND (a OR b));
    print c;

    c := (a AND b) AND (a OR b);
    print c

    { notice also how the optimizer significantly reduces the size of the
      IR here }

  end

end

.
