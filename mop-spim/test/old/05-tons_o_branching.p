program tonsOBranching;

class tonsOBranching

BEGIN

FUNCTION tonsOBranching;
   VAR 
      aa	   : integer;
      bb	   : integer;
      cc	   : integer;
      dd	   : integer;
      ee	   : integer;
BEGIN	   
	aa := 0;
	bb := 0;
	cc := 0;
	WHILE aa < 5 DO
	BEGIN
		bb := 4;
		cc := cc + 1
	END;
	
	aa := bb;

	IF bb > 0 THEN
		BEGIN
			dd := cc;
			IF cc > 0 THEN
				ee := dd
			ELSE
				ee := dd
		END
	ELSE
		aa := 0

END

END
.
