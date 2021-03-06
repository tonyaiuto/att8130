# ATT 8130 "Computer" phone control software

I used one of these as my desk phone in the 1990s.
There was Windows support, but I used some *nix at the time,
so I decided to write my own integration. This code
has not been touched since 1996 and was never meant
for anyone else to see.

## The protocol

The serial port uses an "AT" style protocol.
This is what I reverse engineered so far.


```
ATH0, ATH1 get OK back

+TOH1=1		go off hook on line 1
+TOH2=1		go off hook on line 1

+TAC?		report area code

+TRS		reset

+TRDn?		report on speed dialer button n

+TLS1?		status line 1
+TLS2?		status line 2
	L2: (80)(01)(01)(02)(08)(ff)\r(80)(01)(02)(02)(00)(ff)\rOK\r
        L2: (80)(01)(01)(02)(08)(ff)\r(80)(01)(02)(02)(00)(ff)\rOK\r
	L2: (80)(01)(01)(02)(08)(ff)\r(80)(01)(02)(02)(00)(ff)\rOK\r

+TCLF		download call log
	4669766	01, 3:51pm 05/20 1R
	L2: (e1)(0f)2(12)(0f)2-(05)(14)`(01)(02)(00)\r(e2)5164669766\rOK\r
		 hh mm ss hh mm ss mon day yy ring ?? line? 5164669766


+TQA
+TQTA

L1: AT+TTM=16:24:03\r\r
L2: (82)\rOK\r

+TDO=0		tone dial
+TDO=1		pulse dial

+TLO1=0
```
