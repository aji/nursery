program indirectConstants;

class indirectConstants

BEGIN

FUNCTION indirectConstants;

   VAR 
      aa	   : integer;
      bb	   : integer;
      cc	   : integer;
      dd	   : integer;
BEGIN	   
    cc := 12;
    aa := dd + cc;
    bb := dd + cc;
    bb := 10;
    cc := dd + cc
END

END
.
