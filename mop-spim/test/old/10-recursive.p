program recursive;

class AA
begin
  var b : BB;
end

class BB
begin
  var c : CC;
end

class CC
begin
  var a : AA;
end

class recursive
begin
  var a : AA;
      b : BB;

  function recursive;
  begin
    a := b
  end
end

.
