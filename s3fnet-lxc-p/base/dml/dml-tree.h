/*
 * dml-tree.h :- header file for the DML tree data structure.
 *
 * A DML script can be viewed as a tree structure. Each node in the
 * tree, which can be identified and retrieved via a keypath, is
 * either an attribute or a list of attributes. This module contains
 * the definition of classes and data structures necessary to
 * construct and traverse the DML tree.
 */

#ifndef __PRIME_DML_TREE_H__
#define __PRIME_DML_TREE_H__

#include <vector>

#include <assert.h>
#include <stdio.h>

#include "dml-locinfo.h"

namespace s3f {
namespace dml {

  /*
   * This data structure is used to connect the lexical scanner with
   * the syntax analyzer. Parsed tokens (except singletons such as '['
   * and ']') are all stored as an instance of this object in the
   * buffer maintained by the lexical scanner.
   */
  class KeyValue {
  public:
    // Constructor: a key or a value must be a string.
    KeyValue(const char* yytext);

    // Destructor.
    virtual ~KeyValue();

    // This is the string (representing either the attribute key or
    // value). It's actually pointing to the string in the
    // dictionary maintained by the DML library.
    const char* ident;

    // Location information of this token.
    LocInfo locinfo;
  }; /*KeyValue*/

  /*
   * AttrKey is the class used to specify a DML attribute key. The key
   * is either a keyword (such as _extends, _finds, or _schema), or a
   * generic string identifier per DML grammar.
   */
  class AttrKey {
  public:
    // Enumerating the key types.
    enum {
      DML_KEYTYPE_ROOT    = 0, // used by the root attribute (without key)
      DML_KEYTYPE_EXTENDS = 1, // _extends
      DML_KEYTYPE_FIND    = 2, // _find
      DML_KEYTYPE_SCHEMA  = 3, // _schema
      DML_KEYTYPE_IDENT   = 4  // {generic identifier}
    };

    /*
     * Constructor. Check whether a given attribute key is a keyword
     * (i.e., _extends, _find, or _schema). The key type is set
     * accordingly. If it is a keyword, key_ident is set to be
     * NULL. Otherwise, the text string is copied (i.e., the reference
     * counter to the string in the dictionary is incremented).
     */
    AttrKey(KeyValue* keyval);

    // Destructor.
    virtual ~AttrKey();

    // Copy constructor.
    AttrKey(const AttrKey&);

    // Creates a copy of this object and returns a pointer to the
    // copy. This results a true copy of the object being created.
    AttrKey* clone();

    // Returns the key type.
    int type() const { return key_type; }

    /*
     * Returns the key string, as is. You're not supposed to reclaim
     * the string; the returned value must be explicitly copied.
     */
    const char* ident() const {
      switch(key_type) {
      case DML_KEYTYPE_ROOT:    return 0;
      case DML_KEYTYPE_EXTENDS: return "_extends";
      case DML_KEYTYPE_FIND:    return "_find";
      case DML_KEYTYPE_SCHEMA:  return "_schema";
      case DML_KEYTYPE_IDENT:   assert(key_ident); return key_ident;
      default: assert(0);
      }
      return 0;
    }

    // Return the reference to the location information.
    LocInfo& location() { return locinfo; }

    // Used to pretty print the DML tree to standard output (for
    // debugging purposes).
    void print(FILE* fp, int space) const;

  protected:
    friend class AttrNode;

    /*
     * Type of the attribute key. There can be only four types:
     * DML_KEYTYPE_EXTENDS, DML_KEYTYPE_FIND, DML_KEYTYPE_SCHEMA, and
     * DML_KEYTYPE_IDENT.
     */
    int key_type;

    /*
     * If the key type is DML_KEYTYPE_IDENT, the field stores the
     * identifier string (i.e., a reference to a string in the
     * dictionary). Otherwise, it's NULL.
     */
    const char* key_ident;

    // Location information of the attribute key.
    LocInfo locinfo;
  }; /*AttrKey*/

  /*
   * AttrNode is the class used to represent a node in the DML
   * tree. It has either a single attribute or it has a list of child
   * attributes.
   */
  class AttrNode {
  public:
    // Enumerate attribute value types.
    enum {
      // The attribute's value is a list of attributes.
      DML_ATTRTYPE_LIST    = 5,

      // The attribute's value is a string: it's either a C-style
      // string enclosed by double-quotes, or an unquoted string that
      // fits the DML grammer. Substitution of special characters
      // (such as '\t') is applied to a C-style string.
      DML_ATTRTYPE_STRING  = 6
    };

    /*
     * Constructor. The attribute contains a list of attributes. Only
     * the first child attribute (if not NULL) is given here. Other
     * attributes are supposed to be appended later.
     */
    AttrNode(AttrNode* firstkid = 0);

    /*
     * Constructor. The attribute is a single string: it's either a
     * C-style string in double-quotes or unquoted string comforming
     * to the DML grammer.
     */
    AttrNode(AttrKey* key, KeyValue* keyval);

    // Copy constructor: a "deep" copy of this object is created.
    AttrNode(const AttrNode&);

    /*
     * The clone operator is used to make a "shallow" copy of this
     * object (by incrementing the reference counter); it returns a
     * new reference to this attribute.
     */
    AttrNode* clone();

    /*
     * Destructor. The users should be discouraged to delete an
     * AttrNode object directly, since this bypasses the reference
     * counter scheme we use. Instead, the users need to call
     * the dispose() method.
     */
    virtual ~AttrNode();

    /*
     * Remove a reference to this object. If the reference counter
     * gets to zero, we reclaim it.
     */
    virtual void dispose();

    /*
     * For a list attribute, the key is added after all the child
     * attributes have been appended. The current attribute is
     * returned for convenience. Only root is not doing this. The root
     * does not have an attribute key.
     */
    AttrNode* add_key(AttrKey* key, LocInfo* bracket_locinfo);

    /*
     * More child attributes can be added to the current attribute,
     * one after another. The current attribute node is returned for
     * convenience.
     */
    AttrNode* append_kid(AttrNode* newkid);

    /*
     * Returns the type of the attribute key. This function can only
     * be called after the node is fully constructed (i.e., after
     * parsing).
     */
    int key_type() const { 
      if(attr_key) return attr_key->type(); 
      else {
        assert(!attr_parent); // must be root
        return AttrKey::DML_KEYTYPE_ROOT;
      }
    }

    // Returns the value type of this attribute.
    int value_type() const { return attr_type; }

    /*
     * Returns the key of this attribute, as is. The returned string
     * must be copied explicitly if one wants to store it for later
     * use. Otherwise, the string will be reclaimed together with the
     * DML tree later by the DML library.
     */
    const char* key() const;

    /*
     * Returns the value of this attribute. Applies only when the
     * attribute is not a list (i.e., it's of type
     * DML_ATTRTYPE_STRING). The returned string must be copied
     * explicitly if one wants to store it for later use.  Otherwise,
     * the string will be reclaimed together with the DML tree later
     * by the DML library.
     */
    const char* value() const;

    /*
     * Returns the location information of the attribute value if 
     * it's a singleton. If it's a list of attributes, returns the
     * location of the first attribute. The location is undefined
     * if the list is empty.
     */
    LocInfo& location() { return locinfo; }

    /*
     * This method is called orginally from the root of the DML
     * tree. Recursively, we consolidate the references at each node
     * made by '_extends' or '_find' keywords. The root of the tree is
     * provided as a convenience. If it's NULL, we'll have to search
     * for it through the parent link (not really efficient this way).
     * The method returns non-zero if error occurs.
     */
    int expand(AttrNode* root = 0);

    /*
     * Search for the attribute node from a given keypath. The keypath
     * is composed of a series of key names separated by periods,
     * similar to using '/' to separate directory and file names in a
     * common unix file system. The search is relative to the current
     * node if the keypath is not started with '.'. The root of the
     * tree is provided as a convenience. If it's NULL, we'll have to
     * search for it through the parent link. Located attributes are
     * collected in a vector.
     */
    void find_attributes(char* keypath, std::vector<AttrNode*>& collected, 
                         int onematch = 0, AttrNode* root = 0);

    /*
     * Called by the expand() method to find out whether the DML tree
     * contains recursive references. Returns true if found.
     */
    int traverse_recursion();

    /*
     * Used to pretty print the DML tree. We don't follow _extends or
     * _find references unless 'noexpand' is false. This function is
     * for debugging purposes.
     */
    void print(FILE* fp, int space, int noexpand = 0);

  protected:
    friend class dmlConfig;

    /*
     * We use a reference counter scheme: we use the counter to allow
     * muliple references to the same object.
     */
    int refcnt;

    /*
     * Type of the attribute, which can be either DML_ATTRTYPE_LIST
     * or DML_ATTRTYPE_STRING.
     */
    int attr_type;

    // Parent of this node. NULL if this is the root of the DML tree.
    AttrNode* attr_parent;

    /*
     * Attribute key. NULL if this is the root attribute, which does
     * not have a key.
     */
    AttrKey* attr_key;

    /*
     * Attribute value: either it's a pointer to a string
     * (DML_ATTRTYPE_STRING), or it points to a vector of child
     * attributes of type AttrNode (DML_ATTRTYPE_LIST). If the list
     * attribute is empty, it's NULL.
     */
    union {
      const char* attr_value_string;
      std::vector<AttrNode*>* attr_value_list;
    };

    /*
     * If the key of this attribute is '_extends' or '_find', this
     * field is a pointer to the attribute it is referring
     * to. Otherwise, it's NULL. This variable is used to traverse the
     * DML tree per user's inquiry.
     */
    AttrNode* attr_expand;

    /*
     * This flag is used to mark whether the current node is currently
     * in the process of expanding itself (i.e., processing the
     * _extends keyword) as we traverse the tree. This facility is
     * neccessary to avoid infinite recursions caused by a circular
     * reference in user's DML script.
     */
    int expanding;

    /*
     * Location information of the symbol identified to be the
     * attribute value. If it's a list of attributes, it's the
     * location of the first child attribute. It is undefined if this
     * is an empty list.
     */
    LocInfo locinfo;

    /*
     * If the input string is enclosed in double quotes, we should do
     * extra processing to replace the escape characters embedded in
     * the raw string. A new string is returned. If the input string
     * is not in quotes, a reference to the original string is 
     * returned. This method is called by the constructor.
     */
    const char* make_string(KeyValue* keyval) const;

    // Helper function to the make_string() method.
    static int next_char(const char*& s);

    /*
     * This method is used to instantiate the reference made by the
     * _extends or _find keyword.  Find the node in the DML tree of
     * the given keypath. The tree root should be provided as a
     * parameter in case the key path is absoluted. The attribute node
     * is returned, if found. Or, NULL otherwise. The search does not
     * follow the reference links created from expanded _extends or
     * _find keywords. Also this method returns only one attribute of
     * the given key path; it's the first one if there exist multiple
     * attributes.
     */
    AttrNode* search_attribute(char* keypath, AttrNode* root);

    // Reclaims everything.
    void cleanup();
  }; // class AttrNode

}; // namespace dml
}; // namespace s3f

#endif /*__PRIME_DML_TREE_H__*/

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
