program linalg;

{ this program multiplies a vector <2, 6, 4, 5> by a matrix which swaps the
  first two components and doubles the last one

  it prints 8 numbers. the first 4 numbers are the original vector, and the
  second 4 are the new vector. }

class Vector4
begin
  var V : array [1..4] of integer;

  function Vector4(a, b, c, d : integer): boolean;
  begin
    V[1] := a; V[2] := b; V[3] := c; V[4] := d
  end;

  function dot(that: Vector4): integer;
  begin
    dot := V[1]*that.V[1] + V[2]*that.V[2] + V[3]*that.V[3] + V[4]*that.V[4]
  end;

  function dump(ign: integer): boolean;
  begin
    print V[1];
    print V[2];
    print V[3];
    print V[4]
  end
end

class Matrix4x4
begin
  { row major }
  var M : array [1..4] of array [1..4] of integer;

  function Matrix4x4(
    a, b, c, d,
    e, f, g, h,
    i, j, k, l,
    m, n, o, p : integer): boolean;
  begin
    M[1][1]:=a; M[1][2]:=b; M[1][3]:=c; M[1][4]:=d;
    M[2][1]:=e; M[2][2]:=f; M[2][3]:=g; M[2][4]:=h;
    M[3][1]:=i; M[3][2]:=j; M[3][3]:=k; M[3][4]:=l;
    M[4][1]:=m; M[4][2]:=n; M[4][3]:=o; M[4][4]:=p
  end;

  function trace: integer;
  begin
    trace := M[1][1] + M[2][2] + M[3][3] + M[4][4]
  end;

  function mulv(x: Vector4): Vector4;
  begin
    mulv := new Vector4(
      M[1][1]*x.V[1] + M[1][2]*x.V[2] + M[1][3]*x.V[3] + M[1][4]*x.V[4],
      M[2][1]*x.V[1] + M[2][2]*x.V[2] + M[2][3]*x.V[3] + M[2][4]*x.V[4],
      M[3][1]*x.V[1] + M[3][2]*x.V[2] + M[3][3]*x.V[3] + M[3][4]*x.V[4],
      M[4][1]*x.V[1] + M[4][2]*x.V[2] + M[4][3]*x.V[3] + M[4][4]*x.V[4]
    )
  end
end

class linalg
begin
  var A : Matrix4x4;
      x : Vector4;
      y : Vector4;

  function linalg;
  var ign : boolean;
  begin
    A := new Matrix4x4(
      0, 1, 0, 0,
      1, 0, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 2
    );
    x := new Vector4(2, 6, 4, 5);
    y := A.mulv(x);

    ign := x.dump(0);
    ign := y.dump(0)
  end
end

.
