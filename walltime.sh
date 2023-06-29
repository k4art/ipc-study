#!/bin/awk -f

START   { bs = bns = es = ens = 0 }

/BEGIN/ { bs=$2; bns=$3 }
/END/   { es=$2; ens=$3 }

END     { sec  = es-bs;
          ns   = ens-bns;
          diff = sec * 1000 + ns / 1000000;
          printf diff " ms" }
