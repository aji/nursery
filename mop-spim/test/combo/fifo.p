program fifoTest;

class FIFONode
begin

  var      value : integer;
      isSentinel : boolean;
      next, prev : FIFONode;

  function FIFONode(value: integer; isSentinel: boolean): boolean;
  begin 
    this.value      := value;
    this.isSentinel := isSentinel
  end

end

class FIFO
begin

  var head, tail : FIFONode;
            size : integer;

  function FIFO;
  begin
    head := new FIFONode(0, true);
    tail := new FIFONode(0, true);

    head.next := tail;
    tail.prev := head
  end;

  function add(x: integer): boolean;
  var n : FIFONode;
  begin
    n := new FIFONode(x, false);

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
      remove := tail.prev.value;

      tail.prev := tail.prev.prev;
      size := size - 1
    end
  end;

  function empty(ign: boolean): boolean;
  begin
    empty := size = 0
  end

end

class fifoTest
begin

  function fifoTest;
  var   q : FIFO;
      tmp : integer;
      ign : boolean;
  begin

    q := new FIFO;

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
