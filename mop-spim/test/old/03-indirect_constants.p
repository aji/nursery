program indirectConstants;

class indirectConstants

BEGIN

   VAR 
      aa	   : integer;
      bb	   : integer;
      cc	   : integer;

FUNCTION indirectConstants;
BEGIN	   
   aa := 4;
   bb := aa;
   cc := 3*bb
END

END
.
