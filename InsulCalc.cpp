#include "stdafx.h"
#include "InsulCalc.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace InsulCalc
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;

    struct QGrid
    {
        std::vector<double> row; // DN or Douter
        std::vector<double> tC;
        std::vector<double> v;   // row-major
        size_t nT() const { return tC.size(); }
    };

    double Interp1(const std::vector<double>& x, const std::vector<double>& y, double xi)
    {
        if (x.empty() || y.empty() || x.size() != y.size())
            return 0.0;
        if (xi <= x.front()) return y.front();
        if (xi >= x.back()) return y.back();
        for (size_t i = 0; i + 1 < x.size(); ++i)
        {
            if (xi >= x[i] && xi <= x[i + 1])
            {
                const double u = (xi - x[i]) / (x[i + 1] - x[i]);
                return y[i] * (1.0 - u) + y[i + 1] * u;
            }
        }
        return y.back();
    }

    double Interp2(const QGrid& g, double rowKey, double tC)
    {
        if (g.row.empty() || g.tC.empty() || g.v.size() != g.row.size() * g.tC.size())
            return 0.0;

        auto clampIdx = [](const std::vector<double>& a, double x, size_t& i0, size_t& i1, double& u)
        {
            if (x <= a.front()) { i0 = i1 = 0; u = 0; return; }
            if (x >= a.back()) { i0 = i1 = a.size() - 1; u = 0; return; }
            for (size_t i = 0; i + 1 < a.size(); ++i)
            {
                if (x >= a[i] && x <= a[i + 1])
                {
                    i0 = i; i1 = i + 1;
                    u = (x - a[i]) / (a[i + 1] - a[i]);
                    return;
                }
            }
            i0 = i1 = a.size() - 1; u = 0;
        };

        size_t r0, r1, c0, c1;
        double ur, uc;
        clampIdx(g.row, rowKey, r0, r1, ur);
        clampIdx(g.tC, tC, c0, c1, uc);
        const size_t nT = g.nT();
        const double v00 = g.v[r0 * nT + c0];
        const double v01 = g.v[r0 * nT + c1];
        const double v10 = g.v[r1 * nT + c0];
        const double v11 = g.v[r1 * nT + c1];
        const double v0 = v00 * (1.0 - uc) + v01 * uc;
        const double v1 = v10 * (1.0 - uc) + v11 * uc;
        return v0 * (1.0 - ur) + v1 * ur;
    }

    // SP 61.13330.2012 Table 4 — indoor, >5000 h/year, q W/m by DN x t
    QGrid MakeSp61Tab4()
    {
        QGrid g;
        g.row = { 15,20,25,40,50,65,80,100,125,150,200,250,300,350,400,450,500,600,700,800,900,1000 };
        g.tC  = { 50,100,150,200,250,300,350,400,450,500,550,600 };
        const double rows[][12] = {
            /*15*/  { 6,14,23,33,43,54,66,79,93,107,122,138 },
            /*20*/  { 7,16,26,37,48,60,73,87,102,117,134,151 },
            /*25*/  { 8,18,28,40,52,65,79,94,110,126,144,162 },
            /*40*/  { 9,21,32,45,59,73,89,105,122,141,160,180 },
            /*50*/  { 10,23,36,50,64,80,96,114,133,152,173,194 },
            /*65*/  { 12,26,41,56,72,89,107,127,147,169,191,214 },
            /*80*/  { 13,28,44,60,77,95,114,135,156,179,202,227 },
            /*100*/ { 14,31,48,65,84,103,124,146,169,193,218,244 },
            /*125*/ { 16,35,53,72,92,113,136,159,184,210,237,265 },
            /*150*/ { 18,38,58,79,100,123,147,172,199,226,255,285 },
            /*200*/ { 22,46,70,93,118,144,172,200,230,262,294,328 },
            /*250*/ { 26,53,79,106,134,162,193,224,257,291,327,364 },
            /*300*/ { 29,60,88,118,148,179,212,246,281,318,357,396 },
            /*350*/ { 33,66,97,129,161,195,230,267,305,344,385,428 },
            /*400*/ { 36,72,106,139,174,210,247,286,326,368,411,456 },
            /*450*/ { 39,78,114,150,187,225,264,305,348,392,437,484 },
            /*500*/ { 43,84,123,161,200,241,282,326,370,417,465,514 },
            /*600*/ { 49,96,139,181,225,269,315,363,412,462,515,569 },
            /*700*/ { 55,107,153,200,247,295,344,395,448,502,558,616 },
            /*800*/ { 61,118,169,220,270,322,376,431,487,546,606,668 },
            /*900*/ { 67,130,185,239,294,350,407,466,527,589,653,718 },
            /*1000*/{ 74,141,201,259,318,377,438,501,565,631,699,768 },
        };
        g.v.clear();
        for (size_t r = 0; r < g.row.size(); ++r)
            for (size_t c = 0; c < g.tC.size(); ++c)
                g.v.push_back(rows[r][c]);
        return g;
    }

    // SP 61 Table 5 — indoor, <=5000 h
    QGrid MakeSp61Tab5()
    {
        QGrid g;
        g.row = { 15,20,25,40,50,65,80,100,125,150,200,250,300,350,400,450,500,600,700,800,900,1000 };
        g.tC  = { 50,100,150,200,250,300,350,400,450,500,550,600 };
        const double rows[][12] = {
            { 6,16,25,35,46,58,71,85,99,114,130,147 },
            { 7,18,28,40,52,65,79,93,109,126,143,161 },
            { 8,20,31,43,56,70,85,101,118,136,154,174 },
            { 10,23,36,49,64,80,96,114,132,152,172,194 },
            { 11,25,40,54,70,87,105,124,144,165,187,210 },
            { 13,29,45,62,79,98,118,139,161,184,208,233 },
            { 14,32,49,66,85,105,126,148,171,195,221,247 },
            { 16,35,54,73,93,115,137,161,186,212,239,267 },
            { 18,39,60,81,103,126,151,176,203,231,261,291 },
            { 21,44,66,89,113,138,164,192,221,251,282,315 },
            { 26,53,80,107,134,163,194,225,258,292,328,365 },
            { 30,62,92,122,153,185,218,253,290,327,366,407 },
            { 34,70,103,136,170,205,241,279,319,359,402,446 },
            { 38,77,113,149,186,224,263,304,347,391,436,483 },
            { 42,85,123,162,201,242,284,328,373,419,467,517 },
            { 46,92,134,175,217,260,305,351,398,448,498,551 },
            { 51,100,144,189,233,279,327,375,426,478,532,587 },
            { 58,114,164,214,263,314,367,420,476,533,592,652 },
            { 65,127,182,236,290,345,402,460,520,582,645,710 },
            { 73,141,202,261,320,379,441,504,568,635,703,772 },
            { 81,156,221,285,349,413,479,547,616,687,760,834 },
            { 89,170,241,309,378,447,518,590,663,739,816,896 },
        };
        g.v.clear();
        for (size_t r = 0; r < g.row.size(); ++r)
            for (size_t c = 0; c < g.tC.size(); ++c)
                g.v.push_back(rows[r][c]);
        return g;
    }

    // NR 34-70-118-87 Table 1 — economic q (numerator), by OUTER diameter mm
    // Columns: 50,75,100,125,150,200,250,300,350,400,450,500,550,600
    QGrid MakeNr34Tab1()
    {
        QGrid g;
        g.row = { 10,20,32,48,57,76,89,108,133,159,194,219,273,325,377,426,478,530,630,720,820,920,1020 };
        g.tC  = { 50,75,100,125,150,200,250,300,350,400,450,500,550,600 };
        // numerator only from NR34 table 1 (+ continuation)
        const double rows[][14] = {
            /*10*/   { 7,12,16,22,27,37,48,59,72,84,98,111,125,140 },
            /*20*/   { 8,15,21,27,33,45,58,72,87,102,118,133,150,168 },
            /*32*/   { 11,18,26,33,40,53,68,84,101,118,135,154,173,193 },
            /*48*/   { 13,22,30,38,46,62,79,97,117,135,155,176,198,220 },
            /*57*/   { 14,23,33,41,49,66,84,102,124,144,165,187,209,233 },
            /*76*/   { 16,27,37,47,56,75,94,115,138,162,184,204,234,260 },
            /*89*/   { 17,29,40,50,60,80,101,123,149,172,197,221,247,274 },
            /*108*/  { 21,33,44,56,66,88,111,134,162,187,213,240,268,297 },
            /*133*/  { 24,37,50,63,74,99,122,150,179,206,234,263,293,323 },
            /*159*/  { 27,42,56,69,83,108,135,165,195,225,254,287,318,355 },
            /*194*/  { 32,49,64,78,93,121,150,185,216,249,284,315,349,385 },
            /*219*/  { 36,53,70,85,100,130,161,197,231,264,299,334,371,407 },
            /*273*/  { 42,59,79,97,113,145,179,216,252,289,326,363,401,440 },
            /*325*/  { 50,72,91,108,128,163,199,235,273,316,359,400,441,484 },
            /*377*/  { 57,77,100,122,141,181,220,259,298,342,393,436,480,525 },
            /*426*/  { 65,85,104,125,148,192,238,285,328,378,425,469,517,566 },
            /*478*/  { 70,97,121,150,170,218,265,312,361,408,465,505,553,607 },
            /*530*/  { 82,107,133,160,184,234,283,336,387,441,489,542,592,646 },
            /*630*/  { 94,120,148,177,204,260,318,374,430,501,558,615,671,727 },
            /*720*/  { 97,132,166,197,228,292,356,424,490,548,614,675,736,795 },
            /*820*/  { 100,137,175,213,250,326,402,475,550,618,687,754,820,885 },
            /*920*/  { 104,145,190,234,276,360,445,528,610,690,772,852,932,1010 },
            /*1020*/ { 140,183,225,270,314,404,488,573,659,745,830,914,1000,1080 },
        };
        g.v.clear();
        for (size_t r = 0; r < g.row.size(); ++r)
            for (size_t c = 0; c < g.tC.size(); ++c)
                g.v.push_back(rows[r][c]);
        return g;
    }

    double QNorm(const Input& in)
    {
        double q = 0.0;
        if (in.norm == Norm::Nr34)
        {
            static const QGrid g = MakeNr34Tab1();
            q = Interp2(g, in.dOuterMm, in.tMediaC);
        }
        else
        {
            if (in.hoursOver5000)
            {
                static const QGrid g4 = MakeSp61Tab4();
                q = Interp2(g4, in.dnMm, in.tMediaC);
            }
            else
            {
                static const QGrid g5 = MakeSp61Tab5();
                q = Interp2(g5, in.dnMm, in.tMediaC);
            }
            if (in.loc == Location::Outdoor)
                q *= 1.35; // approx vs tab.2/3; indoor tables are primary for boilers
        }
        if (in.regionK > 0.0)
            q *= in.regionK;
        return q;
    }

    // Mineral wool mats in construction (SP61 app.): lambda grows with mean t
    double LambdaAt(double tMediaC, double tAirC, double lambda0)
    {
        // SP61: tm = (tv + 40)/2 indoor
        const double tm = 0.5 * (tMediaC + 40.0);
        // Typical mats: lambda25~0.044..0.049, b~0.0002
        const double base = (lambda0 > 0.0) ? lambda0 : 0.049;
        return base + 0.00021 * (tm - 25.0);
    }

    double QLinear(double dOuterM, double deltaM, double lambda, double alpha,
        double tMedia, double tAir, double kLoss)
    {
        if (deltaM < 1e-6) deltaM = 1e-6;
        const double r0 = dOuterM * 0.5;
        const double r1 = r0 + deltaM;
        const double Rins = std::log(r1 / r0) / (2.0 * kPi * lambda);
        const double Rout = 1.0 / (2.0 * kPi * r1 * alpha);
        return kLoss * (tMedia - tAir) / (Rins + Rout);
    }

    double TSurface(double dOuterM, double deltaM, double lambda, double alpha,
        double tMedia, double tAir, double kLoss)
    {
        const double q = QLinear(dOuterM, deltaM, lambda, alpha, tMedia, tAir, kLoss);
        const double r1 = dOuterM * 0.5 + deltaM;
        const double Rout = 1.0 / (2.0 * kPi * r1 * alpha);
        return tAir + (q / kLoss) * Rout;
    }

    // Closed form from SP61 / C-O-K: B from heat balance, delta = 0.5*d*(B-1)
    // Iterate lambda and outer alpha term together.
    double SolveDeltaForQ(double dOuterM, double qTarget, double lambda0, double alpha,
        double tMedia, double tAir, double kLoss)
    {
        if (qTarget <= 1e-6 || (tMedia - tAir) <= 1e-6) return 0.0;

        double delta = 0.05; // start 50 mm
        for (int it = 0; it < 24; ++it)
        {
            const double lambda = LambdaAt(tMedia, tAir, lambda0);
            // Required total R: q = K*(dt)/Rtot => Rtot = K*dt/q
            const double Rtot = kLoss * (tMedia - tAir) / qTarget;
            // Rtot = ln(B)/(2*pi*lambda) + 1/(pi*alpha*d*B), B = 1 + 2*delta/d
            // Solve B by bisection
            double lo = 1.01, hi = 1.0 + 2.0 * 0.40 / dOuterM; // up to 400 mm
            for (int j = 0; j < 48; ++j)
            {
                const double B = 0.5 * (lo + hi);
                const double R = std::log(B) / (2.0 * kPi * lambda)
                    + 1.0 / (kPi * alpha * dOuterM * B);
                if (R < Rtot) lo = B; // too thin resistance
                else hi = B;
            }
            const double B = 0.5 * (lo + hi);
            const double deltaNew = 0.5 * dOuterM * (B - 1.0);
            if (std::fabs(deltaNew - delta) < 1e-5) { delta = deltaNew; break; }
            delta = deltaNew;
        }
        // verify / refine with direct QLinear bisection
        double lo = 0.005, hi = 0.40;
        const double lambda = LambdaAt(tMedia, tAir, lambda0);
        for (int i = 0; i < 48; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            const double q = QLinear(dOuterM, mid, lambda, alpha, tMedia, tAir, kLoss);
            if (q > qTarget) lo = mid;
            else hi = mid;
        }
        return 0.5 * (lo + hi);
    }

    double SolveDeltaForTs(double dOuterM, double tsMax, double lambda0, double alpha,
        double tMedia, double tAir, double kLoss)
    {
        double lo = 0.005, hi = 0.40;
        const double lambda = LambdaAt(tMedia, tAir, lambda0);
        for (int i = 0; i < 48; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            const double ts = TSurface(dOuterM, mid, lambda, alpha, tMedia, tAir, kLoss);
            if (ts > tsMax) lo = mid;
            else hi = mid;
        }
        return 0.5 * (lo + hi);
    }

    double SurfaceTempLimit(const Input& in)
    {
        // NR34 p.1.8 / SP61: indoor
        if (in.loc == Location::Outdoor)
            return 55.0;
        if (in.norm == Norm::Nr34)
            return (in.tMediaC > 500.0) ? 48.0 : 45.0;
        if (in.tMediaC > 100.0) return 45.0;
        return 40.0;
    }

    // NR34 p.2.31 — max thickness indoor, by DN
    double Nr34LimitMm(double dnMm)
    {
        struct Row { double dn; double lim; };
        static const Row rows[] = {
            { 10, 40 }, { 25, 60 }, { 40, 80 }, { 50, 100 },
            { 100, 160 }, { 150, 180 }, { 200, 200 },
            { 250, 220 }, { 350, 220 }, { 400, 240 }, { 500, 250 }
        };
        std::vector<double> x, y;
        for (const auto& r : rows) { x.push_back(r.dn); y.push_back(r.lim); }
        return Interp1(x, y, dnMm);
    }

    double DefaultAlpha(const Input& in)
    {
        // SP61 indoor ~7..12; NR34 indoor practice ~10; outdoor ~26
        if (in.loc == Location::Outdoor) return 26.0;
        return 10.0;
    }

    double DefaultTAir(const Input& in)
    {
        if (in.loc == Location::Outdoor) return 5.0;
        return (in.norm == Norm::Nr34) ? 25.0 : 20.0;
    }

    double DefaultKLoss(const Input& in)
    {
        // NTP "Izolyaciya" / SP61: steel pipe on movable supports
        // DN < 150 -> 1.20; DN >= 150 -> 1.15; outdoor often 1.25..1.3
        if (in.loc == Location::Outdoor)
            return 1.25;
        return (in.dnMm < 150.0) ? 1.20 : 1.15;
    }
}

const wchar_t* NormName(Norm n)
{
    switch (n)
    {
    case Norm::Sp61:      return L"SP 61.13330.2012";
    case Norm::Snip4103:  return L"SNiP 41-03-2003";
    case Norm::Snip20414: return L"SNiP 2.04.14-88";
    case Norm::Nr34:      return L"NR 34-70-118-87";
    default:              return L"?";
    }
}

double RoundUpDelta(double deltaMm, double stepMm, double minMm)
{
    if (deltaMm < minMm) deltaMm = minMm;
    if (stepMm <= 0.0) return deltaMm;
    return std::ceil(deltaMm / stepMm - 1e-9) * stepMm;
}

Result Calculate(const Input& in0)
{
    Result r;
    Input in = in0;
    if (in.alpha <= 0.0) in.alpha = DefaultAlpha(in);
    if (in.tAirC < -80.0 || in.tAirC > 80.0) in.tAirC = DefaultTAir(in);
    if (in.norm == Norm::Nr34 && in.loc == Location::Indoor)
        in.tAirC = 25.0; // NR34 p.2.29
    if (in.kLoss <= 0.0) in.kLoss = DefaultKLoss(in);
    if (in.lambda <= 0.0) in.lambda = 0.049;
    if (in.dOuterMm <= 1.0) in.dOuterMm = in.dnMm > 0 ? in.dnMm * 1.08 : 108.0;
    if (in.dnMm <= 1.0) in.dnMm = in.dOuterMm;

    r.normName = NormName(in.norm);
    r.tSurfaceMaxC = SurfaceTempLimit(in);

    const double dM = in.dOuterMm * 0.001;
    r.qNormWm = QNorm(in);

    r.deltaQMm = SolveDeltaForQ(dM, r.qNormWm, in.lambda, in.alpha, in.tMediaC, in.tAirC, in.kLoss) * 1000.0;
    r.deltaTsMm = SolveDeltaForTs(dM, r.tSurfaceMaxC, in.lambda, in.alpha, in.tMediaC, in.tAirC, in.kLoss) * 1000.0;

    // Like NTP Izolyaciya auto-criteria: max(q_norm, burn/surface)
    double raw = (std::max)(r.deltaQMm, r.deltaTsMm);
    r.criterion = (r.deltaTsMm > r.deltaQMm) ? L"ot_ozhoga" : L"q_norm";

    if (in.norm == Norm::Nr34)
    {
        r.deltaLimitMm = Nr34LimitMm(in.dnMm);
        if (raw > r.deltaLimitMm + 1e-6)
        {
            r.limitedByNr34 = true;
            // p.2.31: as a rule do not exceed limit — assign capped value
            raw = r.deltaLimitMm;
            r.note = L"NR34 p.2.31: delta capped to limit table";
        }
    }

    // NR34 p.2.26: min 30 mm; round up to 10 mm (catalog).
    // SP61 p.6.12: accepted thickness may be up to 3 mm below calc — we still round UP.
    r.deltaMm = RoundUpDelta(raw, 10.0, 30.0);
    if (in.norm == Norm::Nr34 && r.deltaLimitMm > 0.0 && r.deltaMm > r.deltaLimitMm)
        r.deltaMm = r.deltaLimitMm;
    if (in.tMediaC > 300.0)
    {
        if (!r.note.empty()) r.note += L" | ";
        r.note += L"SP61 p.6.16: t>300C => min 2 layers (material pick)";
    }

    const double lambda = LambdaAt(in.tMediaC, in.tAirC, in.lambda);
    const double dltM = r.deltaMm * 0.001;
    r.qCalcWm = QLinear(dM, dltM, lambda, in.alpha, in.tMediaC, in.tAirC, in.kLoss);
    r.tSurfaceC = TSurface(dM, dltM, lambda, in.alpha, in.tMediaC, in.tAirC, in.kLoss);

    wchar_t buf[256];
    swprintf_s(buf,
        L"lambda=%.3f Wt/(m*K), alpha=%.1f, K=%.2f, t_air=%.1f C, Dnar=%.1f",
        lambda, in.alpha, in.kLoss, in.tAirC, in.dOuterMm);
    if (!r.note.empty()) r.note += L" | ";
    r.note += buf;

    r.ok = (r.deltaMm > 0.0) && (in.tMediaC > in.tAirC) && (r.qNormWm > 0.0);
    if (!r.ok)
        r.note += L" | error: t / diameter / q table";
    return r;
}
}
