program lifoTest;

class LIFONode
begin

  var      value : integer;
      isSentinel : boolean;
      next, prev : LIFONode;

  function LIFONode(value: integer; isSentinel: boolean): boolean;
  begin 
    this.value      := value;
    this.isSentinel := isSentinel
  end

end

class LIFO
begin

  var head, tail : LIFONode;
            size : integer;

  function LIFO;
  begin
    head := new LIFONode(0, true);
    tail := new LIFONode(0, true);

    head.next := tail;
    tail.prev := head
  end;

  function add(x: integer): boolean;
  var n : LIFONode;
  begin
    n := new LIFONode(x, false);

    n.next := head.next;
    n.prev := head;
    n.next.prev := n;
    n.prev.next := n;

    size := size + 1;
    add := true
  end;

  function remove(ign: boolean): integer;
  begin
    if size = 0 then
      remove := -1
    else begin
      remove := head.next.value;

      head.next := head.next.next;
      size := size - 1
    end
  end;

  function empty(ign: boolean): boolean;
  begin
    empty := size = 0
  end

end

class lifoTest
begin

  function lifoTest;
  var   q : LIFO;
      tmp : integer;
      ign : boolean;
  begin

    q := new LIFO;

    ign := q.add(3);
    ign := q.add(6);
    ign := q.add(8);
    ign := q.add(15);
    ign := q.add(1);

    while not q.empty(ign) do
    begin
      tmp := q.remove(ign);
      print tmp
    end

  end

end

.
