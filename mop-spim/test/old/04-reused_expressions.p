program reusedExpressions;

class reusedExpressions

BEGIN
FUNCTION reusedExpressions;

   VAR 
      aa	   : integer;
      bb	   : integer;
      cc	   : integer;
      dd	   : integer;

BEGIN	   
   aa := 2;
   bb := aa*5;
   cc := aa*5;
   dd := cc
END

END
.
