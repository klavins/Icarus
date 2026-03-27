10 REM ========================
20 REM   OVERFLOW TEST
30 REM ========================
40 REM
100 REM ---- Normal math ----
110 PRINT "2 + 2 = ", 2 + 2
120 PRINT "100 * 100 = ", 100 * 100
130 PRINT "1000 - 2000 = ", 1000 - 2000
140 PRINT ""
200 REM ---- Addition overflow ----
210 PRINT "2000000000 + 2000000000:"
220 LET X = 2000000000 + 2000000000
230 PRINT "X = ", X
240 PRINT ""
300 REM ---- Multiplication overflow ----
310 PRINT "100000 * 100000:"
320 LET Y = 100000 * 100000
330 PRINT "Y = ", Y
340 PRINT ""
400 REM ---- Division by zero ----
410 PRINT "10 / 0:"
420 LET Z = 10 / 0
430 PRINT "Z = ", Z
440 PRINT ""
500 REM ---- Negative overflow ----
510 PRINT "-2000000000 - 2000000000:"
520 LET W = 0 - 2000000000 - 2000000000
530 PRINT "W = ", W
540 PRINT ""
600 PRINT "TESTS COMPLETE"
