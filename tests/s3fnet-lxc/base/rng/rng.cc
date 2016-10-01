/**
 * \file rng.cc
 *
 * \brief Source file of RNG class, %Random Number Generator.
 *
 */

#include "rng.h"
using namespace Random;
RNG::RNG() {
  __rngStream = new RNGS::RngStream();
  // pthread_mutex_init(&(__rngStream->nextState_lock), NULL);
}

double RNG::Random() { return __rngStream->RandU01(); }

/* ------------------------------------------------------------------------- */
/* These are C lang routines for generating random variables from eight      */
/* continuous distributions.                                                 */
/*                                                                           */
/* There are generators for 8 continuous distributions:                      */
/*                                                                           */
/*      Generator        Range (x)    Mean           Variance                */
/*                                                                           */
/*      Uniform(a, b)    a < x < b    (a+b)/2        (b-a)*(b-a)/12          */
/*      Exponential(m)   x > 0        m              m*m                     */
/*      Erlang(n, b)     x > 0        n*b            n*b*b                   */
/*      Normal           all x        0              1                       */
/*      Gauss(m, s)      all x        m              s*s                     */
/*      Lognormal(a, b)  x > 0           see below                           */
/*      Chisquare(n)     x > 0        n              2*n                     */
/*      Student(n)       all x        0  (n > 1)     n/(n - 2)   (n > 2)     */
/*                                                                           */
/* For the Lognormal(a, b), the mean and variance are:                       */
/*                                                                           */
/*                        mean = Exp(a + 0.5*b*b)                            */
/*                    variance = (Exp(b*b) - 1)*Exp(2*a + b*b)               */
/*                                                                           */
/*                                                                           */
/* Name            : CGU.C                                                   */
/* Purpose         : Continuous Generator Routines                           */
/* Author          : Steve Park                                              */
/* Language        : Turbo Pascal, 5.0                                       */
/* Latest Revision : 10-24-90                                                */
/* Reference       : Lecture Notes on Simulation, by Steve Park              */
/* Converted to C  : David W. Geyer  09-03-91                                */
/* ------------------------------------------------------------------------- */
/* NOTE - must link with routine rng.obj */

/* include files */
#include <math.h>                          /* for log(), exp() */

/* ========================================================================= */
/*                             double Uniform()                              */
/* ========================================================================= */
/* Returns a uniformly distributed real between a and b.                     */
/* NOTE: use a < b                                                           */
/* ========================================================================= */
double RNG::Uniform(double a, double b)
{ 
  return(a + (b - a) * Random());
}  /* Uniform */

/* ========================================================================= */
/*                             double Exponential()                          */
/* ========================================================================= */
/* Returns an exponentially distributed positive real.                       */
/* NOTE: use m > 0                                                           */
/* ========================================================================= */
double RNG::Exponential(double m)
{ 
  return(- m * log(1.0 - Random()));
} /* Exponential */

/* ========================================================================= */
/*                             double Erlang()                               */
/* ========================================================================= */
/* Returns an Erlang distributed positive real.                              */
/* NOTE: use n > 0 and b > 0                                                 */
/* ========================================================================= */
double RNG::Erlang(long n, double b)
{ long   i;
  double x;

  x = 0.0;
  for (i=0;i < n;i++)
    x += Exponential(b);
  return(x);
} /* Erlang */

/* ========================================================================= */
/*                             double Normal()                               */
/* ========================================================================= */
/* Returns a standard normal distributed real.                               */
/* Uses a very accurate approximation of the Normal idf due to Odeh & Evans, */
/* J. Applied Statistics, 1974, vol 23, pp 96-97.                            */
/* ========================================================================= */
double RNG::Normal()
{ double p0 = 0.322232431088;        double q0 = 0.099348462606;
  double p1 = 1.0;                   double q1 = 0.588581570495;
  double p2 = 0.342242088547;        double q2 = 0.531103462366;
  double p3 = 0.204231210245e-1;     double q3 = 0.103537752850;
  double p4 = 0.453642210148e-4;     double q4 = 0.385607006340e-2;
  double u, t, p, q; 

  u = Random();
  if (u < 0.5)
    t = sqrt(- 2 * log(u));
  else
    t = sqrt(- 2 * log(1.0 - u));
  p = p0 + t * (p1 + t * (p2 + t * (p3 + t * p4)));
  q = q0 + t * (q1 + t * (q2 + t * (q3 + t * q4)));
  if (u < 0.5)
    return( (p / q) - t );
  else
    return( t - (p / q) );
}  /* Normal */

/* ========================================================================= */
/*                             double Gauss()                                */
/* ========================================================================= */
/* Returns a Gaussian distributed real.                                      */
/* NOTE: use s > 0                                                           */
/* ========================================================================= */
double RNG::Gauss(double m, double s)
{ 
  return(m + s * Normal());
}  /* Gauss */

/* ========================================================================= */
/*                             double Lognormal()                            */
/* ========================================================================= */
/* Returns a lognormal distributed positive real.                            */
/* NOTE: use b > 0                                                           */
/* ========================================================================= */
double RNG::Lognormal(double a, double b)
{ 
  return( exp(a + b * Normal()) );
}  /* Lognormal */

/* ========================================================================= */
/*                             double Chisquare()                            */
/* ========================================================================= */
/* Returns a chi-square distributed positive real.                           */
/* NOTE: use n > 0                                                           */
/* ========================================================================= */
double RNG::Chisquare(long n)
{ long    i;
  double  z, x;

  x = 0.0;
  for(i=0;i < n;i++)  {
    z = Normal();
    x += z * z;
  }
  return(x);
} /* Chisquare */

/* ========================================================================= */
/*                             double Student()                              */
/* ========================================================================= */
/* Returns a Student-t distributed real.                                     */
/* NOTE: use n > 0                                                           */
/* ========================================================================= */
double RNG::Student(long n)
{ 
  return( Normal() / sqrt(Chisquare(n) / (double)n) );
} /* Student */


/* ------------------------------------------------------------------------- */
/* These are C lang routines for generating random variables from six        */
/* discrete distributions.                                                   */
/*                                                                           */
/* There are generators for the following 6 discrete distributions:          */
/*                                                                           */
/*      Generator           Range     Mean         Variance                  */
/*                                                                           */
/*      Bernoulli(p)        0..1      p            p*(1-p)                   */
/*      Binomial(n, p)      0..n      n*p          n*p*(1-p)                 */
/*      Equilikely(a, b)    a..b      (a+b)/2      ((b-a+1)*(b-a+1)-1)/12    */
/*      Geometric(p)        0...      p/(1-p)      p/((1-p)*(1-p))           */
/*      Pascal(n, p)        0...      n*p/(1-p)    n*p/((1-p)*(1-p))         */
/*      Poisson(m)          0...      m            m                         */
/*                                                                           */
/* Name            : DGU.C                                                   */
/* Purpose         : Discrete Generator Routines                             */
/* Author          : Steve Park                                              */
/* Language        : Turbo Pascal, 5.0                                       */
/* Latest Revision : 09-22-90                                                 */
/* Reference       : Lecture Notes on Simulation, by Steve Park              */
/* Converted to C  : David W. Geyer  09-02-91                                */
/* ------------------------------------------------------------------------- */

/* ========================================================================= */
/*                             long Bernoulli()                              */
/* ========================================================================= */
long RNG::Bernoulli(double p)
/* Returns 1 with probability p or 0 with probability 1 - p.
   NOTE: use 0 < p < 1                                       */
{ 
  if (Random() < (1 - p))
    return((long) 0);
  else
    return((long) 1);
}  /* Bernoulli */

/* ========================================================================= */
/*                             long Binomial()                               */
/* ========================================================================= */
long RNG::Binomial(long n, double p)
/* Returns a binomial distributed integer between 0 and n inclusive.
   NOTE: use n > 0 and 0 < p < 1                                     */
{ long i, x = 0;

  for(i=0;i < n;i++)
    x += Bernoulli(p);
  return(x);
}  /* Binomial */

/* ========================================================================= */
/*                            long Equilikely()                              */
/* ========================================================================= */
long RNG::Equilikely(long a, long b)
/* Returns an equilikely distributed integer between a and b inclusive.
   NOTE: use a < b                                                      */
{ 
  return(a + (long)((b - a + 1) * Random()));
}  /* Equilikely */

/* ========================================================================= */
/*                            long Geometric()                               */
/* ========================================================================= */
long RNG::Geometric(double p)
/* Returns a geometric distributed non-negative integer.
   NOTE: use 0 < p < 1                                   */
{ 
  return( (long)( (log(1 - Random()) / log(p)) ) );
}  /* Geometric */

/* ========================================================================= */
/*                            long Pascal()                                  */
/* ========================================================================= */
long RNG::Pascal(long n, double p)
/* Returns a Pascal distributed non-negative integer.
   NOTE: use n > 0 and 0 < p < 1                      */
{ long i, x = 0;

  for (i=0;i < n;i++)
    x += Geometric(p);
  return(x);
}  /* Pascal */

/* ========================================================================= */
/*                            long Poisson()                                 */
/* ========================================================================= */
long RNG::Poisson(double m)
/* Returns a Poisson distributed non-negative integer.
   NOTE: use m > 0                                     */
{ long   x = 0;
  double t;

  t = exp(m);
  do {
    x++;
    t *= Random();
  } while (t >= 1);
  return(x - 1);
}  /* Poisson */


/* ------------------------------------------------------------------------- */
/* These are C lang routines which can be used to evaluate some of the       */
/* 'special' functions which arrise in the modeling and analysis of various  */
/* statistical distributions.                                                */
/*                                                                           */
/* Name            : SFU.C                                                   */
/* Purpose         : Special Function Routines                               */
/* Author          : Steve Park                                              */
/* Language        : Turbo Pascal, 5.0                                       */
/* Latest Revision : 09-19-90                                                */
/* Reference       : Lecture Notes on Simulation, by Steve Park              */
/* Converted to C  : David W. Geyer  09-02-91                                */
/* ------------------------------------------------------------------------- */
/* include files */

/* ========================================================================= */
/*                             double Ln_Gamma()                             */
/* ========================================================================= */
/* Ln_Gamma returns the natural log of the gamma function.                   */
/* NOTE: use a > 0.0                                                         */
/*                                                                           */
/* The algorithm used to evaluate the natural log of the gamma function is   */
/* based on an approximation by C. Lanczos, SIAM J. Numerical Analysis, B,   */
/* vol 1, 1964.  The constants have been selected to yield a relative error  */
/* which is less than 2.0e-10 for all positive values of the parameter a.    */
/* ========================================================================= */
double RNG::Ln_Gamma(double a)
{ double c = 2.506628274631;                     /* Sqrt(2 * pi) */
  double s[6], sum, temp;
  int    i;

  s[0] =  76.180091729406 / a;
  s[1] = -86.505320327112 / (a + 1);
  s[2] =  24.014098222230 / (a + 2);
  s[3] =  -1.231739516140 / (a + 3);
  s[4] =   0.001208580030 / (a + 4);
  s[5] =  -0.000005363820 / (a + 5);
  sum  =   1.000000000178;
  for (i=0;i < 6;i++)
    sum += s[i];
  temp = (a - 0.5) * log(a + 4.5) - (a + 4.5) + log(c * sum);
  return(temp);
}  /* Ln_Gamma */

/* ========================================================================= */
/*                         double Ln_Factorial()                             */
/* ========================================================================= */
/* Ln_Factorial(n) returns the natural log of n!                             */
/* NOTE: use n >= 0                                                          */
/*                                                                           */
/* The algorithm used to evaluate the natural log of n! is based on a        */
/* simple equation which relates the gamma and factorial functions.          */
/* ========================================================================= */
double RNG::Ln_Factorial(long n)
{ 
  return(Ln_Gamma((double)n + (double)1.0));
}  /* Ln_Factorial */

/* ========================================================================= */
/*                            double Ln_Beta()                               */
/* ========================================================================= */
/* Ln_Beta returns the natural log of the beta function.                     */
/* NOTE: use a > 0.0 and b > 0.0                                             */
/*                                                                           */
/* The algorithm used to evaluate the natural log of the beta function is    */
/* based on a simple equation which relates the gamma and beta functions.    */
/* ========================================================================= */
double RNG::Ln_Beta(double a, double b)
{ double temp;

  temp    = Ln_Gamma(a) + Ln_Gamma(b) - Ln_Gamma(a + b);
  return(temp);
}  /* Ln_Beta */

/* ========================================================================= */
/*                            double Ln_Choose()                             */
/* ========================================================================= */
/* Ln_Choose returns the natural log of the binomial coefficient C(n,m).     */
/* NOTE: use 0 <= m <= n                                                     */
/*                                                                           */
/* The algorithm used to evaluate the natural log of a binomial coefficient  */
/* is based on a simple equation which relates the beta function to a        */
/* binomial coefficient.                                                     */
/* ========================================================================= */
double RNG::Ln_Choose(long n, long m)
{

  if (m > 0)
    return(- Ln_Beta((double)m, (double)(n - m + 1)) - log((double)m));
  else
    return((double) 0.0);
}  /* Ln_Choose */

/* ========================================================================= */
/*                     double Incomplete_Gamma()                             */
/* ========================================================================= */
/* Evaluates the incomplete gamma function.                                  */
/* NOTE: use a > 0.0 and x >= 0.0                                            */
/*                                                                           */
/* The algorithm used to evaluate the incomplete gamma function is based on  */
/* Algorithm AS 32, J. Applied Statistics, 1970, by G. P. Bhattacharjee.     */
/* See also equations 6.5.29 and 6.5.31 in the Handbook of Mathematical      */
/* Functions, Abramowitz and Stegum (editors).  The absolute error is less   */
/* than 1e-10 for all non-negative values of x.                              */
/* ========================================================================= */
double RNG::Incomplete_Gamma(double a, double x)
{ double tiny = 1.0e-10;                      /* convergence parameter */
  double t, sum, term, factor, f, g, c[2], p[3], q[3];
  long   n;

  if (x > 0.0)
    factor = exp(-x + a * log(x) - Ln_Gamma(a));
  else
    factor = 0.0;
  if (x < a + 1.0) {                 /* evaluate as an infinite series - */
      t    = a;                      /* A & S equation 6.5.29            */
      term = 1.0 / a;
      sum  = term;
      do {                           /* sum until 'term' is small */
        t++;
        term *= x / t;
        sum  += term;
      } while (term >= tiny * sum);
      return(factor * sum);
  }

  else  {                            /* evaluate as a continued fraction - */
    p[0]  = 0.0;                     /* A & S eqn 6.5.31 with the extended */
    q[0]  = 1.0;                     /* pattern 2-a, 2, 3-a, 3, 4-a, 4,... */
    p[1]  = 1.0;                     /* - see also A & S sec 3.10, eqn (3) */
    q[1]  = x;
    f     = p[1] / q[1];
    n     = 0;
    do {                             /* recursively generate the continued */
      g  = f;                        /* fraction 'f' until two consecutive */
      n++;                           /* values are small                   */
      if ((n%2) > 0) {
        c[0] = ((double)(n + 1) / 2) - a;
        c[1] = 1.0;
      }
      else  {
        c[0] = (double)(n / 2);
        c[1] = x;
      }
      p[2] = c[1] * p[1] + c[0] * p[0];
      q[2] = c[1] * q[1] + c[0] * q[0];
      if (q[2] != 0.0)  {             /* rescale to avoid overflow */
          p[0] = p[1] / q[2];
          q[0] = q[1] / q[2];
          p[1] = p[2] / q[2];
          q[1] = 1.0;
          f    = p[1];
      }
    } while ((fabs(f - g) >= tiny) || (q[1] != 1.0));
    return((double)1.0 - factor * f);
  }
}  /* Incomplete_Gamma */

/* ========================================================================= */
/*                     double Incomplete_Beta()                              */
/* ========================================================================= */
/* Evaluates the incomplete beta function.                                   */
/* NOTE: use a > 0.0, b > 0.0 and 0.0 <= x <= 1.0                            */
/*                                                                           */
/* The algorithm used to evaluate the incomplete beta function is based on   */
/* equation 26.5.8 in the Handbook of Mathematical Functions, Abramowitz     */
/* and Stegum (editors).  The absolute error is less than 1e-10 for all x    */
/* between 0 and 1.                                                          */
/* ========================================================================= */
double RNG::Incomplete_Beta(double a, double b, double x)
{ double tiny = 1.0e-10;                      /* convergence parameter */
  double t, factor, f, g, c, p[3], q[3];
  int swap;
  long   n;

  if (x > (a + 1) / (a + b + 1)) {   /* to accelerate convergence   */
    swap = 1;                        /* complement x and swap a & b */
    x    = 1 - x;
    t    = a;
    a    = b;
    b    = t;
  }
  else                                /* do nothing */
    swap = 0;
  if (x > 0)
    factor = exp(a * log(x) + b * log(1 - x) - Ln_Beta(a,b)) / a;
  else
    factor = 0;
  p[0] = 0;
  q[0] = 1;
  p[1] = 1;
  q[1] = 1;
  f    = p[1] / q[1];
  n    = 0;
  do {                               /* recursively generate the continued */
    g = f;                           /* fraction 'f' until two consecutive */
    n++;                             /* values are small                   */
    if ((n%2) > 0)  {
      t = (n - 1) / 2;
      c = -(a + t) * (a + b + t) * x / ((a + n - 1) * (a + n));
    }
    else  {
      t = n / 2;
      c = t * (b - t) * x / ((a + n - 1) * (a + n));
    }
    p[2] = p[1] + c * p[0];
    q[2] = q[1] + c * q[0];
    if (q[2] != 0)  {                /* rescale to avoid overflow */
      p[0] = p[1] / q[2];
      q[0] = q[1] / q[2];
      p[1] = p[2] / q[2];
      q[1] = 1.0;
      f    = p[1];
    }
  } while ((ABS(f - g) >= tiny) || (q[1] != 1));
  if (swap)
    return(1.0 - factor * f);
  else
    return(factor * f);
}  /* Incomplete_Beta */

/* ------------------------------------------------------------------------- */
/* These are C lang routines which can be used to evaluate the probability   */
/* density functions (pdf's), cumulative distribution functions (cdf's), and */
/* inverse distribution functions (idf's) for a variety of discrete and      */
/* continuous random variables.                                              */
/*                                                                           */
/* The following notational conventions are used -                           */
/*                 x : possible value of the random variable                 */
/*                 u : real variable (probability) between 0 and 1           */
/*  a, b, n, p, m, s : distribution specific parameters                      */
/*                                                                           */
/* There are pdf's, cdf's and idf's for 6 discrete random variables:         */
/*                                                                           */
/*      Random Variable    Range (x)  Mean         Variance                  */
/*                                                                           */
/*      Bernoulli(p)       0..1       p            p*(1-p)                   */
/*      Binomial(n, p)     0..n       n*p          n*p*(1-p)                 */
/*      Equilikely(a, b)   a..b       (a+b)/2      ((b-a+1)*(b-a+1)-1)/12    */
/*      Geometric(p)       0...       p/(1-p)      p/((1-p)*(1-p))           */
/*      Pascal(n, p)       0...       n*p/(1-p)    n*p/((1-p)*(1-p))         */
/*      Poisson(m)         0...       m            m                         */
/*                                                                           */
/* and for 8 continuous random variables:                                    */
/*                                                                           */
/*      Random Variable    Range (x)  Mean         Variance                  */
/*                                                                           */
/*      Uniform(a, b)      a < x < b  (a+b)/2      (b-a)*(b-a)/12            */
/*      Exponential(m)     x > 0      m            m*m                       */
/*      Erlang(n, b)       x > 0      n*b          n*b*b                     */
/*      Normal             all x      0            1                         */
/*      Gauss(m, s)        all x      m            s*s                       */
/*      Lognormal(a, b)    x > 0         see below                           */
/*      Chisquare(n)       x > 0      n            2*n                       */
/*      Student(n)         all x      0  (n > 1)   n/(n-2)   (n > 2)         */
/*                                                                           */
/* For the Lognormal(a, b), the mean and variance are:                       */
/*                                                                           */
/*                        mean = Exp(a + 0.5*b*b)                            */
/*                    variance = (Exp(b*b) - 1)*Exp(2*a + b*b)               */
/*                                                                           */
/* Name            : DFU.C                                                   */
/* Purpose         : Distribution/Density Function Routines                  */
/* Author          : Steve Park                                              */
/* Language        : Turbo Pascal, 5.0                                       */
/* Latest Revision : 09-19-90                                                */
/* Reference       : Lecture Notes on Simulation, by Steve Park              */
/* Converted to C  : David W. Geyer  09-02-91                                */
/* ------------------------------------------------------------------------- */
/* NOTE - must link with routine sfu.o */

/* include files */
#include "binsearch.h"

/* ========================================================================= */
/*                         double Bernoulli_pdf()                            */
/*                         double Bernoulli_cdf()                            */
/*                          long  Bernoulli_idf()                            */
/*                                                                           */
/* NOTE: use 0 < p < 1, 0 <= x <= 1 and 0 < u < 1                            */
/* ========================================================================= */
double RNG::Bernoulli_pdf(double p, long x)
{
  if (x == 0)
    return((double)1.0 - p);
  else
    return(p);
}  /* Bernoulli_pdf */

double RNG::Bernoulli_cdf(double p, long x)
{
  if (x == 0)
    return((double)1.0 - p);
  else
    return((double)1.0);
}  /* Bernoulli_cdf */

long RNG::Bernoulli_idf(double p, double u)
{
  if (u < ((double)1.0 - p))
    return((long)0.0);
  else
    return((long)1.0);
}  /* Bernoulli_idf */

/* ========================================================================= */
/*                        double Equilikely_pdf()                            */
/*                        double Equilikely_cdf()                            */
/*                         long  Equilikely_idf()                            */
/*                                                                           */
/* NOTE: use a <= x <= b and 0 < u < 1                                       */
/* ========================================================================= */
double RNG::Equilikely_pdf(long a, long b, long x)
{
  return( (double)1.0 / ((double)b - (double)a + (double)1.0) );
}  /* Equilikely_pdf */

double RNG::Equilikely_cdf(long a, long b, long x)
{
  return( ((double)x - (double)a + (double)1.0) /
          ((double)b - (double)a + (double)1.0)  );
}  /* Equilikely_cdf */

long RNG::Equilikely_idf(long a, long b, double u)
{
  return(a + (long)(u * (b - a + 1)) );
}  /* Equilikely_idf */

/* ========================================================================= */
/*                          double Binomial_pdf()                            */
/*                          double Binomial_cdf()                            */
/*                           long  Binomial_idf()                            */
/*                                                                           */
/* NOTE: use 0 <= x <= n, 0 < p < 1 and 0 < u < 1                            */
/* ========================================================================= */
double RNG::Binomial_pdf(long n, double p, long x)
{ double s, t;

  s = Ln_Choose(n, x);                   /* break this calculation        */
  t = x * log(p) + (n - x) * log(1 - p); /* into two parts                */
  return(exp(s + t));                    /* to avoid 80x87 stack overflow */
}  /* Binomial_pdf */

double RNG::Binomial_cdf(long n, double p, long x)
{ 
  if (x < n)
    return((double)1.0 - Incomplete_Beta((double)(x+1), (double)(n-x), p));
  else
    return((double)1.0);
}  /* Binomial_cdf */

long RNG::Binomial_idf(long n, double p, double u)
{ long  x;
  x = (long)(n * p);                         /* start searching at the mean */
  if (Binomial_cdf(n, p, x) <= u)
    while (Binomial_cdf(n, p, x) <= u)  {
      x++;
    }
  else if (Binomial_cdf(n, p, 0) <= u)
    while (Binomial_cdf(n, p, x - 1) > u)  {
      x--;
    }
  else
    x = 0;
  return(x);
}  /* Binomial_idf */

/* ========================================================================= */
/*                         double Geometric_pdf()                            */
/*                         double Geometric_cdf()                            */
/*                          long  Geometric_idf()                            */
/*                                                                           */
/* NOTE: use 0 < p < 1, x >= 0 and 0 < u < 1                                 */
/* ========================================================================= */
double RNG::Geometric_pdf(double p, long x)
{
  return( (1.0 - p) * exp(x * log(p)) );
}  /* Geometric_pdf */

double RNG::Geometric_cdf(double p, long x)
{
  return( 1.0 - exp((x + 1) * log(p)) );
}  /* Geometric_cdf */

long RNG::Geometric_idf(double p, double u)
{
  return( (long)(log(1.0 - u) / log(p)) );
}  /* Geometric_idf */

/* ========================================================================= */
/*                            double Pascal_pdf()                            */
/*                            double Pascal_cdf()                            */
/*                             long  Pascal_idf()                            */
/*                                                                           */
/* NOTE: use n >= 1, 0 < p < 1, x >= 0 and 0 < u < 1                         */
/* ========================================================================= */
double RNG::Pascal_pdf(long n, double p, long x)
{ double  s, t;

  s = Ln_Choose(n + x - 1, x);            /* break this calculation        */
  t = x * log(p) + n * log(1.0 - p);      /* into two parts                */
  return(exp(s + t));                     /* to avoid 80x87 stack overflow */
}  /* Pascal_pdf */

double RNG::Pascal_cdf(long n, double p, long x)
{ 
  return(1.0 - Incomplete_Beta(x + 1, (double)n, p));
}  /* Pascal_cdf */

long RNG::Pascal_idf(long n, double p, double u)
{ long  x;

  x = (long)(n * p / (1.0 - p));            /* start searching at the mean */
  if (Pascal_cdf(n, p, x) <= u)
    while (Pascal_cdf(n, p, x) <= u)  {
      x++;
    }
  else if (Pascal_cdf(n, p, 0) <= u)
    while (Pascal_cdf(n, p, x - 1) > u)  {
      x--;
    }
  else
    x = 0;
  return(x);
}  /* Pascal_idf */

/* ========================================================================= */
/*                          double Poisson_pdf()                             */
/*                          double Poisson_cdf()                             */
/*                           long  Poisson_idf()                             */
/*                                                                           */
/* NOTE: use m > 0, x >= 0 and 0 < u < 1                                     */
/* ========================================================================= */
double RNG::Poisson_pdf(double m, long x)
{ double t;

  t = - m + x * log(m) - Ln_Factorial(x);
  return(exp(t));
}  /* Poisson_pdf */

double RNG::Poisson_cdf(double m, long x)
{ 
  return((double)1.0 - Incomplete_Gamma((double)x + 1.0, m));
}  /* Poisson_cdf */

long RNG::Poisson_idf(double m, double u)
{ long x;
  x = (long)m;                            /* start searching at the mean */

#ifdef OLDWAY
  if (Poisson_cdf(m, x) <= u)
    while (Poisson_cdf(m, x) <= u)  {
      x++;
    }
  else if (Poisson_cdf(m, 0) <= u)
    while (Poisson_cdf(m, x - 1) > u)  {
      x--;
    }
  else
    x = 0;
  return(x);
#else
  int limit = (int)(2*m);
  BinarySearch BS(0,limit);  // use this to control search
  x = BS.search();       // starting point
  while(! BS.found() ) {
        if( Poisson_cdf(m, x) <= u ) x = BS.refine(0);
        else x = BS.refine(1);
  }
  x = BS.search();
  
  // if x == 2*m then we need to correct for having excluded upper end
  if (x < limit) return (x);
  while (Poisson_cdf(m, x) <= u)  {
      x++;
  }
  return x;

#endif


}  /* Poisson_idf */

/* ========================================================================= */
/*                          double Uniform_pdf()                             */
/*                          double Uniform_cdf()                             */
/*                          double Uniform_idf()                             */
/*                                                                           */
/* NOTE: use a < x < b and 0 < u < 1                                         */
/* ========================================================================= */
double RNG::Uniform_pdf(double a, double b, double x)
{
  return(1.0 / (b - a));
}  /* Uniform_pdf */

double RNG::Uniform_cdf(double a, double b, double x)
{
  return((x - a) / (b - a));
}  /* Uniform_cdf */

double RNG::Uniform_idf(double a, double b, double u)
{
  return(a + (b - a) * u);
}  /* Uniform_idf */

/* ========================================================================= */
/*                        double Exponential_pdf()                           */
/*                        double Exponential_cdf()                           */
/*                        double Exponential_idf()                           */
/*                                                                           */
/* NOTE: use m > 0, x > 0 and 0 < u < 1                                      */
/* ========================================================================= */
double RNG::Exponential_pdf(double m, double x)
{
  return((1.0 / m) * exp(- x / m));
}  /* Exponential_pdf */

double RNG::Exponential_cdf(double m, double x)
{
  return(1.0 - exp(- x / m));
}  /* Exponential_cdf */

double RNG::Exponential_idf(double m, double u)
{
  return(- m * log(1 - u));
}  /* Exponential_idf */

/* ========================================================================= */
/*                          double Erlang_pdf()                              */
/*                          double Erlang_cdf()                              */
/*                          double Erlang_idf()                              */
/*                                                                           */
/* NOTE: use n >= 1, b > 0, x > 0 and 0 < u < 1                              */
/* ========================================================================= */
double RNG::Erlang_pdf(long n, double b, double x)
{ double t;

  t = (double)(n - 1) * log(x / b) - (x / b) - log(b) - Ln_Gamma((double)n);
  return(exp(t));
}  /* Erlang_pdf */

double RNG::Erlang_cdf(long n, double b, double x)
{ 
  return(Incomplete_Gamma((double)n, x / b));
}  /* Erlang_cdf */

double RNG::Erlang_idf(long n, double b, double u)
{ double tiny = 1.0e-10, t, x;
  x = (double)n * b;                 /* initialize to the mean, then - */
  do {                               /* use Newton-Raphson iteration   */
    t = x;
    x = t + (u - Erlang_cdf(n, b, t)) / Erlang_pdf(n, b, t);
    if (x <= 0.0)
      x = 0.5 * t;
  } while (fabs(x - t) >= tiny);
  return(x);
}  /* Erlang_idf */

/* ========================================================================= */
/*                          double Normal_pdf()                              */
/*                          double Normal_cdf()                              */
/*                          double Normal_idf()                              */
/*                                                                           */
/* NOTE: x can be any value, but 0 < u < 1                                   */
/* ========================================================================= */
double RNG::Normal_pdf (double x)
{ double c = 0.3989422804;                          /* 1 / Sqrt(2 * pi) */

  return(c * exp(-0.5 * x * x));
}  /* Normal_pdf */

double RNG::Normal_cdf (double x)
{ double t;

  t = Incomplete_Gamma(0.5, 0.5 * x * x);
  if (x < 0.0)
    return(0.5 * (1.0 - t));
  else
    return(0.5 * (1.0 + t));
}  /* Normal_cdf */

double RNG::Normal_idf (double u)
{ double tiny = 1.0e-10, t, x;

  x = 0.0;                           /* initialize to the mean, then - */
  do {                               /* use Newton-Raphson iteration   */
    t = x;
    x = t + (u - Normal_cdf(t)) / Normal_pdf(t);
  } while (fabs(x - t) >= tiny);
  return(x);
}  /* Normal_idf */

/* ========================================================================= */
/*                           double Gauss_pdf()                              */
/*                           double Gauss_cdf()                              */
/*                           double Gauss_idf()                              */
/*                                                                           */
/* NOTE: x and m can be any value, but s > 0 and 0 < u < 1                   */
/* ========================================================================= */
double RNG::Gauss_pdf(double m, double s, double x)
{ double t;

  t = (x - m) / s;
  return(Normal_pdf(t) / s);
}  /* Gauss_pdf */

double RNG::Gauss_cdf(double m, double s, double x)
{ double t;

  t = (x - m) / s;
  return(Normal_cdf(t));
}  /* Gauss_cdf */

double RNG::Gauss_idf(double m, double s, double u)
{ 
  return(m + s * Normal_idf(u));
}  /* Gauss_idf */

/* ========================================================================= */
/*                          double Lognormal_pdf()                           */
/*                          double Lognormal_cdf()                           */
/*                          double Lognormal_idf()                           */
/*                                                                           */
/* NOTE: a can have any value, but b > 0, x > 0 and 0 < u < 1                */
/* ========================================================================= */
double RNG::Lognormal_pdf(double a, double b, double x)
{ double t;

  t = (log(x) - a) / b;
  return(Normal_pdf(t) / (b * x));
}  /* Lognormal_pdf */

double RNG::Lognormal_cdf(double a, double b, double x)
{ double t;

  t = (log(x) - a) / b;
  return(Normal_cdf(t));
}  /* Lognormal_cdf */

double RNG::Lognormal_idf(double a, double b, double u)
{ double t;

  t = a + b * Normal_idf(u);
  return(exp(t));
}  /* Lognormal_idf */

/* ========================================================================= */
/*                          double Chisquare_pdf()                           */
/*                          double Chisquare_cdf()                           */
/*                          double Chisquare_idf()                           */
/*                                                                           */
/* NOTE: use n >= 1, x > 0 and 0 < u < 1                                     */
/* ========================================================================= */
double RNG::Chisquare_pdf(long n, double x)
{ double t;

  t = (n / 2 - 1) * log(x / 2) - (x / 2) - log(2) - Ln_Gamma( (double)(n / 2));
  return(exp(t));
}  /* Chisquare_pdf */

double RNG::Chisquare_cdf(long n, double x)
{ 
  return(Incomplete_Gamma(n / 2, x / 2));
}  /* Chisquare_cdf */

double RNG::Chisquare_idf(long n, double u)
{ double tiny = 1.0e-10, t, x;
  x = n;                               /* initialize to the mean, then - */
  do {                                 /* use Newton-Raphson iteration   */
    t = x;
    x = t + (u - Chisquare_cdf(n, t)) / Chisquare_pdf(n, t);
    if (x <= 0)
      x = 0.5 * t;
  } while (fabs(x - t) >= tiny);
  return(x);
}  /* Chisquare_idf */

/* ========================================================================= */
/*                           double Student_pdf()                            */
/*                           double Student_cdf()                            */
/*                           double Student_idf()                            */
/*                                                                           */
/* NOTE: use n >= 1, x > 0 and 0 < u < 1                                     */
/* ========================================================================= */
double RNG::Student_pdf(long n, double x)
{ double t;

  t = -0.5 * (n + 1) * log(1 + ((x * x) / (double)n))
                                     - Ln_Beta(0.5, (double)n / 2.0);
  return(exp(t) / sqrt((double)n));
}  /* Student_pdf */

double RNG::Student_cdf(long n, double x)
{ double s, t;

  t = (x * x) / (n + x * x);
  s = Incomplete_Beta(0.5, (double)n / 2.0, t);
  if (x >= 0)
    return(0.5 * (1.0 + s));
  else
    return(0.5 * (1.0 - s));
}  /* Student_cdf */

double RNG::Student_idf(long n, double u)
{ double tiny = 1.0e-10, t, x;

  x = 0;                              /* initialize to the mean, then - */
  do {                                /* use Newton-Raphson iteration   */
    t = x;
    x = t + (u - Student_cdf(n, t)) / Student_pdf(n, t);
  } while (ABS(x - t) >= tiny);
  return(x);
}  /* Student_idf */
