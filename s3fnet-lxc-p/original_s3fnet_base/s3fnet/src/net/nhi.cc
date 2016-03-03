/**
 * \file nhi.cc
 * \brief Source file for the Nhi class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/nhi.h"
#include "util/errhandle.h"


namespace s3f {
namespace s3fnet {

char Nhi::internal_buf[50];

Nhi::Nhi() : start(0), type(NHI_INVALID)
{}

Nhi::Nhi(char* str, int t) : start(0), type(NHI_INVALID)
{
  // convert the string into NHI
  convert(str, t);
}

Nhi::Nhi(Nhi& rhs)
{
  (*this) = rhs;
}

Nhi::~Nhi() { ids.clear(); }

Nhi& Nhi::operator = (Nhi& rhs)
{
  if(this == &rhs) 
    return (*this);

  start = rhs.start;
  type = rhs.type;
  ids.clear();
  for(unsigned i=0; i < rhs.ids.size(); i++) 
    ids.push_back(rhs.ids[i]);

  return (*this);
}

Nhi& Nhi::operator += (int id) 
{
  ids.push_back(id);
  return (*this);
}

bool operator == (Nhi& n1, Nhi& n2)
{
  int len, len2;
  
  len = n1.ids.size() - n1.start;
  len2 = n2.ids.size() - n2.start;

  if(len != len2)
    return false;
  
  for(int i=0; i < len; i++) 
    if(n1.ids[n1.start + i] != n2.ids[n2.start + i])
      return false;

  return true;
}

char* const Nhi::toString()
{
  // not a good idea in a multiprocessor environment
  return toString(internal_buf);
}

char* Nhi::toString(char* buf)
{
  unsigned i;
  char* p = buf; *p = '\0';
  int minlength;

  if(type == NHI_INVALID) {
    strcpy(buf, "NHI_INVALID");
    return buf;
  }

  minlength = (type == Nhi::NHI_INTERFACE)?2:1;

  if(ids.size() == 0) {
    buf[0] = '\0';
    return buf;
  }

  for(i=start; i < ids.size() - minlength; i++) 
    p += sprintf(p, "%ld:", ids[i]);

  switch (type) {
  case NHI_INTERFACE:
    sprintf(p, "%ld(%ld)", ids[i], ids[i+1]);
    break;

  case NHI_MACHINE:
  case NHI_NET:
    sprintf(p, "%ld", ids[i]);
    break;

  default:
    assert(0);
    break;
  }

  return buf;
}

S3FNET_STRING Nhi::toStlString()
{
  char buf[255];
  toString(buf);
  S3FNET_STRING str(buf);
  return str;
}

void Nhi::display()
{
  unsigned i;
  int minlength;

  if(type == NHI_INVALID) {
    printf("NHI_INVALID");
    return;
  }

  minlength = (type == Nhi::NHI_INTERFACE)?2:1;

  for(i=start; i < ids.size() - minlength; i++)
    printf("%ld:", ids[i]);

  switch (type) {
  case NHI_INTERFACE:
    printf("%ld(%ld)", ids[i], ids[i+1]);
    break;

  case NHI_MACHINE:
  case NHI_NET:
    printf("%ld", ids[i]);
    break;

  default:
    assert(0);
    break;
  }
}

int Nhi::convert(char* str, int ntype)
{
  char* p = str, *q;

  while((q = strchr(p, ':'))) {
    *q = '\0';
    ids.push_back(atoi(p));
    *q = ':';
    p = ++q;
  }

  // At this point p must point to a string of the form 2(1)
  // If we just have 2, this is a machine NHI or a Net NHI,
  type = ntype;

  q = strchr(p, '(');
  if(q) {
    *q = '\0';
    type = NHI_INTERFACE;
  } else {
    type &= ~NHI_INTERFACE;
  }

  ids.push_back(atoi(p));

  if(type == NHI_INTERFACE) {
    *q = '(';
    p = ++q;

    q = strchr(p, ')');
    if(q) {
      *q = '\0';
      ids.push_back(atoi(p));
      *q = ')';
    } else {
      type = NHI_INVALID;
      ids.push_back(atoi(p)); // unnecessary
    }
  }

  start = 0;
  return (type & ntype)?0:1;
}

//
// Function to read the contents of an nhi_range attribute
// into a vector. The vector is responsible for deallocating
// the Nhi objects we create and the vector we return.
//
// [port 10 nhi_range [from 1:2(0) to 1:5(0)]
//
int Nhi::readNhiRange(S3FNET_POINTER_VECTOR& nhivec,
		      s3f::dml::Configuration* cfg)
{
  // Look for from and to.
  Nhi from, to;
  int expected = Nhi::NHI_INTERFACE; // both must be interface nhi

  char* str = (char*)cfg->findSingle("from");
  if(!str || s3f::dml::dmlConfig::isConf(str)) {
    error_quit("ERROR: invalid FROM attribute in NHI_RANGE!\n");
  } else if(from.convert(str, expected)) {
    error_quit("ERROR: nhi type conversion error: \"%s\", "
	       "invalid FROM attribute in NHI_RANGE.\n", str);
  }

  str = (char*)cfg->findSingle("to");
  if(!str || s3f::dml::dmlConfig::isConf(str)) {
    error_quit("ERROR: invalid TO attribute in NHI_RANGE!\n");
  } else if(to.convert(str, expected)) {
    error_quit("ERROR: nhi type conversion error: \"%s\", "
	       "invalid TO attribute in NHI_RANGE.\n", str);
  }

  // These must have the same length and same prefix.
  unsigned size = from.ids.size();
  assert(size >= 2); // interface nhi must have such length
  if(size != to.ids.size()) {
    error_quit("ERROR: FROM and TO attributes in NHI_range must have the same size.\n");
  }
  for(unsigned k=0; k<size-2; k++) {
    if(from.ids[k] != to.ids[k]) {
      char buf0[50], buf1[50];
      error_quit("ERROR: FROM \"%s\" and TO \"%s\" are not in the same subnet.\n",
		 from.toString(buf0), to.toString(buf1));
    }
  }

  // cycle the machine id and create new Nhi objects for each.
  int toid = to.ids[size-2];
  for(int i=from.ids[size-2]; i <= toid; i++) {
    from.ids[size-2] = i;
    Nhi* pNhi = new Nhi(from);
    nhivec.push_back(pNhi);
  }

  return 0;
}

bool Nhi::contains(Nhi& nhi)
{
  int len, len2;
  
  len = ids.size() - start;
  len2 = nhi.ids.size() - nhi.start;

  if(len > len2) return false;
  
  for(int i=0; i < len; i++) 
    if(ids[start + i] != nhi.ids[nhi.start + i])
      return false;

  return true;
}

bool Nhi::contains(char* nhiAddr)
{
  assert(nhiAddr);
  for(unsigned i=start; i<ids.size(); i++) {
    if(*nhiAddr == 0) return false;

    int x = 0;
    while(*nhiAddr != 0 && *nhiAddr != ':') x = x*10+((*nhiAddr++)-'0');
    if(*nhiAddr) nhiAddr++;

    if(x != ids[i]) return false;
  }

  return true;
}

void Nhi::setRelativeTo(Nhi& parent)
{
  if(!parent.contains(*this)) {
    char buf1[50], buf2[50];
    error_quit("ERROR: Nhi::setRelativeTo(), "
	       "parent \"%s\" does not contain this nhi \"%s\".\n",
	       parent.toString(buf1), toString(buf2));
  }
  start += parent.ids.size();
}

}; // namespace s3fnet
}; // namespace s3f
