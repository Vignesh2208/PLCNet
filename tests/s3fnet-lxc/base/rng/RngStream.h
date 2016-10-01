/**
 * \file RngStream.h
 *
 * \brief Header file of RngStream class, %Random number stream generator.
 *
 */

#ifndef RNGSTREAM_H
#define RNGSTREAM_H
 
#include <string>
#include <pthread.h>

/**
 * \namespace RNGS %Random Number Stream
 */
namespace RNGS {
/**
 *  %Random number stream generator.
 */
class RngStream
{
public:
  /** constructor */
  RngStream ();
  static bool SetPackageSeed (const unsigned long seed[6]);
  /** Reset Stream to beginning of Stream. */
  void ResetStartStream ();
  /** Reset Stream to beginning of SubStream. */
  void ResetStartSubstream ();
  /**  Reset Stream to NextSubStream. */
  void ResetNextSubstream ();

  void SetAntithetic (bool a);
  void IncreasedPrecis (bool incp);
  bool SetSeed (const unsigned long seed[6]);
  /** Jump n steps forward if n > 0, backwards if n < 0.
   * if e > 0, let n = 2^e + c;
   * if e < 0, let n = -2^(-e) + c;
   * if e = 0, let n = c.
   */
  void AdvanceState (long e, long c);
  void GetState (unsigned long seed[6]) const;
  void WriteState () const;
  void WriteStateFull () const;
  /** Generate the next random number. */
  double RandU01 ();
  /** Generate the next random integer. */
  int RandInt (int i, int j);
  static pthread_mutex_t nextState_lock;

private:
  double Cg[6], Bg[6], Ig[6];
  bool anti, incPrec;
  std::string name;
  /** The default seed of the package; will be the seed of the first declared RngStream, unless SetPackageSeed is called. */
  static double nextSeed[6];
  /** Generate the next random number. */
  double U01 ();
  /** Generate the next random number with extended (53 bits) precision. */
  double U01d ();
 };
}

#endif
 

