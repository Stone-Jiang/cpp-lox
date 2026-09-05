#pragma once

#include <cmath>
#include <limits>
#include <stdexcept>


namespace statcpp
{
/**
 * @brief Pi constant
 */
constexpr double pi = 3.14159265358979323846;

/**
 * @brief Square root of 2
 */
constexpr double sqrt_2 = 1.41421356237309504880;

/**
 * @brief Square root of 2*pi
 */
constexpr double sqrt_2_pi = 2.50662827463100050242;

/**
 * @brief Natural logarithm of sqrt(2*pi)
 */
constexpr double log_sqrt_2_pi = 0.91893853320467274178;

double lgamma_impl(double x);

/**
 * @brief Log-gamma function
 *
 * Computes the natural logarithm of the gamma function.
 *
 * @param x Argument
 * @return log(Gamma(x))
 * @throws std::domain_error If x is a non-positive integer
 */
double lgamma(double x);

/**
 * @brief Gamma function
 *
 * Computes the gamma function Gamma(x).
 *
 * @param x Argument
 * @return Gamma(x)
 * @throws std::domain_error If x is a non-positive integer
 *
 * @note For small positive integers, computes factorial directly.
 */
double tgamma(double x);

// ============================================================================
// Beta Function / Incomplete Beta Function
// ============================================================================

/**
 * @brief Beta function
 *
 * Computes the beta function B(a, b) = Gamma(a) * Gamma(b) / Gamma(a + b).
 *
 * @param a First parameter (must be positive)
 * @param b Second parameter (must be positive)
 * @return B(a, b)
 * @throws std::domain_error If a or b is non-positive
 */
double beta(double a, double b);

/**
 * @brief Log-beta function
 *
 * Computes the natural logarithm of the beta function.
 *
 * @param a First parameter (must be positive)
 * @param b Second parameter (must be positive)
 * @return log(B(a, b))
 * @throws std::domain_error If a or b is non-positive
 */
double lbeta(double a, double b);

/**
 * @brief Internal regularized incomplete beta function
 *
 * Computes the regularized incomplete beta function I_x(a, b) using continued fraction expansion.
 *
 * @param a First parameter
 * @param b Second parameter
 * @param x Upper limit of integration (must be in [0, 1])
 * @param recursion_depth Recursion depth for tracking
 * @return I_x(a, b)
 * @throws std::runtime_error If recursion depth is exceeded
 *
 * @note Reference: Numerical Recipes, Press et al.
 * @note Uses Lentz's algorithm
 */
double betainc_impl(double a, double b, double x, int recursion_depth);

/**
 * @brief Regularized incomplete beta function
 *
 * Computes the regularized incomplete beta function I_x(a, b).
 *
 * @param a First parameter (must be positive)
 * @param b Second parameter (must be positive)
 * @param x Upper limit of integration (must be in [0, 1])
 * @return I_x(a, b)
 * @throws std::domain_error If parameters are invalid
 *
 * @note Used for computing CDFs of beta, F, and t distributions.
 */
double betainc(double a, double b, double x);

/**
 * @brief Inverse regularized incomplete beta function
 *
 * Computes x such that I_x(a, b) = p (quantile function).
 *
 * @param a First parameter (must be positive)
 * @param b Second parameter (must be positive)
 * @param p Probability value (must be in [0, 1])
 * @return x such that I_x(a, b) = p
 * @throws std::domain_error If parameters are invalid
 *
 * @note Uses Newton-Raphson iteration
 */
double betaincinv(double a, double b, double p);

// ============================================================================
// Error Function (erf / erfc)
// ============================================================================

/**
 * @brief Error function
 *
 * Computes the error function erf(x).
 *
 * @param x Argument
 * @return erf(x)
 *
 * @note Reference: Abramowitz and Stegun, 7.1.26
 * @note Uses Horner's method approximation
 */
double erf(double x);

/**
 * @brief Complementary error function
 *
 * Computes the complementary error function erfc(x) = 1 - erf(x).
 *
 * @param x Argument
 * @return erfc(x)
 */
double erfc(double x);

// ============================================================================
// Normal Distribution CDF and Quantile (Phi and Phi^{-1})
// ============================================================================

/**
 * @brief Standard normal CDF
 *
 * Computes the cumulative distribution function Phi(x) of the standard normal distribution.
 *
 * @param x Argument
 * @return Phi(x) = P(Z <= x) where Z ~ N(0, 1)
 */
double norm_cdf(double x);

/**
 * @brief Standard normal quantile function
 *
 * Computes the quantile function Phi^{-1}(p) of the standard normal distribution.
 *
 * @param p Probability (must be in (0, 1))
 * @return x such that Phi(x) = p
 *
 * @note Reference: https://home.online.no/~pjacklam/notes/invnorm/
 * @note Rational approximation by Acklam's algorithm
 */
double norm_quantile(double p);

// ============================================================================
// Lower Incomplete Gamma Function (for chi-square, gamma distributions)
// ============================================================================

/**
 * @brief Lower regularized incomplete gamma function
 *
 * Computes the lower regularized incomplete gamma function P(a, x) = gamma(a, x) / Gamma(a).
 *
 * @param a Shape parameter (must be positive)
 * @param x Upper limit of integration (must be non-negative)
 * @return P(a, x)
 * @throws std::domain_error If parameters are invalid
 *
 * @note Uses series expansion for x < a + 1, continued fraction for x >= a + 1
 */
double gammainc_lower(double a, double x);

/**
 * @brief Upper regularized incomplete gamma function
 *
 * Computes the upper regularized incomplete gamma function Q(a, x) = Gamma(a, x) / Gamma(a) = 1 - P(a, x).
 *
 * @param a Shape parameter (must be positive)
 * @param x Lower limit of integration (must be non-negative)
 * @return Q(a, x)
 * @throws std::domain_error If parameters are invalid
 */
double gammainc_upper(double a, double x);

/**
 * @brief Inverse lower regularized incomplete gamma function
 *
 * Computes x such that P(a, x) = p.
 *
 * @param a Shape parameter (must be positive)
 * @param p Probability value (must be in [0, 1])
 * @return x such that P(a, x) = p
 * @throws std::domain_error If parameters are invalid
 *
 * @note Uses Newton-Raphson iteration
 * @note Uses Wilson-Hilferty approximation for initial guess when a > 1
 */
double gammainc_lower_inv(double a, double p);

}