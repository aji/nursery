program overallocated;

class overallocated
begin

  function overallocated;
  var a, b, c, d, e, f, g,
      h, i, j, k, l, m, n,
      o, p, q, r, s, t, u,
      w, x, y, z : integer;
  begin
    z := 5;

    { this prevents the optimizer from replacing our 'print z' with 'print 5' }
    if z = z then
      z := z
    else
      z := z;

    print z
  end

end

.
