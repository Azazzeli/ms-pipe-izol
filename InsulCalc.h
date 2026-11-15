#pragma once

#include <string>

// Insulation thickness calculator (final δ, mm).
// Norms: SP 61.13330.2012 / SNiP 41-03-2003, SNiP 2.04.14-88 (MVP via SP61 tables),
//        NR 34-70-118-87 (TES/NPP: max(q, ts) + limit table).
// Material is NOT selected — reference λ is used only as a heat-transfer parameter.

namespace InsulCalc
{
    enum class Norm
    {
        Sp61 = 1,       // СП 61.13330.2012 (= актуализация СНиП 41-03-2003)
        Snip4103 = 2,   // СНиП 41-03-2003 (same engine / tables as SP61 for MVP)
        Snip20414 = 3,  // СНиП 2.04.14-88 (MVP: same cylinder solver, SP61 q-grid)
        Nr34 = 4        // НР 34-70-118-87 (ТЭС/АЭС)
    };

    enum class Location
    {
        Indoor = 1,
        Outdoor = 2
    };

    struct Input
    {
        Norm norm = Norm::Sp61;
        Location loc = Location::Indoor;
        double dnMm = 100.0;          // условный Ду
        double dOuterMm = 108.0;      // наружный диаметр трубы, мм
        double tMediaC = 150.0;       // температура теплоносителя, °C
        double tAirC = 20.0;          // расчётная t воздуха (помещение 20 / НР34: 25)
        bool hoursOver5000 = true;    // режим >5000 ч/год (СП 61 табл.)
        double lambda = 0.049;        // Вт/(м·К) — опорная λ (не «материал»)
        double alpha = 10.0;          // Вт/(м²·К) — помещение ~7–12, улица ~26
        double kLoss = 1.2;           // доп. потери опоры/арматура (помещ. 1.15–1.2, улица 1.25–1.3)
        double regionK = 1.0;         // СП 61 табл.13 — районный коэф. к норме q
    };

    struct Result
    {
        bool ok = false;
        double deltaMm = 0.0;         // итоговая толщина (округление вверх)
        double deltaQMm = 0.0;        // по нормированной плотности потока
        double deltaTsMm = 0.0;       // по t поверхности
        double qNormWm = 0.0;         // норма q, Вт/м
        double qCalcWm = 0.0;         // расчётный q при δ
        double tSurfaceC = 0.0;       // t на поверхности при δ
        double tSurfaceMaxC = 45.0;   // предел t поверхности
        double deltaLimitMm = 0.0;    // предел по НР 34 (0 = нет)
        bool limitedByNr34 = false;
        const wchar_t* normName = L"";
        const wchar_t* criterion = L"";
        std::wstring note;
    };

    const wchar_t* NormName(Norm n);
    Result Calculate(const Input& in);

    // Round up to manufacturing step (default 10 mm), min 30 mm (NR 34 / practice).
    double RoundUpDelta(double deltaMm, double stepMm = 10.0, double minMm = 30.0);
}
