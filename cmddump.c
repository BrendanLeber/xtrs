/* 
 * Copyright (c) 1996-2020, Timothy P. Mann
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Simulated TRS-80 DOS /cmd file loader.
 *
 * See the LDOS Quarterly, April 1, 1982 (Vol 1, No 4), for documentation
 *  of the TRS-80 DOS /cmd file format. 
 *
 * Usage: cmddump [-flags] infile [outfile startbyte nbytes]
 * If the optional arguments are given, the given byte range is dumped
 *  from the simulated memory after loading.
 *
 * Flags: -q      quiet: turn off -t, -m, -d, -s (later flags can override).
 *        -t      print text of module headers, pds headers, 
 *                  patch names, and copyright notices.
 *        -m      print running load map as file is parsed,
 *                  coalescing adjacent blocks (implies -t). (default)
 *        -d      print detailed map; same as -m, but don't coalesce.
 *        -s      print summary load map after file is parsed.
 *        -i n    select ISAM entry n (0x notation OK)
 *        -p foo  select PDS entry "foo" (padded to 8 bytes with spaces)
 *        -x      ignore anything after the first xfer address
 */

#define _XOPEN_SOURCE /* unistd.h: getopt(), optarg, optind, opterr */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "load_cmd.h"

unsigned char memory[64*1024] = { 0 };
unsigned char loadmap[64*1024];

#define ARGS "qtmdsi:p:x"

int
main(int argc, char* argv[])
{
  FILE* f;
  int verbosity = VERBOSITY_MAP;
  int isam = -1;
  char *pds = NULL;
  int xferaddr, stopxfer = 0;
  int print_summary = 0;
  char pdsbuf[9];
  int res;
  int c, errflg = 0;

  optarg = NULL;
  while (!errflg && (c = getopt(argc, argv, ARGS)) != -1) {
    switch (c) {
    case 'q':
      verbosity = VERBOSITY_QUIET;
      break;
    case 't':
      verbosity = VERBOSITY_TEXT;
      break;
    case 'm':
      verbosity = VERBOSITY_MAP;
      break;
    case 'd':
      verbosity = VERBOSITY_DETAILED;
      break;
    case 's':
      print_summary = 1;
      break;
    case 'i':
      isam = strtol(optarg, NULL, 0);
      break;
    case 'p':
      snprintf(pdsbuf, sizeof(pdsbuf), "%s%8s", optarg, "");
      pds = pdsbuf;
      break;
    case 'x':
      stopxfer = 1;
      break;
    default:
      errflg++;
      break;
    }
  }

  if (errflg || !(argc == optind + 1 || argc == optind + 4)) {
    fprintf(stderr,
	    "Usage: %s [-%s] infile [outfile startbyte nbytes]\n",
	    argv[0], ARGS);
    exit(1);
  }
    
  f = fopen(argv[optind], "r");
  if (f == NULL) {
    perror(argv[optind]);
    exit(1);
  }	

  res = load_cmd(f, memory, print_summary ? loadmap : NULL, 
		 verbosity, stdout, isam, pds, &xferaddr, stopxfer);

  switch (res) {
  case LOAD_CMD_OK:
    if (isam != -1) {
      fprintf(stderr, "warning: not ISAM file but -i flag given\n");
    } else if (pds) {
      fprintf(stderr, "warning: not PDS file but -p flag given\n");
    }
    break;
  case LOAD_CMD_EOF:
    fprintf(stderr, "premature end of file\n");
    exit(1);
  case LOAD_CMD_NOT_FOUND:
    if (pds) {
      fprintf(stderr, "PDS entry \"%s\" not found\n", pds);
    } else {
      fprintf(stderr, "ISAM entry 0x%02x not found\n", isam);
    }
    exit(1);
  case LOAD_CMD_ISAM:
    if (isam == -1) {
      fprintf(stderr, "warning: ISAM file but -i flag not given\n");
    }
    break;
  case LOAD_CMD_PDS:
    if (pds == NULL) {
      fprintf(stderr, "warning: PDS file but -p flag not given\n");
    }
    break;
  default:
    fprintf(stderr, "load file format error, bad block type 0x%02x\n", res);
    break;
  }

  if (print_summary) {
    /* Print load map */
    int lastaddr = -1;
    int lastcount = -1;
    int addr;
    for (addr = 0; addr < (int)sizeof(memory); addr++) {
      if (loadmap[addr] != lastcount) {
	if (lastcount == 1) {
	  printf("loaded 0x%04x - 0x%04x\n",
		 lastaddr, addr-1);
	} else if (lastcount > 1) {
	  printf("loaded 0x%04x - 0x%04x (%d times!)\n",
		 lastaddr, addr-1, lastcount);
	}
	lastaddr = addr;
	lastcount = loadmap[addr];
      }
    }
    printf("transfer address = 0x%04x\n", xferaddr);
  }

  /* Dump memory - optional */
  if (argc == optind + 4) {
    FILE* outf;
    int addr, count;
    outf = fopen(argv[optind + 1], "w");
    if (outf == NULL) {
      perror(argv[optind + 1]);
      exit(1);
    }
    addr = strtol(argv[optind + 2], (char**)NULL, 0);
    count = strtol(argv[optind + 3], (char**)NULL, 0);

    while (count-- > 0) {
      putc(memory[addr++], outf);
    }
    fclose(outf);
  }
  return 0;
}
