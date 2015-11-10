program indirectConstants;

class indirectConstants

BEGIN

FUNCTION indirectConstants;

   VAR 
      aa	   : integer;
      bb	   : integer;
      cc	   : integer;
      dd	   : integer;
      ee	   : integer;
BEGIN	   
	aa := 2;
	bb := 7;
	cc := aa + bb;
	IF bb > 0 THEN
		BEGIN
			dd := aa + bb;
			IF bb > 0 THEN
				ee := aa + bb
			ELSE
				cc := 0
		END
	ELSE
		cc := 0
	END
END
.
