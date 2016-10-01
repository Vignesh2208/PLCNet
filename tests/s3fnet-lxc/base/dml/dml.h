/**
 * \file dml.h
 * \brief Header file for the PRIME DML library.
 *
 * This header file contains the standard interface for parsing a DML
 * file or a set of DML files. DML stands for Domain Modeling Language
 * and it's used for configuring simulation models. The users need to
 * only include this file in their programs to access the DML library.
 */

#ifndef __PRIME_DML_H__
#define __PRIME_DML_H__

#include <vector>

#include "dml-tree.h"
#include "dml-dictionary.h"
#include "dml-exception.h"

typedef unsigned int u_int32_t;

namespace s3f {

/// DML objects are defined in this namespace.
namespace dml {

  /**
   * \brief An abstract class for enumeration type.
   *
   * This abstract container class is not part of the C++ standard
   * template library (STL). However, it's been defined in the DML
   * standard interface specification to accommodate the Java binding.
   */
  class Enumeration {
  public:
    /// The constructor.
    Enumeration() {}

    /// The destructor.
    virtual ~Enumeration() {}

    /// Returns true (non-zero) if there are more elements to be enumerated.
    virtual int hasMoreElements() = 0;

    /// Returns the next elements in the enumeration.
    virtual void* nextElement() = 0;
  }; /*Enumeration*/

  /**
   * \brief An enumeration class with extended capabilities.
   *
   * This class is provided for enumerating DML attributes returned
   * from the dmlConfig::find() method. The class contains internal
   * implementation details, and the user is advised not to make a
   * direct reference to this class. The only exception is when the
   * user wants to use the extended methods (such as the reset() or
   * the size() method). It is advised that using these methods
   * the user assumes that the application depends on the particular
   * implementation and its portability may later become questionable.
   */
  class VectorEnumeration : public Enumeration {
  public:
    /// The constructor.
    VectorEnumeration() {} // nothing to be defined here

    /// The destructor.
    virtual ~VectorEnumeration() {} // nothing to be defined here

    /// Returns true if we have more elements to be enumerated.
    virtual int hasMoreElements();

    /// Returns the next elements in the enumeration.
    virtual void* nextElement();

    /**
     * \brief Resets the enumeration; restarts from the beginning.
     *
     * Standard enumeration object does not support the this method.
     * The standard is that enumeration can only be processed once.
     * This unnecessarily causes unwanted restrictions for
     * programming. We added the method here to make it more
     * convenient to use. But one needs to know this is a PRIME DML
     * extension and may not be supported by other implementations.
     */
    void reset() { iter = collected.begin(); }

    /**
     * \brief Returns the total number of elements in the enumeration.
     *
     * Standard enumeration object does not support the this method.
     * The standard is that we don't know a priori the number of the
     * items in the enumeration until we scan through it. This
     * unnecessarily causes unwanted restrictions for programming. We
     * added the method here to make it more convenient to use. But
     * one needs to know this is a PRIME DML extension and may not be
     * supported by other implementations.
     */
    int size() { return collected.size(); }

  private:
    friend class dmlConfig;

    // A vector of nodes contained by the find method.
    std::vector<AttrNode*> collected;

    // This is an iterator of the above vector structure, which
    // indicates the item currently being numerated.
    std::vector<AttrNode*>::iterator iter;
  }; /*VectorEnumeration*/

  /**
   * \brief An abstract class that represents DML configuration.
   *
   * This abstract class is defined in the standard DML interface
   * specification. It is used as the base class to represent a DML
   * configuration or a attribute list (you may think of it as a node
   * in the DML tree that contains a list of attributes that you use
   * to configure a simulation model).
   */
  class Configuration {
  public:
    /**
     * \brief The constructor.
     *
     * The constructor does nothing other than setting the DNA of the
     * data structure. The DNA is a fixed bit pattern---a signature to
     * distinguish this attribute list structure (i.e., a compound DML
     * attribute value) from a single DML attribute value (which can
     * be either a number of a string).
     */
    Configuration() : DNA(DML_CONFIGURATION_DNA) {}

    /**
     * \brief The copy constructor.
     *
     * The copy constructor does nothing other than setting the DNA of
     * the data structure. The DNA is a fixed bit pattern---a
     * signature to distinguish this attribute list structure (i.e., a
     * compound DML attribute value) from a single DML attribute value
     * (which can be either a number of a string).
     */
    Configuration(const Configuration& cfg) :
      DNA(DML_CONFIGURATION_DNA) {}

    /// The destructor.
    virtual ~Configuration() {}

    /**
     * \brief Reclaims this configuration node.
     *
     * The user is strongly encouraged to use this method rather than
     * invoking the destructor of this class directly. This is because
     * internally we use a reference counter scheme to manage DML tree
     * nodes. The destructor should be called if and only if we know
     * that no outstanding references exist for a particular tree node
     * (which could be the root of the DML tree).
     */
    virtual void dispose() = 0;

    /**
     * \brief Returns the attribute value of a given key.
     * \param spec is the key of the attribute.
     * \returns the value of the attribute.
     *
     * There could be multiple attributes defined in the DML tree with
     * the same keypath. In this case, only one of them is returned by
     * this method. The DML standard does not specify the order of
     * attributes, and one should not make any presumptions about
     * which one is returned if multiple attributes are defined. NULL
     * is returned when no attribute is found. Note that the user
     * shall not reclaim the value returned from this method. This
     * resource is managed by the DML library. The value of the
     * attribute is an opaque type (void*). The user must cast the
     * return value to an appropriate type. In case the return value
     * is another attribute list, the user should cast it to a pointer
     * to Configuration. As a PRIME DML extension, one can use the
     * isConf() method to find out whether the returned value is a
     * single value or another attribute list.
     */
    virtual void* findSingle(char* spec) = 0;

    /**
     * \brief Returns a list of attribute values with the same key.
     * \param spec is the key of the attribute.
     * \returns an enumeration of the attributes of the given key.
     *
     * The ordering of these attributes is not preserved for
     * retrieval, according to the DML specification. It is guaranteed
     * that all attributes that share the same key will be
     * returned. The user can scan through the returned value as an
     * enumeration type. It is important to know that the enumeration
     * data structure itself should be reclaimed by the user
     * afterwards. However, the items listed in the enumeration are
     * not reclaimable, same as the value returned from the
     * findSingle() method.
     */
    virtual Enumeration* find(char* spec) = 0;

    /**
     * \brief Determines whether it is a single attribute value or a
     * DML configuration.
     * \param p is the attribute value returned from the findSingle()
     * or the find() method.
     * \returns true (non-zero) if it is a configuration.
     *
     * This method is used to determine whether a pointer returned by
     * the findSingle() method or an enumerated item in the
     * enumeration structure returned by the find() method is a
     * pointer to a Configuration object (a list of DML
     * attributes). This method is a PRIME DML extension. Note that
     * the method returns false (zero) if p is NULL.
     */
    static int isConf(void* p);

    /**
     * \brief Returns the key of the attribute.
     * \param p points to the attribute.
     *
     * This method returns the key of the attribute returned by the
     * findSingle() method or an enumerated item in the enumeration
     * structure returned by the find() method. This method is a
     * PRIME DML extension. The method returns NULL if p is NULL.
     */
    static char* getKey(void* p);

    /**
     * \brief Returns a new reference to this configuration node.
     *
     * This method is used to make a clone this attribute list
     * structure. This method will result a "shallow copy" of this
     * object, which means that, although all attributes in the list
     * will be cloned, only their reference counters will be
     * incremented. No new objects are created as a result of this
     * method call.
     */
    virtual Configuration* clone() = 0;

  private:
    /*
     * The first four bytes of this data structure has a unique bit
     * pattern, so that one can ask whether a pointer to this object
     * is really a configuration object or a simple string. This is
     * very useful for users to determine whether the value returned
     * from the findSingle() method or an item in the enumeration
     * returned from the find() method is a single string (which we
     * call singleton) or another list of attributes. This class
     * defines a static method called isConf to distinguish between
     * these two cases. The standard DML interface does not support
     * this capability, unfortunately. This is a PRIME DML extension.
     *
     * Note that we could have a problem here if the singleton
     * actually coincided with the signature. We made the signature
     * special in that we have two consecutive zeros in the
     * middle. Since the implementation concatenates the value and the
     * key as the return value of find() or findSingle() and since the
     * key can't be empty string, this will never happen.
     */
    u_int32_t DNA;

    // A DML configuration must have the following DNA string.
    const static u_int32_t DML_CONFIGURATION_DNA = 0xf000000f;
  }; /*Configuration*/

  /**
   * \brief Determines whether it is  a single attribute value or a DML configuration.
   * \param x is the attribute value returned from the Configuration::findSingle() or
   * the Configuration::find() method.
   * \returns true (non-zero) if it is a configuration.
   *
   * This macro is designed as a shortcut to the
   * s3f::dml::Configuration::isConf() method, which is used to test
   * whether a DML attribute value (returned by
   * s3f::dml::Configuration::findSingle() or
   * s3f::dml::Configuration::find() method) is a singleton or
   * another list of attributes (as of a Configuration object).
   */
#define DML_ISCONF(x) s3f::dml::Configuration::isConf(x)

  /**
   * \brief An abstract class that contains the config method.
   *
   * This is an abstract class defined in the DML interface
   * specification. It can be used as a base class for other classes
   * that define a config() method for retrieving the DML
   * configuration.
   */
  class Configurable {
  public:
    /// Virtual destructor.
    virtual ~Configurable() {}

    /// Customizes the object using a DML configuration.
    virtual void config(Configuration* cfg) = 0;
  }; /*Configurable*/

  /**
   * \brief DML configuration implementation.
   *
   * This class implements the Configuration class. This class also
   * defines the method for parsing and scanning the DML file.
   */
  class dmlConfig : public Configuration, public AttrNode {
  public:
    /**
     * \brief The constructor that parses the given DML file.
     * \param fname is the name of the DML file.
     *
     * If file name is NULL, an empty tree node is created.
     */
    dmlConfig(char* fname = 0);

    /**
     * \brief The constructor that parses the given DML files.
     * \param fnames is a null-terminated list of DML file names.
     *
     * Conceptually, the DML files are concatenated into a large file
     * before the parsing.
     */
    dmlConfig(char** fnames);

    /**
     * \brief The copy constructor.
     *
     * This copy constructor performs a "deep copy", in which case a
     * new DML tree will be created, which is identical to the current
     * DML tree.
     */
    dmlConfig(const dmlConfig& cfg) : Configuration(cfg), AttrNode(cfg) {}

    /**
     * \brief Makes a new reference to the DML tree.
     *
     * Cloning here means making a "shallow copy" of the DML node,
     * which means that only the reference counter of this object (as
     * well as the reference counters of all child nodes in the DML
     * sub-tree) will be incremented. No new object will be created as
     * a result of this method call.
     */
    virtual Configuration* clone() { return (dmlConfig*)AttrNode::clone(); }

    /**
     * \brief The destructor.
     *
     * This destructor does nothing in particular. Internally, the
     * destructor in the parent class will take care of reclaiming the
     * tree nodes. The user is not expected to call this method or
     * delete the object directly. Instead, the user should call
     * dispose() to reclaim the DML node.
     */
    virtual ~dmlConfig() {}

    /**
     * \brief Reclaims the DML node.
     * \sa Configuration::dispose().
     *
     * The user is strongly encouraged to call this method rather than
     * the destructor (i.e., using the delete method).
     */
    virtual void dispose() { AttrNode::dispose(); }

    /**
     * \brief Load the given DML file.
     * \param fname is the name of the DML file.
     *
     * This method is called to parse the given DML file. The current
     * DML tree will be reclaimed to make room for the new DML
     * tree. Typically, a DML node is created empty and then filled in
     * using this method. Nothing will happen if the given file name
     * is a NULL.
     */
    void load(char* fname = 0);

    /**
     * \brief Load the given DML files.
     * \param fnames is a null-terminated list of DML file names.
     *
     * This method is called to parse the given list of DML file. The
     * current DML tree will be reclaimed to make room for the new DML
     * tree. Typically, a DML node is created empty and then filled in
     * using this method. Nothing will happen if the list is empty.
     */
    void load(char** fnames);
  
    /**
     * \brief Returns the attribute value of a given key.
     * \param spec is the key of the attribute.
     * \returns the value of the attribute.
     * \sa Configuration::find().
     */
    virtual void* findSingle(char* spec);

    /**
     * \brief Returns a list of attribute values with the same key.
     * \param spec is the key of the attribute.
     * \returns an enumeration of the attributes of the given key.
     * \sa Configuration::find().
     */
    virtual Enumeration* find(char* spec);

  public: // treat the following as private
    // Constructor: add the first child; used internally by parser.
    dmlConfig(dmlConfig* firstkid);

    // Constructor: this is a leaf node; used internally by parser.
    dmlConfig(AttrKey* key, KeyValue* keyval);

    // Load from the given DML file.
    void loading(char* filename);

    // Load from the (null-terminated) list of DML files.
    void loading(char** filenames);
  
    /*  
     * This method copies the child attributes of the given DML tree
     * root node. Used internally to connect with the parser.
     */
    void first_level_copy(dmlConfig* root);

    /*
     * The DML tree structure maintains it own dictionary. All strings
     * used in the DML file are stored in this dictionary.  References
     * to the same string are aliased to same memory location.  This
     * field should be accessed only by the dictionary() method.
     */
    static Dictionary* dict;

    /*
     * This method returns the pointer to the one and only one
     * dictionary object defined for the entire process that uses the
     * DML library. Note that this arrangement is not
     * "thread-safe". That is, if the DML library is used by multiple
     * processes, some protection is expected for mutual exclusion
     * since the dictionary object (declared here globally) will be
     * shared by all processes.
     */
    static Dictionary* dictionary();

    /*
     * The following variables are used to support the PRIME DML
     * extension of listing attributes in a DML sub-tree. In our
     * implementation, one can call the find() method using a wildcard
     * (such as "*"), in which case all attributes that match with the
     * wildcard will be returned. To help the user distinguish the
     * attributes returned by this method. We concatenate the DML
     * value and key (separated by a zero) for each enumerated item
     * returned from the find() method.
     */
    static int attrbufsz;
    static char* attrbuf;
    static char* attribute_buffer(int size);
  }; /*dmlConfig*/

}; // namespace dml
}; // namespace s3f

#endif /*__PRIME_DML_H__*/

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
