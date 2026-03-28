10 REM ========================
20 REM   Logic Operator Tests
30 REM ========================
40 REM
100 REM ---- AND ----
110 PRINT "AND tests:"
120 LET X = 5
130 IF X > 0 AND X < 10 THEN PRINT "  5 in range 0-10: YES"
140 IF X > 0 AND X < 3 THEN PRINT "  BUG"
145 IF NOT (X > 0 AND X < 3) THEN PRINT "  5 in range 0-3: NO"
150 IF 1 AND 1 THEN PRINT "  1 AND 1 = TRUE"
160 IF 1 AND 0 THEN PRINT "  BUG"
170 IF 0 AND 1 THEN PRINT "  BUG"
180 PRINT ""
200 REM ---- OR ----
210 PRINT "OR tests:"
220 LET A = 0
230 LET B = 1
240 IF A = 1 OR B = 1 THEN PRINT "  A=0 OR B=1: YES"
250 IF A = 1 OR B = 0 THEN PRINT "  BUG"
260 IF 0 OR 0 THEN PRINT "  BUG"
270 IF 0 OR 1 THEN PRINT "  0 OR 1 = TRUE"
280 IF 1 OR 0 THEN PRINT "  1 OR 0 = TRUE"
290 PRINT ""
300 REM ---- NOT ----
310 PRINT "NOT tests:"
320 IF NOT 0 THEN PRINT "  NOT 0 = TRUE"
330 IF NOT 1 THEN PRINT "  BUG"
340 IF NOT (X = 3) THEN PRINT "  NOT (5=3) = TRUE"
350 IF NOT (X = 5) THEN PRINT "  BUG"
360 PRINT ""
400 REM ---- Combined ----
410 PRINT "Combined tests:"
420 LET X = 5
430 LET Y = 15
440 IF (X > 0 AND X < 10) OR Y > 20 THEN PRINT "  X in range OR Y>20: YES"
450 IF (X > 0 AND X < 10) AND (Y > 10 AND Y < 20) THEN PRINT "  both in range: YES"
460 IF NOT (X < 0 OR Y < 0) THEN PRINT "  neither negative: YES"
470 IF (X > 0 AND X < 3) OR (Y > 0 AND Y < 3) THEN PRINT "  BUG"
480 PRINT ""
500 PRINT "All tests passed if no BUG lines printed."
