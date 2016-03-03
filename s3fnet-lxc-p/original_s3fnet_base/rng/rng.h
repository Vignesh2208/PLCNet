/**
 * \file rng.h
 *
 * \brief Header file of RNG class, %Random Number Generator.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string>

using namespace std;
#include "binsearch.h"
#include "RngStream.h"

/**
 * \namespace Random %Random Number Generator
 */
namespace Random {
class RngStream;

/* ------------------------------------------------------------------------- */
/** These are C++ lang routines which can be used to evaluate the probability
 * density functions (pdf's), cumulative distribution functions (cdf's), and
 * inverse distribution functions (idf's) for a variety of discrete and
 * continuous random variables.
 *
 * The following notational conventions are used:
 *                 <BR> x : possible value of the random variable
 *                 <BR> u : real variable (probability) between 0 and 1
 *  <BR> a, b, n, p, m, s : distribution specific parameters
 *
 * There are pdf's, cdf's and idf's for 6 discrete random variables:
 *
 *      Random Variable    Range (x)  Mean         Variance
 *
 *      Bernoulli(p)       0..1       p            p*(1-p)
 *      Binomial(n, p)     0..n       n*p          n*p*(1-p)
 *      Equilikely(a, b)   a..b       (a+b)/2      ((b-a+1)*(b-a+1)-1)/12
 *      Geometric(p)       0...       p/(1-p)      p/((1-p)*(1-p))
 *      Pascal(n, p)       0...       n*p/(1-p)    n*p/((1-p)*(1-p))
 *      Poisson(m)         0...       m            m
 *
 * and for 8 continuous random variables:
 *
 *      Random Variable    Range (x)  Mean         Variance
 *
 *      Uniform(a, b)      a < x < b  (a+b)/2      (b-a)*(b-a)/12
 *      Exponential(m)     x > 0      m            m*m
 *      Erlang(n, b)       x > 0      n*b          n*b*b
 *      Normal             all x      0            1
 *      Gauss(m, s)        all x      m            s*s
 *      Lognormal(a, b)    x > 0         see below
 *      Chisquare(n)       x > 0      n            2*n
 *      Student(n)         all x      0  (n > 1)   n/(n-2)   (n > 2)
 *
 * For the Lognormal(a, b), the mean and variance are:
 *
 *                        mean = Exp(a + 0.5*b*b)
 *                    variance = (Exp(b*b) - 1)*Exp(2*a + b*b)
 *
 * <BR> Name            : DFU.C
 * <BR> Purpose         : Distribution/Density Function Routines
 * <BR> Author          : Steve Park
 * <BR> Language        : Turbo Pascal, 5.0
 * <BR> Latest Revision : 09-19-90
 * <BR> Reference       : Lecture Notes on Simulation, by Steve Park
 * <BR> Converted to C  : David W. Geyer  09-02-91
 * ------------------------------------------------------------------------- */
/* NOTE - must link with routine sfu.o */

class RNG {
 public:
  RNG();
  RNGS::RngStream* __rngStream;
  double Random();
  ~RNG() {};

/* include files */
#define ABS(a) ( (a > 0) ? a : -(a) )

/* ========================================================================= */
/*                         double Bernoulli_pdf()                            */
/*                         double Bernoulli_cdf()                            */
/*                          long  Bernoulli_idf()                            */
/*                                                                           */
/* NOTE: use 0 < p < 1, 0 <= x <= 1 and 0 < u < 1                            */
/* ========================================================================= */
double Bernoulli_pdf(double p, long x);
double Bernoulli_cdf(double p, long x);
long Bernoulli_idf(double p, double u);
double Equilikely_pdf(long a, long b, long x);
double Equilikely_cdf(long a, long b, long x);
long Equilikely_idf(long a, long b, double u);
double Binomial_pdf(long n, double p, long x);
double Binomial_cdf(long n, double p, long x);
long Binomial_idf(long n, double p, double u);
double Geometric_pdf(double p, long x);
double Geometric_cdf(double p, long x);
long Geometric_idf(double p, double u);
double Pascal_pdf(long n, double p, long x);
double Pascal_cdf(long n, double p, long x);
long Pascal_idf(long n, double p, double u);
double Poisson_pdf(double m, long x);
double Poisson_cdf(double m, long x);
long Poisson_idf(double m, double u);

double Uniform_pdf(double a, double b, double x);
double Uniform_cdf(double a, double b, double x);
double Uniform_idf(double a, double b, double u);
double Exponential_pdf(double m, double x);
double Exponential_cdf(double m, double x);
double Exponential_idf(double m, double u);
double Erlang_pdf(long n, double b, double x);
double Erlang_cdf(long n, double b, double x);
double Erlang_idf(long n, double b, double u);
double Normal_pdf (double x);
double Normal_cdf (double x);
double Normal_idf (double u);
double Gauss_pdf(double m, double s, double x);
double Gauss_cdf(double m, double s, double x);
double Gauss_idf(double m, double s, double u);
double Lognormal_pdf(double a, double b, double x);
double Lognormal_cdf(double a, double b, double x);
double Lognormal_idf(double a, double b, double u);
double Chisquare_pdf(long n, double x);
double Chisquare_cdf(long n, double x);
double Chisquare_idf(long n, double u);
double Student_pdf(long n, double x);
double Student_cdf(long n, double x);
double Student_idf(long n, double u);

double Uniform(double a, double b);
double Exponential(double m);
double Erlang(long n, double b);
double Normal();
double Gauss(double m, double s);
double Lognormal(double a, double b);
double Chisquare(long n);
double Student(long n);

double Ln_Gamma(double a);
double Ln_Factorial(long n);
double Ln_Beta(double a, double b);
double Ln_Choose(long n, long m);
double Incomplete_Gamma(double a, double x);
double Incomplete_Beta(double a, double b, double x);

long Bernoulli(double p);
long Binomial(long n, double p);
long Equilikely(long a, long b);
long Geometric(double p);
long Pascal(long n, double p);
long Poisson(double m);
};
}
