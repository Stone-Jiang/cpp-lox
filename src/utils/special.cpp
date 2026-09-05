#include "special.hpp"

namespace statcpp
{

double lgamma_impl(double x)
{
    if (x <= 0.0 && x == std::floor(x)) {
        throw std::domain_error("statcpp::lgamma: non-positive integer argument");
    }

    // Lanczos coefficients for g=7
    static const double c[9] = {
        0.99999999999980993,
        676.5203681218851,
        -1259.1392167224028,
        771.32342877765313,
        -176.61502916214059,
        12.507343278686905,
        -0.13857109526572012,
        9.9843695780195716e-6,
        1.5056327351493116e-7
    };

    if (x < 0.5) {
        // Reflection formula: Gamma(x) * Gamma(1-x) = pi / sin(pi*x)
        return std::log(pi / std::sin(pi * x)) - lgamma_impl(1.0 - x);
    }

    x -= 1.0;
    double a = c[0];
    for (int i = 1; i < 9; ++i) {
        a += c[i] / (x + static_cast<double>(i));
    }

    double t = x + 7.5;
    return log_sqrt_2_pi + (x + 0.5) * std::log(t) - t + std::log(a);
}

double lgamma(double x)
{
    return lgamma_impl(x);
}

double tgamma(double x)
{
    if (x <= 0.0 && x == std::floor(x)) {
        throw std::domain_error("statcpp::tgamma: non-positive integer argument");
    }

    // For small positive integers, use factorial
    if (x > 0.0 && x <= 20.0 && x == std::floor(x)) {
        double result = 1.0;
        for (int i = 2; i < static_cast<int>(x); ++i) {
            result *= i;
        }
        return result;
    }

    return std::exp(lgamma_impl(x));
}

double beta(double a, double b)
{
    if (a <= 0.0 || b <= 0.0) {
        throw std::domain_error("statcpp::beta: parameters must be positive");
    }
    return std::exp(lgamma(a) + lgamma(b) - lgamma(a + b));
}

double lbeta(double a, double b)
{
    if (a <= 0.0 || b <= 0.0) {
        throw std::domain_error("statcpp::lbeta: parameters must be positive");
    }
    return lgamma(a) + lgamma(b) - lgamma(a + b);
}

double betainc_impl(double a, double b, double x, int recursion_depth)
{
    // Recursion depth check (prevent infinite recursion)
    // At most 1 recursive call is made via the symmetry relation I_x(a,b) = 1 - I_{1-x}(b,a).
    // After that single redirect the continued fraction always converges, so depth > 1 is unreachable.
    // We keep the guard at depth > 1 to be safe and ensure we never loop.
    if (recursion_depth > 1) {
        throw std::runtime_error("statcpp::betainc: maximum recursion depth exceeded");
    }

    // Use symmetry relation for better convergence
    // I_x(a, b) = 1 - I_{1-x}(b, a)
    if (x > (a + 1.0) / (a + b + 2.0)) {
        return 1.0 - betainc_impl(b, a, 1.0 - x, recursion_depth + 1);
    }

    // Continued fraction (Lentz's algorithm)
    const double eps = std::numeric_limits<double>::epsilon();
    const double tiny = std::numeric_limits<double>::min();
    const int max_iter = 200;

    double qab = a + b;
    double qap = a + 1.0;
    double qam = a - 1.0;

    double c = 1.0;
    double d = 1.0 - qab * x / qap;
    if (std::abs(d) < tiny) d = tiny;
    d = 1.0 / d;
    double h = d;

    for (int m = 1; m <= max_iter; ++m) {
        int m2 = 2 * m;

        // Even step
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (std::abs(d) < tiny) d = tiny;
        c = 1.0 + aa / c;
        if (std::abs(c) < tiny) c = tiny;
        d = 1.0 / d;
        h *= d * c;

        // Odd step
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (std::abs(d) < tiny) d = tiny;
        c = 1.0 + aa / c;
        if (std::abs(c) < tiny) c = tiny;
        d = 1.0 / d;
        double del = d * c;
        h *= del;

        if (std::abs(del - 1.0) < eps) {
            break;
        }
    }

    double front = std::exp(a * std::log(x) + b * std::log(1.0 - x) - lbeta(a, b)) / a;
    return front * h;
}

double betainc(double a, double b, double x)
{
    if (a <= 0.0 || b <= 0.0) {
        throw std::domain_error("statcpp::betainc: parameters must be positive");
    }
    if (x < 0.0 || x > 1.0) {
        throw std::domain_error("statcpp::betainc: x must be in [0, 1]");
    }
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;

    return betainc_impl(a, b, x, 0);
}

double betaincinv(double a, double b, double p)
{
    if (a <= 0.0 || b <= 0.0) {
        throw std::domain_error("statcpp::betaincinv: parameters must be positive");
    }
    if (p < 0.0 || p > 1.0) {
        throw std::domain_error("statcpp::betaincinv: p must be in [0, 1]");
    }
    if (p == 0.0) return 0.0;
    if (p == 1.0) return 1.0;

    const double eps = 1e-10;
    const int max_iter = 100;

    // Initial guess using approximation
    double x = a / (a + b);
    if (a < 1.0 || b < 1.0) {
        x = 0.5;
    }

    // Newton-Raphson with bisection fallback
    double lo = 0.0, hi = 1.0;

    for (int i = 0; i < max_iter; ++i) {
        double f = betainc(a, b, x) - p;

        if (std::abs(f) < eps) {
            return x;
        }

        // Update bisection bounds
        if (f < 0.0) {
            lo = x;
        } else {
            hi = x;
        }

        // Derivative: d/dx I_x(a,b) = x^(a-1) * (1-x)^(b-1) / B(a,b)
        double df = std::exp((a - 1.0) * std::log(x) + (b - 1.0) * std::log(1.0 - x) - lbeta(a, b));

        double dx = f / df;
        double x_new = x - dx;

        // Use bisection if Newton step goes out of bounds
        if (x_new <= lo || x_new >= hi) {
            x_new = (lo + hi) / 2.0;
        }

        if (std::abs(x_new - x) < eps * x) {
            return x_new;
        }

        x = x_new;
    }

    return x;
}

double erf(double x)
{
    return std::erf(x);
}

double erfc(double x)
{
    return std::erfc(x);
}

double norm_cdf(double x)
{
    return 0.5 * (1.0 + erf(x / sqrt_2));
}

double norm_quantile(double p)
{
    if (p <= 0.0) {
        return -std::numeric_limits<double>::infinity();
    }
    if (p >= 1.0) {
        return std::numeric_limits<double>::infinity();
    }

    // Coefficients for rational approximation
    static const double a[6] = {
        -3.969683028665376e+01,
         2.209460984245205e+02,
        -2.759285104469687e+02,
         1.383577518672690e+02,
        -3.066479806614716e+01,
         2.506628277459239e+00
    };
    static const double b[5] = {
        -5.447609879822406e+01,
         1.615858368580409e+02,
        -1.556989798598866e+02,
         6.680131188771972e+01,
        -1.328068155288572e+01
    };
    static const double c[6] = {
        -7.784894002430293e-03,
        -3.223964580411365e-01,
        -2.400758277161838e+00,
        -2.549732539343734e+00,
         4.374664141464968e+00,
         2.938163982698783e+00
    };
    static const double d[4] = {
         7.784695709041462e-03,
         3.224671290700398e-01,
         2.445134137142996e+00,
         3.754408661907416e+00
    };

    const double p_low  = 0.02425;
    const double p_high = 1.0 - p_low;

    double q, r;

    if (p < p_low) {
        // Rational approximation for lower region
        q = std::sqrt(-2.0 * std::log(p));
        return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
               ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    } else if (p <= p_high) {
        // Rational approximation for central region
        q = p - 0.5;
        r = q * q;
        return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q /
               (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1.0);
    } else {
        // Rational approximation for upper region
        q = std::sqrt(-2.0 * std::log(1.0 - p));
        return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
                ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    }
}

double gammainc_lower(double a, double x)
{
    if (a <= 0.0) {
        throw std::domain_error("statcpp::gammainc_lower: a must be positive");
    }
    if (x < 0.0) {
        throw std::domain_error("statcpp::gammainc_lower: x must be non-negative");
    }
    if (x == 0.0) return 0.0;

    const double eps = std::numeric_limits<double>::epsilon();
    const int max_iter = 200;

    // Use series expansion for x < a + 1
    if (x < a + 1.0) {
        double term = 1.0 / a;
        double sum = term;
        for (int n = 1; n <= max_iter; ++n) {
            term *= x / (a + n);
            sum += term;
            if (std::abs(term) < eps * std::abs(sum)) {
                break;
            }
        }
        return sum * std::exp(-x + a * std::log(x) - lgamma(a));
    }

    // Use continued fraction for x >= a + 1 (Lentz's algorithm)
    const double tiny = std::numeric_limits<double>::min();

    double b = x + 1.0 - a;
    double c = 1.0 / tiny;
    double d = 1.0 / b;
    double h = d;

    for (int i = 1; i <= max_iter; ++i) {
        double an = -i * (i - a);
        b += 2.0;
        d = an * d + b;
        if (std::abs(d) < tiny) d = tiny;
        c = b + an / c;
        if (std::abs(c) < tiny) c = tiny;
        d = 1.0 / d;
        double del = d * c;
        h *= del;
        if (std::abs(del - 1.0) < eps) {
            break;
        }
    }

    // Q(a,x) = 1 - P(a,x), where we computed Q via continued fraction
    double q = std::exp(-x + a * std::log(x) - lgamma(a)) * h;
    return 1.0 - q;
}

double gammainc_upper(double a, double x)
{
    return 1.0 - gammainc_lower(a, x);
}

double gammainc_lower_inv(double a, double p)
{
    if (a <= 0.0) {
        throw std::domain_error("statcpp::gammainc_lower_inv: a must be positive");
    }
    if (p < 0.0 || p > 1.0) {
        throw std::domain_error("statcpp::gammainc_lower_inv: p must be in [0, 1]");
    }
    if (p == 0.0) return 0.0;
    if (p == 1.0) return std::numeric_limits<double>::infinity();

    const double eps = 1e-10;
    const int max_iter = 100;

    // Initial guess
    double x;
    if (a > 1.0) {
        // Use Wilson-Hilferty approximation
        double t = norm_quantile(p);
        double v = t * std::sqrt(1.0 / (9.0 * a)) + 1.0 - 1.0 / (9.0 * a);
        x = a * v * v * v;
        if (x <= 0.0) x = 0.5;
    } else {
        x = std::pow(p * tgamma(a + 1.0), 1.0 / a);
    }

    // 二分法の探索範囲を初期化
    double lo = 0.0;
    double hi = a + 50.0 * std::sqrt(a);
    if (hi < 10.0) hi = 10.0;

    // hi が実際に上界であることを保証する
    while (gammainc_lower(a, hi) < p) {
        hi *= 2.0;
    }

    bool converged = false;

    // Newton-Raphson (二分法の範囲追跡付き)
    for (int i = 0; i < max_iter; ++i) {
        double f = gammainc_lower(a, x) - p;
        if (std::abs(f) < eps) {
            converged = true;
            break;
        }

        // 二分法の範囲を更新
        if (f < 0.0) {
            lo = x;
        } else {
            hi = x;
        }

        // Derivative: d/dx P(a,x) = x^(a-1) * e^(-x) / Gamma(a)
        double df = std::exp((a - 1.0) * std::log(x) - x - lgamma(a));
        if (df == 0.0) {
            // 導関数がゼロ: 二分法フォールバックへ
            break;
        }

        double x_new = x - f / df;

        // Newton ステップが範囲外なら二分法を使う
        if (x_new <= lo || x_new >= hi) {
            x_new = (lo + hi) / 2.0;
        }

        if (std::abs(x_new - x) < eps * x) {
            converged = true;
            x = x_new;
            break;
        }

        x = x_new;
    }

    // Newton-Raphson が収束しなかった場合の二分法フォールバック
    if (!converged) {
        for (int bisect_iter = 0; bisect_iter < 100; ++bisect_iter) {
            double mid = (lo + hi) / 2.0;
            double f_mid = gammainc_lower(a, mid) - p;
            if (std::abs(f_mid) < 1e-12 || (hi - lo) < 1e-12 * mid) {
                return mid;
            }
            if (f_mid < 0) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        return (lo + hi) / 2.0;
    }

    return x;
}

}
