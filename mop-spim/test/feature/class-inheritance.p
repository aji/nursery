program classes;

class Quadrilateral
begin
  var a, b : integer;

  function Quadrilateral(a, b: integer): integer;
  begin
    this.a := a;
    this.b := b
  end;

  function getArea(ign: integer): integer;
  begin
    getArea := a * b
  end
end

class Square extends Quadrilateral
begin
  function Square(a: integer): integer;
  begin
    this.a := a;
    this.b := a
  end
end

class classes
begin

  function printArea(q: Quadrilateral): integer;
  begin
    printArea := q.getArea(0);
    print printArea
  end;

  function classes;
  var    x : integer;
      quad : Quadrilateral;
       squ : Square;
  begin

    quad := new Quadrilateral(3, 6);
    squ := new Square(5);

    x := printArea(quad);
    x := printArea(squ)
  end

end

.
