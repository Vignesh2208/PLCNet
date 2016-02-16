/**
 * \file ip_prefix.cc
 * \brief Source file for the IPPrefix class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/ip_prefix.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#undef IPADDR_DEBUG // too much
#ifdef IPADDR_DEBUG
#define IPADDR_DUMP(x) printf("IPADDR: "); x
#else
#define IPADDR_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

IPADDR IPPrefix::masks[33] = {
  0x0,
  0x80000000,0xC0000000,0xE0000000,0xF0000000,
  0xF8000000,0xFC000000,0xFE000000,0xFF000000,
  0xFF800000,0xFFC00000,0xFFE00000,0xFFF00000,
  0xFFF80000,0xFFFC0000,0xFFFE0000,0xFFFF0000,
  0xFFFF8000,0xFFFFC000,0xFFFFE000,0xFFFFF000,
  0xFFFFF800,0xFFFFFC00,0xFFFFFE00,0xFFFFFF00,
  0xFFFFFF80,0xFFFFFFC0,0xFFFFFFE0,0xFFFFFFF0,
  0xFFFFFFF8,0xFFFFFFFC,0xFFFFFFFE,0xFFFFFFFF
};

// CAUTION: there could be a problem with global static variables;
// they may be shared and unprotected for simultaneous access by
// multiple threads!!!
char IPPrefix::dispbuf[50];

char* IPPrefix::ip2txt(IPADDR ip, char* str)
{
  char* s = str;
  for(int i=24; i >=0; i-=8) {
    sprintf(s, "%u", ((unsigned)((ip >> i) & 0xFF)));
    s += strlen(s);
    if(i > 0) sprintf(s++, ".");
  }
  IPADDR_DUMP(printf("ip2txt(): 0x%x => %s\n", ip, str));
  return str;
}

char* const IPPrefix::ip2txt(IPADDR ip)
{
  return ip2txt(ip, dispbuf);
}

IPADDR IPPrefix::txt2ip(const char* str, int* len)
{
  assert(str);
  int dot_num = 0;  // how many dot are met
  IPADDR base = 0, incr = 0;
  if(len) *len = 32; // default

  for(int i=0; (isdigit(str[i]) || str[i] == '\0' || str[i] == '/'  ||
		(str[i] == '.' && dot_num <= 2)); i++) {
    // we get to the end of a number
    if((str[i] == '.') || (str[i] == '\0') || (str[i] == '/')) {
      base *= 256; 
      base += incr;
      incr = 0;

      // at the end of the string
      if(str[i] == '\0') break;

      // the end of the part should be the length of mask
      if(str[i] == '/') {
	if(len) *len = atoi(str + i + 1);
	break;
      }

      // another dot
      dot_num++;
    } else { // it is a digit
      incr *= 10;
      incr += (str[i] - '0'); 
    }
  }

  IPADDR_DUMP(printf("txt2ip(): %s => 0x%x", str, base);
	      if(len && *len<IPADDR_LENGTH) printf("/%d", *len);
	      printf("\n"));
  return base;
}

bool IPPrefix::txt2ip(const char* str, IPADDR& addr)
{
  assert(str);
  long total=0, t;
  char *p=(char*)str, *endp;

  for (int i=0; i<3; i++) { // the first three segments
    t = strtol(p, &endp, 10);
    if (endp == p || *endp == '\0') return false;
    if (t < 0 || t>255) return false;
    total <<= 8; total += t;
    p = endp + 1;
    if (*p == '\0') return false;
  }

  // the last segment
  t = strtol(p, &endp, 10);
  if (endp == p || *endp != '\0') return false;
  if (t < 0 || t>255) return false;
  total <<= 8; total += t;

  addr = (IPADDR)total;
  return true;
}

IPPrefix& IPPrefix::operator = (const IPPrefix& rhs)
{
  addr = rhs.addr;
  len = rhs.len;
  return *this;
}

bool IPPrefix::contains(IPPrefix& ip)
{
  // the ip mask length is ignored in this case
  return ((ip.addr ^ addr) & masks[len]) == 0;
}

bool IPPrefix::contains(IPADDR ipaddr)
{
  return ((ipaddr ^ addr) & masks[len]) == 0;
}

void IPPrefix::display(FILE* fp) const
{
  if(NULL == fp) fp = stdout;

  for(int i=24; i >=0; i-=8) {
    fprintf(fp, "%ld", (long)((addr >> i) & 0xFF));
    if(i > 0) fprintf(fp, ".");
  }
  if(len < IPADDR_LENGTH) fprintf(fp, "/%d", len);
}

char* IPPrefix::toString(char* buf) const
{
  if(!buf) buf = dispbuf;
  char* ret = buf;
  for(int i=24; i >=0; i-=8) {
    sprintf(buf, "%ld", (long)((addr >> i) & 0xFF));
    buf += strlen(buf);
    if(i > 0) sprintf(buf++, ".");
  }
  if(len < IPADDR_LENGTH) sprintf(buf, "/%d", len);
  return ret;
}

bool operator == (const IPPrefix& ip1, const IPPrefix& ip2)
{
  return (ip1.len == ip2.len) && 
    ((ip1.addr & IPPrefix::masks[ip1.len]) == 
     (ip2.addr & IPPrefix::masks[ip2.len]));
}

}; // namespace s3fnet
}; // namespace s3f
