/**
 * dml.cc :- the source file implementing the standard DML interface.
 *
 * This source file contains the implementation of the classes in the
 * standard DML interface. These classes are defined in the header
 * file with the same name.
 */

#ifndef __PRIME_DML_CONFIG_H__
#define __PRIME_DML_CONFIG_H__
#include "dml-config.h"
#endif /*__PRIME_DML_CONFIG_H__*/

#include <string.h>
#include <assert.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#else
#error "Missing header file: sys/types.h"
#endif

#include "dml.h"
#include "dml-parser.h"

namespace s3f {
namespace dml {

#define DML_ATTR_BUFSIZ 4096

Dictionary* dmlConfig::dict = 0;
int dmlConfig::attrbufsz = 0;
char* dmlConfig::attrbuf = 0;

int VectorEnumeration::hasMoreElements()
{
  return iter != collected.end();
}

void* VectorEnumeration::nextElement()
{
  // return NULL if already reached the end of the list
  if(iter == collected.end()) return 0;

  // otherwise, get the item and move the pointer forward
  dmlConfig* cfg = (dmlConfig*)(*iter++);
  assert(cfg);
  if(cfg->value_type() == AttrNode::DML_ATTRTYPE_LIST) {
    // if the attribute value is another list of attributes, return
    // the node structure
    return cfg;
  } else {
    // otherwise, concatenate the value and the key in the attribute
    // buffer and return its pointer
    /*return (void*)cfg->value();*/
    const char* k = cfg->key(); int kk = strlen(k)+1;
    const char* v = cfg->value(); int vv = strlen(v)+1;
    char* attrbuf = cfg->attribute_buffer(kk+vv);
    strcpy(attrbuf, v);
    strcpy(&attrbuf[vv], k);
    return attrbuf;
  }
}

int Configuration::isConf(void* p)
{
  if(!p) return 0; // return false if the pointer is NULL
  else return ((Configuration*)p)->DNA == DML_CONFIGURATION_DNA;
}

char* Configuration::getKey(void* p)
{
  if(!p) return 0;
  if(isConf(p)) return (char*)((dmlConfig*)p)->key();
  return (char*)p+strlen((char*)p)+1;
}

dmlConfig::dmlConfig(char* filename)
{
  if(filename) loading(filename);
}

dmlConfig::dmlConfig(char** filenames)
{
  if(filenames) loading(filenames);
}

void dmlConfig::load(char* filename)
{
  cleanup();
  if(filename) loading(filename);
}

void dmlConfig::load(char** filenames)
{
  cleanup();
  if(filenames) loading(filenames);
}

void dmlConfig::loading(char* filename)
{
  assert(filename); // can't be NULL when this function is called

  // the root of the DML tree is returned when the file is parsed
  // without an error
  dmlConfig* root = parse(stderr, filename);
  if(root) {
    // we steal the first-level children of the root of the DML tree
    // constructed by the parser, since we can't assign the returned
    // tree to 'this'; we then reclaim the DML tree (with only the
    // root left) as it is no longer useful
    first_level_copy(root);
    delete root; 

    // we expand the tree following the directives, such as finds or
    // extends, and we also check if recursive definition is present;
    // if anything happens, we clean up the tree (which indicates an
    // error to the user)
    if(expand(this) || traverse_recursion())
      cleanup();
  }
}

void dmlConfig::loading(char** filenames)
{
  assert(filenames); // can't be a NULL list when this function is called

  // iterate through the DML files one by one; for each file, parse
  // the DML file and copy the resulting tree to 'this' (by stealing
  // its first-level children); skip those files causing parser errors
  int loaded = 0;
  for(int k=0; filenames[k]; k++) {
    dmlConfig* root = parse(stderr, filenames[k]);
    if(root) { first_level_copy(root); delete root; loaded = 1; }
  }

  // expand the tree and check recursion; if anything's wrong, we
  // clean up the entire tree
  if(loaded && (expand(this) || traverse_recursion()))
    cleanup();
}

void* dmlConfig::findSingle(char* keypath)
{
  if(!keypath) return 0;
  void* ret = 0;
  VectorEnumeration* set = new VectorEnumeration();
  assert(set);
  find_attributes(keypath, set->collected, 1); // only one match is needed
  set->iter = set->collected.begin();
  if(set->hasMoreElements()) 
    ret = set->nextElement();
  delete set;
  return ret;
}

Enumeration* dmlConfig::find(char* keypath)
{
  VectorEnumeration* set = new VectorEnumeration();
  assert(set);
  if(keypath) find_attributes(keypath, set->collected);
  set->iter = set->collected.begin();
  return set;
}

dmlConfig::dmlConfig(dmlConfig* firstkid) :
  AttrNode(firstkid)
{
  // it's possible that firstkid is NULL, if this is an empty list
}

dmlConfig::dmlConfig(AttrKey* key, KeyValue* keyval) :
  AttrNode(key, keyval)
{
  assert(key);
  assert(keyval);
}

void dmlConfig::first_level_copy(dmlConfig* root)
{
  // 'this' node must be root and therefore it must be a list
  assert(key_type() == AttrKey::DML_KEYTYPE_ROOT && 
	 value_type() == DML_ATTRTYPE_LIST);

  // the 'root' parameter must also be root and therefore must be a list
  assert(root && root->key_type() == AttrKey::DML_KEYTYPE_ROOT && 
	 root->value_type() == DML_ATTRTYPE_LIST);

  // note that the parsed DML tree must not be empty! this is
  // specified in the DML grammar (see dml-syntax.yy)
  assert(root->attr_value_list);

  if(attr_value_list) { // this node has been loaded earlier: copy it over
    for(std::vector<AttrNode*>::iterator iter = root->attr_value_list->begin();
        iter != root->attr_value_list->end(); iter++) {
      dmlConfig* kid = (dmlConfig*)(*iter); assert(kid);
      kid->attr_parent = this;
      attr_value_list->push_back(kid);
    }
    delete root->attr_value_list;
  } else { // this a fresh node: move it over
    for(std::vector<AttrNode*>::iterator iter = root->attr_value_list->begin();
        iter != root->attr_value_list->end(); iter++) {
      (*iter)->attr_parent = this;
    }
    attr_value_list = root->attr_value_list;
  }
  root->attr_value_list = 0; // so that deleting root won't go down recursively
  //#if PRIME_DML_LOCINFO
  locinfo.set(root->locinfo);
  //#endif
}

Dictionary* dmlConfig::dictionary() {
  // the dictionary is permanent and will never be reclaimed
  if(!dict) dict = new Dictionary();
  return dict;
}

char* dmlConfig::attribute_buffer(int sz) {
  // the attribute buffer is permanent and will not be reclaimed; it
  // can be resized though if it can no longer hold the key-value pair
  assert(sz > 0);
  if(sz <= attrbufsz) return attrbuf; // reuse the buffer
  else {
    if(attrbufsz == 0) attrbufsz = DML_ATTR_BUFSIZ;
    while(sz > attrbufsz) {
      int newsz = attrbufsz << 1;
      if(newsz < attrbufsz) { // if overflow happens
	attrbufsz = sz;
	break;
      } else attrbufsz = newsz;
    }
    char* t = new char[attrbufsz]; assert(t);
    if(attrbuf) delete[] attrbuf;
    return attrbuf = t;
  }
  return attrbuf;
}

}; // namespace dml
}; // namespace s3f

/*
 * Copyright (c) 2007 Florida International University.
 *
 * Permission is hereby granted, free of charge, to any individual or
 * institution obtaining a copy of this software and associated
 * documentation files (the "software"), to use, copy, modify, and
 * distribute without restriction.
 * 
 * The software is provided "as is", without warranty of any kind,
 * express or implied, including but not limited to the warranties of
 * merchantability, fitness for a particular purpose and
 * noninfringement. In no event shall Florida International University be
 * liable for any claim, damages or other liability, whether in an
 * action of contract, tort or otherwise, arising from, out of or in
 * connection with the software or the use or other dealings in the
 * software.
 * 
 * This software is developed and maintained by
 *
 *   The PRIME Research Group
 *   School of Computing and Information Sciences
 *   Florida International University
 *   Miami, FL 33199, USA
 *
 * Contact Jason Liu <liux@cis.fiu.edu> for questions regarding the use
 * of this software.
 */

/*
 * $Id$
 */
