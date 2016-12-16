# invoke SourceDir generated makefile for DTC1200.pem4f
DTC1200.pem4f: .libraries,DTC1200.pem4f
.libraries,DTC1200.pem4f: package/cfg/DTC1200_pem4f.xdl
	$(MAKE) -f C:\Users\bob\workspace_v6_2\DTC1200_TivaTM4C123AE6PM/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\bob\workspace_v6_2\DTC1200_TivaTM4C123AE6PM/src/makefile.libs clean

