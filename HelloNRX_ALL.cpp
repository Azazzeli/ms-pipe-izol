// =============================================================================
// HelloNRX_ALL.cpp ? ALL project C++ in one file (amalgamation)
// Contains: resource IDs, InsulCalc, AlignDlg, InsulDlg, NRX commands.
// Still needed separately: stdafx.h, HelloNRX.rc, .vcxproj / SDK.
// =============================================================================

#include "stdafx.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <map>

// ----- resource.h -----
#define IDD_ALIGN_PIPE                 101
#define IDD_INSUL                      102

#define IDC_STATIC_INFO                1001
#define IDC_LIST_COORDS                1002
#define IDC_LIST_ELBOWS                1003
#define IDC_EDIT_ANGLE                 1004
#define IDC_STATIC_ANGLE_CUR           1005
#define IDC_CHK_ORTHO                  1006
#define IDC_BTN_ALIGN_COORDS           1007
#define IDC_BTN_ALIGN_ALL_ELBOWS       1008
#define IDC_BTN_APPLY_ELBOW_ANGLE      1009
#define IDC_STATIC_ELBOW_HINT          1010
#define IDC_EDIT_X                     1011
#define IDC_EDIT_Y                     1012
#define IDC_EDIT_Z                     1013
#define IDC_BTN_APPLY_XYZ              1014
#define IDC_BTN_ROUND_XYZ              1015
#define IDC_STATIC_XYZ                 1016
#define IDC_CHK_PROTECT_TERM           1017

#define IDC_INSUL_TAB                  1100
#define IDC_INSUL_CMB_REGION           1101
#define IDC_INSUL_CMB_MEDIA            1102
#define IDC_INSUL_CMB_PLACE            1103
#define IDC_INSUL_CMB_NORM             1104
#define IDC_INSUL_EDIT_T               1105
#define IDC_INSUL_EDIT_REPORT          1106
#define IDC_INSUL_BTN_CALC             1107
#define IDC_INSUL_BTN_ASSIGN           1108
#define IDC_INSUL_STATIC_D             1109
#define IDC_INSUL_STATIC_RESULT        1110

#define IDC_INSUL_LIST_ZONES           1111
#define IDC_INSUL_BTN_REFRESH          1112
#define IDC_INSUL_BTN_CALC_ALL         1113
#define IDC_INSUL_BTN_ASSIGN_ALL       1114
#define IDC_INSUL_BTN_APPLY_ROW        1115
#define IDC_INSUL_CHK_SPLIT_WELD       1116
#define IDC_INSUL_CMB_REGION2          1117
#define IDC_INSUL_CMB_MEDIA2           1118
#define IDC_INSUL_EDIT_T2              1119
#define IDC_INSUL_STATIC_BATCH         1120
#define IDC_INSUL_BTN_SEL_ALL          1121
#define IDC_INSUL_BTN_SEL_NONE         1122

// ----- InsulCalc.h -----
#include <string>

// Insulation thickness calculator (final Оґ, mm).
// Norms: SP 61.13330.2012 / SNiP 41-03-2003, SNiP 2.04.14-88 (MVP via SP61 tables),
//        NR 34-70-118-87 (TES/NPP: max(q, ts) + limit table).
// Material is NOT selected вЂ” reference О» is used only as a heat-transfer parameter.

namespace InsulCalc
{
    enum class Norm
    {
        Sp61 = 1,       // РЎРџ 61.13330.2012 (= Р°РєС‚СѓР°Р»РёР·Р°С†РёСЏ РЎРќРёРџ 41-03-2003)
        Snip4103 = 2,   // РЎРќРёРџ 41-03-2003 (same engine / tables as SP61 for MVP)
        Snip20414 = 3,  // РЎРќРёРџ 2.04.14-88 (MVP: same cylinder solver, SP61 q-grid)
        Nr34 = 4        // РќР  34-70-118-87 (РўР­РЎ/РђР­РЎ)
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
        double dnMm = 100.0;          // СѓСЃР»РѕРІРЅС‹Р№ Р”Сѓ
        double dOuterMm = 108.0;      // РЅР°СЂСѓР¶РЅС‹Р№ РґРёР°РјРµС‚СЂ С‚СЂСѓР±С‹, РјРј
        double tMediaC = 150.0;       // С‚РµРјРїРµСЂР°С‚СѓСЂР° С‚РµРїР»РѕРЅРѕСЃРёС‚РµР»СЏ, В°C
        double tAirC = 20.0;          // СЂР°СЃС‡С‘С‚РЅР°СЏ t РІРѕР·РґСѓС…Р° (РїРѕРјРµС‰РµРЅРёРµ 20 / РќР 34: 25)
        bool hoursOver5000 = true;    // СЂРµР¶РёРј >5000 С‡/РіРѕРґ (РЎРџ 61 С‚Р°Р±Р».)
        double lambda = 0.049;        // Р’С‚/(РјВ·Рљ) вЂ” РѕРїРѕСЂРЅР°СЏ О» (РЅРµ В«РјР°С‚РµСЂРёР°Р»В»)
        double alpha = 10.0;          // Р’С‚/(РјВІВ·Рљ) вЂ” РїРѕРјРµС‰РµРЅРёРµ ~7вЂ“12, СѓР»РёС†Р° ~26
        double kLoss = 1.2;           // РґРѕРї. РїРѕС‚РµСЂРё РѕРїРѕСЂС‹/Р°СЂРјР°С‚СѓСЂР° (РїРѕРјРµС‰. 1.15вЂ“1.2, СѓР»РёС†Р° 1.25вЂ“1.3)
        double regionK = 1.0;         // РЎРџ 61 С‚Р°Р±Р».13 вЂ” СЂР°Р№РѕРЅРЅС‹Р№ РєРѕСЌС„. Рє РЅРѕСЂРјРµ q
    };

    struct Result
    {
        bool ok = false;
        double deltaMm = 0.0;         // РёС‚РѕРіРѕРІР°СЏ С‚РѕР»С‰РёРЅР° (РѕРєСЂСѓРіР»РµРЅРёРµ РІРІРµСЂС…)
        double deltaQMm = 0.0;        // РїРѕ РЅРѕСЂРјРёСЂРѕРІР°РЅРЅРѕР№ РїР»РѕС‚РЅРѕСЃС‚Рё РїРѕС‚РѕРєР°
        double deltaTsMm = 0.0;       // РїРѕ t РїРѕРІРµСЂС…РЅРѕСЃС‚Рё
        double qNormWm = 0.0;         // РЅРѕСЂРјР° q, Р’С‚/Рј
        double qCalcWm = 0.0;         // СЂР°СЃС‡С‘С‚РЅС‹Р№ q РїСЂРё Оґ
        double tSurfaceC = 0.0;       // t РЅР° РїРѕРІРµСЂС…РЅРѕСЃС‚Рё РїСЂРё Оґ
        double tSurfaceMaxC = 45.0;   // РїСЂРµРґРµР» t РїРѕРІРµСЂС…РЅРѕСЃС‚Рё
        double deltaLimitMm = 0.0;    // РїСЂРµРґРµР» РїРѕ РќР  34 (0 = РЅРµС‚)
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

// ----- AlignDlg.h -----
#include <vector>

// Crash breadcrumb logger (file + console). Safe to call from HelloNRX.cpp.
void PipeAlignCrashLog(const wchar_t* msg);

struct AlignNodeInfo
{
    int index = -1;
    AcGePoint3d pt;
    bool isElbow = false;
    bool isTerm = false;
    bool isLocked = false;
    bool isEnd = false;      // axis ends — always immutable
    double angleDeg = 0.0;   // TURN angle
    bool needsRound = false;
    AcDbObjectId oid;
};

class CAlignDlg : public CDialog
{
public:
    CAlignDlg(AcDbObjectId idAxis, CWnd* pParent = nullptr);

    enum { IDD = IDD_ALIGN_PIPE };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnSelChangeCoords();
    afx_msg void OnSelChangeElbows();
    afx_msg void OnAlignCoords();
    afx_msg void OnAlignAllElbows();
    afx_msg void OnApplyElbowAngle();
    afx_msg void OnApplyXyz();
    afx_msg void OnRoundXyz();
    afx_msg void OnCloseDlg();

    DECLARE_MESSAGE_MAP()

private:
    bool RefreshFromModel();
    void FillLists();
    void ShowNodeEdits(int idx);
    void HighlightElbow(int elbowListIndex);
    void ClearHighlight();
    bool ReadXyz(AcGePoint3d& pt) const;

    // Ends fixed. Any turnDeg. If does not fit — slide elbow along route (MS style).
    static bool MoveForTurnAngle(class vCS_DM_Axis* pAxis, int elbowNodeIdx, double turnDeg);

    // Snapshot / restore axis ends so ReCalculate cannot drag terminators.
    static void SnapshotEnds(class vCS_DM_Axis* pAxis, AcGePoint3d& pt0, AcGePoint3d& ptN, int& nNodes);
    static void RestoreEnds(class vCS_DM_Axis* pAxis, const AcGePoint3d& pt0, const AcGePoint3d& ptN);

    static double RadToDeg(double r);
    static double DegToRad(double d);
    static double SnapToRightAngle(double deg);
    static double TurnAngleDeg(const AcGePoint3d& A, const AcGePoint3d& B, const AcGePoint3d& C);
    static AcGePoint3d RoundPt(const AcGePoint3d& pt);
    static bool IsWholeMm(const AcGePoint3d& pt);
    static bool IsImmutableEnd(class vCS_DM_Node* p, int idx, int nNodes);

    AcDbObjectId m_idAxis;
    std::vector<AlignNodeInfo> m_nodes;
    std::vector<int> m_elbowIdx;
    AcDbObjectId m_highlighted;
    int m_selNode = -1;
    BOOL m_bWcsOrtho = FALSE;
    BOOL m_bProtectTerm = TRUE; // always treat ends as locked; checkbox keeps mid terms locked
};

// ----- InsulDlg.h -----
#include <vector>

struct InsulZone
{
    AcDbObjectId idAxis;
    CString axisName;
    CString zoneName;
    int zoneIndex = 0;
    int zoneCount = 1;
    AcDbObjectIdArray pipeIds;
    double dOuterMm = 108.0;
    double dnMm = 100.0;
    double tMediaC = 90.0;
    int mediaIdx = 0;
    bool selected = true;
    double deltaMm = 0.0;
    bool calculated = false;
    bool splitByWeld = false;
};

enum class InsulDlgMode
{
    SingleAxis = 0,
    AllAxes = 1
};

class CInsulDlg : public CDialog
{
public:
    explicit CInsulDlg(AcDbObjectId idAxis, InsulDlgMode mode, CWnd* pParent = nullptr);
    enum { IDD = IDD_INSUL };

    static CInsulDlg* GetInstance() { return s_pInstance; }
    static bool ShowModeless(AcDbObjectId idAxis, InsulDlgMode mode, CWnd* pParent);

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void PostNcDestroy() override;
    virtual void OnOK() override;
    virtual void OnCancel() override;

    afx_msg void OnBnCalculate();
    afx_msg void OnBnAssign();
    afx_msg void OnCbnChange();
    afx_msg void OnTabChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnRefreshZones();
    afx_msg void OnBnCalcAll();
    afx_msg void OnBnAssignAll();
    afx_msg void OnBnApplyRow();
    afx_msg void OnBnSelAll();
    afx_msg void OnBnSelNone();
    afx_msg void OnZoneItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()

private:
    bool ReadUi(InsulCalc::Input& in, CString& mediaName, CString& regionName) const;
    void ShowResult(const InsulCalc::Input& in, const InsulCalc::Result& r,
        const CString& mediaName, const CString& regionName);
    bool CollectAxisSegments(AcDbObjectIdArray& segIds) const;
    bool CollectAxisSegments(AcDbObjectId idAxis, AcDbObjectIdArray& pipeIds) const;
    bool AssignInsulation(double thicknessMm, double tMediaC, const CString& regionName, const CString& mediaName);
    bool AssignToPipes(AcDbObjectId idAxis, const AcDbObjectIdArray& pipeIds,
        double thicknessMm, double tMediaC, const CString& regionName, const CString& mediaName,
        bool writeAxisLevel);
    void WriteAxisParams(double thicknessMm, double tMediaC, const CString& regionName, const CString& mediaName);
    void WriteAxisParams(AcDbObjectId idAxis, double thicknessMm, double tMediaC,
        const CString& regionName, const CString& mediaName);

    void ShowTab(int tab);
    void FillRegionMediaCombos();
    void RebuildZones();
    void FillZoneList();
    void SyncZoneFromListChecks();
    void ReadDiameters(AcDbObjectId idAxis, double& dOuter, double& dn) const;
    CString AxisDisplayName(AcDbObjectId idAxis) const;
    void BuildZonesForAxis(AcDbObjectId idAxis, bool splitByWeld, std::vector<InsulZone>& out) const;

    void ClearHighlight();
    void HighlightAxis(AcDbObjectId idAxis);
    void HighlightPipes(const AcDbObjectIdArray& pipeIds);

    static CInsulDlg* s_pInstance;

    InsulDlgMode m_mode = InsulDlgMode::SingleAxis;
    AcDbObjectId m_idAxis;
    InsulCalc::Result m_last;
    bool m_hasResult = false;
    double m_dOuterMm = 108.0;
    double m_dnMm = 100.0;
    CString m_lastRegion;
    CString m_lastMedia;
    double m_lastT = 90.0;

    CTabCtrl m_tab;
    CListCtrl m_listZones;
    std::vector<InsulZone> m_zones;
    int m_curTab = 0;
    AcDbObjectId m_hlAxis;
};

// ----- InsulCalc.cpp -----
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

    // SP 61.13330.2012 Table 4 вЂ” indoor, >5000 h/year, q W/m by DN x t
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

    // SP 61 Table 5 вЂ” indoor, <=5000 h
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

    // NR 34-70-118-87 Table 1 вЂ” economic q (numerator), by OUTER diameter mm
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

    // NR34 p.2.31 вЂ” max thickness indoor, by DN
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
            // p.2.31: as a rule do not exceed limit вЂ” assign capped value
            raw = r.deltaLimitMm;
            r.note = L"NR34 p.2.31: delta capped to limit table";
        }
    }

    // NR34 p.2.26: min 30 mm; round up to 10 mm (catalog).
    // SP61 p.6.12: accepted thickness may be up to 3 mm below calc вЂ” we still round UP.
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

// ----- AlignDlg.cpp -----
#include "aced.h"

#include "vCSDragManager.h"
#include "vCSDragManagerSmart.h"
#include "vCS_DM_Axis.h"
#include "vCS_DM_Node.h"
#include "vCS_DM_NodeElbow.h"

#include <cmath>
#include <cstdio>
#include <cstdarg>

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kMinSegLen = 50.0; // mm — give room near terminators

    // Crash breadcrumb log: console + file (flushed every line).
    // After crash open: %USERPROFILE%\Documents\ModelStudioPipeTools\PIPE_ALIGN_crash.log
    void CrashLogWriteRaw(const wchar_t* line)
    {
        // Several paths — one of them must work even if Documents is redirected.
        static const wchar_t* kPaths[] = {
            L"C:\\Users\\poppa\\Documents\\ModelStudioPipeTools\\PIPE_ALIGN_crash.log",
            L"D:\\WORK\\SAP\\HelloNRX\\PIPE_ALIGN_crash.log",
            L"C:\\Temp\\PIPE_ALIGN_crash.log",
            nullptr
        };
        for (int i = 0; kPaths[i]; ++i)
        {
            // ensure parent dir
            wchar_t dir[MAX_PATH];
            wcsncpy_s(dir, kPaths[i], _TRUNCATE);
            wchar_t* slash = wcsrchr(dir, L'\\');
            if (slash) { *slash = 0; CreateDirectoryW(dir, nullptr); }

            FILE* f = nullptr;
            if (_wfopen_s(&f, kPaths[i], L"a+") == 0 && f)
            {
                fputws(line, f);
                fputws(L"\n", f);
                fflush(f);
                fclose(f);
            }
        }
        OutputDebugStringW(line);
        OutputDebugStringW(L"\n");
    }

    void CrashLog(const wchar_t* fmt, ...)
    {
        wchar_t buf[1024];
        va_list ap;
        va_start(ap, fmt);
        _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
        va_end(ap);

        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t line[1200];
        _snwprintf_s(line, _TRUNCATE, L"[%02d:%02d:%02d.%03d] %s",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);

        // Console may crash if CAD is mid-destruct — wrap lightly
        try { acutPrintf(L"\nPIPE_LOG: %s", buf); } catch (...) {}

        CrashLogWriteRaw(line);
    }

    AcGeVector3d NearestCardinal(const AcGeVector3d& v, bool& ok, double tolDeg = 15.0)
    {
        ok = false;
        if (v.isZeroLength())
            return AcGeVector3d::kIdentity;
        const AcGeVector3d n = v.normal();
        const AcGeVector3d axes[6] = {
            AcGeVector3d::kXAxis, -AcGeVector3d::kXAxis,
            AcGeVector3d::kYAxis, -AcGeVector3d::kYAxis,
            AcGeVector3d::kZAxis, -AcGeVector3d::kZAxis
        };
        double bestDot = -2.0;
        AcGeVector3d best = AcGeVector3d::kXAxis;
        for (int i = 0; i < 6; ++i)
        {
            const double d = n.dotProduct(axes[i]);
            if (d > bestDot) { bestDot = d; best = axes[i]; }
        }
        ok = (bestDot >= std::cos(tolDeg * kPi / 180.0));
        return best;
    }

    vCS_DM_Axis* GetAxis(vCSDragManager* pDM, const AcDbObjectId& idAxis)
    {
        if (!pDM)
            return nullptr;
        vCS_DM_Axis* pAxis = pDM->GetAxis(idAxis);
        if (!pAxis)
            pAxis = pDM->FindAxis(idAxis);
        return pAxis;
    }

    AcGeVector3d StableNormal(const AcGeVector3d& a, const AcGeVector3d& b)
    {
        AcGeVector3d n = a.crossProduct(b);
        if (n.isZeroLength())
        {
            n = a.crossProduct(AcGeVector3d::kZAxis);
            if (n.isZeroLength())
                n = a.crossProduct(AcGeVector3d::kYAxis);
            if (n.isZeroLength())
                n = a.crossProduct(AcGeVector3d::kXAxis);
        }
        if (n.isZeroLength())
            return AcGeVector3d::kZAxis;
        return n.normal();
    }

    AcGeVector3d ForceCardinal(const AcGeVector3d& v)
    {
        bool ok = false;
        return NearestCardinal(v, ok, 89.0);
    }

    // Turn Vin->Vout = turnDeg, keep local bend plane / 3D side (cone).
    AcGeVector3d OutDirForTurn(const AcGeVector3d& vinIn, const AcGeVector3d& voutIn, double turnDeg)
    {
        AcGeVector3d vin = vinIn.isZeroLength() ? AcGeVector3d::kXAxis : vinIn.normal();
        AcGeVector3d vout = voutIn.isZeroLength() ? vin.perpVector() : voutIn.normal();

        AcGeVector3d perp = vout - vin * vout.dotProduct(vin);
        if (perp.lengthSqrd() < 1e-12)
            perp = vin.perpVector();
        if (perp.isZeroLength())
            perp = StableNormal(vin, AcGeVector3d::kZAxis);
        perp = perp.normal();

        const double rad = turnDeg * kPi / 180.0;
        AcGeVector3d dir = vin * std::cos(rad) + perp * std::sin(rad);
        return dir.isZeroLength() ? perp : dir.normal();
    }

    double TurnOf(const AcGePoint3d& A, const AcGePoint3d& B, const AcGePoint3d& C)
    {
        AcGeVector3d vin = B - A;
        AcGeVector3d vout = C - B;
        if (vin.isZeroLength() || vout.isZeroLength())
            return 0.0;
        vin = vin.normal();
        vout = vout.normal();
        double c = vin.dotProduct(vout);
        c = (std::max)(-1.0, (std::min)(1.0, c));
        return std::acos(c) * 180.0 / kPi;
    }

    // Slide elbow B with A,C fixed. Search in bend plane (u along AC, signed height).
    bool SlideElbowLight(const AcGePoint3d& A, const AcGePoint3d& C, const AcGePoint3d& Bcur,
        double turnDeg, AcGePoint3d& Bbest, double& bestErr)
    {
        AcGeVector3d ac = C - A;
        const double acLen = ac.length();
        if (acLen < 2.0 * kMinSegLen)
            return false;

        const AcGeVector3d e1 = ac.normal();
        const AcGePoint3d foot0 = A + e1 * ((Bcur - A).dotProduct(e1));
        AcGeVector3d up0 = Bcur - foot0;
        if (up0.length() < 1.0)
        {
            AcGeVector3d n = StableNormal(Bcur - A, C - Bcur);
            up0 = n.crossProduct(e1);
            if (up0.isZeroLength())
                up0 = e1.perpVector();
            up0 = up0.normal() * (std::max)(kMinSegLen, acLen * 0.25);
        }
        const AcGeVector3d upDir = up0.normal();
        // base height: keep scale relative to AC (small turn => B near AC)
        const double hBase = (std::max)(up0.length(), acLen * 0.05);

        bestErr = 1e99;
        Bbest = Bcur;
        bool found = false;
        for (double u = 0.08; u <= 0.92; u += 0.02)
        {
            // signed height: both sides of AC, including near-line for small turns
            for (double hs = -3.0; hs <= 3.0; hs += 0.12)
            {
                if (std::abs(hs) < 0.02) continue; // exactly on AC => turn 0/180 degenerate
                const AcGePoint3d P = A + e1 * (u * acLen) + upDir * (hBase * hs);
                if (P.distanceTo(A) < kMinSegLen || P.distanceTo(C) < kMinSegLen)
                    continue;
                const double err = std::abs(TurnOf(A, P, C) - turnDeg);
                if (err < bestErr)
                {
                    bestErr = err;
                    Bbest = P;
                    found = true;
                }
            }
        }
        return found && bestErr <= 1.5;
    }

    bool IsElbowNode(vCS_DM_Node* p)
    {
        return p && p->IsElbow() != nullptr;
    }
}

void PipeAlignCrashLog(const wchar_t* msg)
{
    CrashLog(L"%s", msg ? msg : L"(null)");
}

CAlignDlg::CAlignDlg(AcDbObjectId idAxis, CWnd* pParent)
    : CDialog(IDD_ALIGN_PIPE, pParent)
    , m_idAxis(idAxis)
{
}

void CAlignDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHK_ORTHO, m_bWcsOrtho);
    DDX_Check(pDX, IDC_CHK_PROTECT_TERM, m_bProtectTerm);
}

BEGIN_MESSAGE_MAP(CAlignDlg, CDialog)
    ON_LBN_SELCHANGE(IDC_LIST_COORDS, &CAlignDlg::OnSelChangeCoords)
    ON_LBN_SELCHANGE(IDC_LIST_ELBOWS, &CAlignDlg::OnSelChangeElbows)
    ON_BN_CLICKED(IDC_BTN_ALIGN_COORDS, &CAlignDlg::OnAlignCoords)
    ON_BN_CLICKED(IDC_BTN_ALIGN_ALL_ELBOWS, &CAlignDlg::OnAlignAllElbows)
    ON_BN_CLICKED(IDC_BTN_APPLY_ELBOW_ANGLE, &CAlignDlg::OnApplyElbowAngle)
    ON_BN_CLICKED(IDC_BTN_APPLY_XYZ, &CAlignDlg::OnApplyXyz)
    ON_BN_CLICKED(IDC_BTN_ROUND_XYZ, &CAlignDlg::OnRoundXyz)
    ON_BN_CLICKED(IDCANCEL, &CAlignDlg::OnCloseDlg)
END_MESSAGE_MAP()

double CAlignDlg::RadToDeg(double r) { return r * 180.0 / kPi; }
double CAlignDlg::DegToRad(double d) { return d * kPi / 180.0; }

AcGePoint3d CAlignDlg::RoundPt(const AcGePoint3d& pt)
{
    return AcGePoint3d(std::round(pt.x), std::round(pt.y), std::round(pt.z));
}

bool CAlignDlg::IsWholeMm(const AcGePoint3d& pt)
{
    return std::abs(pt.x - std::round(pt.x)) < 1e-6
        && std::abs(pt.y - std::round(pt.y)) < 1e-6
        && std::abs(pt.z - std::round(pt.z)) < 1e-6;
}

double CAlignDlg::TurnAngleDeg(const AcGePoint3d& A, const AcGePoint3d& B, const AcGePoint3d& C)
{
    AcGeVector3d vin = B - A;
    AcGeVector3d vout = C - B;
    if (vin.isZeroLength() || vout.isZeroLength())
        return 0.0;
    vin = vin.normal();
    vout = vout.normal();
    double c = vin.dotProduct(vout);
    c = (std::max)(-1.0, (std::min)(1.0, c));
    return RadToDeg(std::acos(c));
}

double CAlignDlg::SnapToRightAngle(double deg)
{
    // Snap TURN angles: prefer 90 in typical band
    if (deg >= 55.0 && deg <= 125.0)
        return 90.0;
    static const double presets[] = { 45.0, 90.0, 135.0, 180.0 };
    double best = 90.0, bestDiff = 1e9;
    for (double p : presets)
    {
        const double d = std::abs(deg - p);
        if (d < bestDiff) { bestDiff = d; best = p; }
    }
    return best;
}

bool CAlignDlg::IsImmutableEnd(vCS_DM_Node* p, int idx, int nNodes)
{
    if (!p)
        return true;
    if (idx <= 0 || idx >= nNodes - 1)
        return true; // pipe ends never move
    if (p->IsTerminator())
        return true;
    if (p->PositionIsFixed())
        return true;
    return false;
}

bool CAlignDlg::MoveForTurnAngle(vCS_DM_Axis* pAxis, int elbowNodeIdx, double turnDeg)
{
    CrashLog(L"MoveForTurnAngle ENTER elbowIdx=%d turn=%.3f", elbowNodeIdx, turnDeg);

    if (!pAxis)
    {
        CrashLog(L"MoveForTurnAngle FAIL pAxis=null");
        return false;
    }
    const int n = pAxis->GetNodesCount();
    CrashLog(L"MoveForTurnAngle nodesCount=%d", n);
    if (elbowNodeIdx <= 0 || elbowNodeIdx + 1 >= n)
    {
        CrashLog(L"MoveForTurnAngle FAIL bad elbow index");
        return false;
    }

    CrashLog(L"MoveForTurnAngle getNode A/B/C...");
    vCS_DM_Node* pA = pAxis->GetNode(elbowNodeIdx - 1);
    vCS_DM_Node* pB = pAxis->GetNode(elbowNodeIdx);
    vCS_DM_Node* pC = pAxis->GetNode(elbowNodeIdx + 1);
    if (!pA || !pB || !pC)
    {
        CrashLog(L"MoveForTurnAngle FAIL null neighbor A=%p B=%p C=%p", pA, pB, pC);
        return false;
    }

    const bool lockA = IsImmutableEnd(pA, elbowNodeIdx - 1, n);
    const bool lockC = IsImmutableEnd(pC, elbowNodeIdx + 1, n);
    const bool lockB = pB->PositionIsFixed() || (pB->IsTerminator() != nullptr);
    CrashLog(L"MoveForTurnAngle locks A=%d B=%d C=%d", lockA ? 1 : 0, lockB ? 1 : 0, lockC ? 1 : 0);

    AcGePoint3d A = pA->BasePoint();
    AcGePoint3d B = pB->BasePoint();
    AcGePoint3d C = pC->BasePoint();
    CrashLog(L"MoveForTurnAngle A=(%.1f %.1f %.1f) B=(%.1f %.1f %.1f) C=(%.1f %.1f %.1f)",
        A.x, A.y, A.z, B.x, B.y, B.z, C.x, C.y, C.z);

    if ((B - A).isZeroLength() || (C - B).isZeroLength())
    {
        CrashLog(L"MoveForTurnAngle FAIL zero-length segment");
        return false;
    }

    const double curTurn = TurnOf(A, B, C);
    CrashLog(L"MoveForTurnAngle curTurn=%.3f target=%.3f", curTurn, turnDeg);

    // SAFE kinematics only:
    // - rotate free NEXT/PREV leg, keep segment length
    // - NEVER slide elbow B (teleports and breaks fittings)
    // - NEVER move axis ends or neighbor elbows
    // If no free mid-node leg — return false; caller uses MS FixedAngle+Recalc.
    const bool elbowA = IsElbowNode(pA);
    const bool elbowC = IsElbowNode(pC);
    const bool canMoveC = !lockC && !elbowC;
    const bool canMoveA = !lockA && !elbowA;
    CrashLog(L"MoveForTurnAngle canMove A=%d C=%d (elbowA=%d elbowC=%d)",
        canMoveA ? 1 : 0, canMoveC ? 1 : 0, elbowA ? 1 : 0, elbowC ? 1 : 0);

    auto applyPt = [&](vCS_DM_Node* pNode, const AcGePoint3d& pt, const wchar_t* tag) -> bool
    {
        const double dist = pNode->BasePoint().distanceTo(pt);
        CrashLog(L"MoveForTurnAngle SetBasePoint %s -> (%.1f %.1f %.1f) d=%.1f BEFORE",
            tag, pt.x, pt.y, pt.z, dist);
        // Refuse huge jumps (broken pipes) — keep within 2x current segment length.
        if (dist > 5000.0)
        {
            CrashLog(L"MoveForTurnAngle REFUSE huge move d=%.1f", dist);
            return false;
        }
        const Acad::ErrorStatus es = pNode->SetBasePoint(pt, true);
        CrashLog(L"MoveForTurnAngle SetBasePoint %s AFTER es=%d", tag, (int)es);
        return es == Acad::eOk;
    };

    if (canMoveC)
    {
        CrashLog(L"MoveForTurnAngle strategy=MOVE_NEXT");
        const AcGeVector3d outDir = OutDirForTurn(B - A, C - B, turnDeg);
        double outLen = (C - B).length();
        if (outLen < kMinSegLen) outLen = kMinSegLen;
        const AcGePoint3d Cnew = RoundPt(B + outDir * outLen);
        CrashLog(L"MoveForTurnAngle geomTurn after C=%.3f", TurnOf(A, B, Cnew));
        return applyPt(pC, Cnew, L"C");
    }

    if (canMoveA)
    {
        CrashLog(L"MoveForTurnAngle strategy=MOVE_PREV");
        AcGeVector3d vout = C - B;
        AcGeVector3d vinCur = B - A;
        AcGeVector3d vin = OutDirForTurn(vout, vinCur, turnDeg);
        double inLen = (B - A).length();
        if (inLen < kMinSegLen) inLen = kMinSegLen;
        const AcGePoint3d Anew = RoundPt(B - vin * inLen);
        CrashLog(L"MoveForTurnAngle geomTurn after A=%.3f", TurnOf(Anew, B, C));
        return applyPt(pA, Anew, L"A");
    }

    CrashLog(L"MoveForTurnAngle NO free leg — leave to MS FixedAngle/Recalc");
    return false; // not a hard fail for Apply: MS path can still run
}

void CAlignDlg::SnapshotEnds(vCS_DM_Axis* pAxis, AcGePoint3d& pt0, AcGePoint3d& ptN, int& nNodes)
{
    nNodes = 0;
    pt0 = ptN = AcGePoint3d::kOrigin;
    if (!pAxis) return;
    nNodes = pAxis->GetNodesCount();
    if (nNodes <= 0) return;
    if (vCS_DM_Node* p0 = pAxis->GetNode(0))
        pt0 = p0->BasePoint();
    if (vCS_DM_Node* pN = pAxis->GetNode(nNodes - 1))
        ptN = pN->BasePoint();
}

void CAlignDlg::RestoreEnds(vCS_DM_Axis* pAxis, const AcGePoint3d& pt0, const AcGePoint3d& ptN)
{
    if (!pAxis) return;
    const int n = pAxis->GetNodesCount();
    if (n <= 0) return;
    if (vCS_DM_Node* p0 = pAxis->GetNode(0))
    {
        if (p0->BasePoint().distanceTo(pt0) > 0.5)
        {
            CrashLog(L"RestoreEnds: start moved -> restoring");
            p0->SetBasePoint(pt0, true);
            p0->SetPositionIsFixed(true);
        }
    }
    if (vCS_DM_Node* pN = pAxis->GetNode(n - 1))
    {
        if (pN->BasePoint().distanceTo(ptN) > 0.5)
        {
            CrashLog(L"RestoreEnds: end moved -> restoring");
            pN->SetBasePoint(ptN, true);
            pN->SetPositionIsFixed(true);
        }
    }
}

void CAlignDlg::ClearHighlight()
{
    acedSSSetFirst(nullptr, nullptr);
    m_highlighted.setNull();
}

bool CAlignDlg::RefreshFromModel()
{
    CrashLog(L"RefreshFromModel ENTER");
    m_nodes.clear();
    m_elbowIdx.clear();

    vCSDragManagerSmart dms(m_idAxis);
    CrashLog(L"RefreshFromModel DM OK");
    vCSDragManager* pDM = dms.operator->();
    vCS_DM_Axis* pAxis = GetAxis(pDM, m_idAxis);
    if (!pAxis)
    {
        CrashLog(L"RefreshFromModel FAIL no axis");
        return false;
    }

    const int nCount = pAxis->GetNodesCount();
    std::vector<AcGePoint3d> pts(nCount);
    std::vector<vCS_DM_Node*> nodes(nCount, nullptr);
    for (int i = 0; i < nCount; ++i)
    {
        nodes[i] = pAxis->GetNode(i);
        pts[i] = nodes[i] ? nodes[i]->BasePoint() : AcGePoint3d::kOrigin;
    }

    for (int i = 0; i < nCount; ++i)
    {
        vCS_DM_Node* pNode = nodes[i];
        if (!pNode)
            continue;

        AlignNodeInfo info;
        info.index = i;
        info.pt = pts[i];
        info.oid = pNode->OID();
        info.needsRound = !IsWholeMm(info.pt);
        info.isTerm = (pNode->IsTerminator() != nullptr);
        info.isEnd = (i == 0 || i == nCount - 1);
        info.isLocked = info.isEnd || info.isTerm || pNode->PositionIsFixed();

        if (vCS_DM_NodeImplElbow* pElbow = pNode->IsElbow())
        {
            info.isElbow = true;
            // Prefer geometric TURN angle; fall back to MS value
            if (i > 0 && i + 1 < nCount)
                info.angleDeg = TurnAngleDeg(pts[i - 1], pts[i], pts[i + 1]);
            else
                info.angleDeg = RadToDeg(pElbow->GetElbowAngle());
            m_elbowIdx.push_back(static_cast<int>(m_nodes.size()));
        }
        m_nodes.push_back(info);
    }
    return true;
}

void CAlignDlg::FillLists()
{
    CListBox* pC = static_cast<CListBox*>(GetDlgItem(IDC_LIST_COORDS));
    CListBox* pE = static_cast<CListBox*>(GetDlgItem(IDC_LIST_ELBOWS));
    if (!pC || !pE)
        return;

    const int keepC = pC->GetCurSel();
    const int keepE = pE->GetCurSel();
    pC->ResetContent();
    pE->ResetContent();

    int badCoords = 0, badElbows = 0;
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        const AlignNodeInfo& n = m_nodes[i];
        if (n.needsRound) ++badCoords;
        const AcGePoint3d r = RoundPt(n.pt);
        CString flags;
        if (n.isEnd) flags += L"[END] ";
        if (n.isTerm) flags += L"[TERM] ";
        else if (n.isLocked && !n.isEnd) flags += L"[LOCK] ";
        if (n.isElbow) flags += L"[ELBOW] ";
        CString s;
        s.Format(L"%s%s#%d  X=%.3f Y=%.3f Z=%.3f  -> %.0f %.0f %.0f",
            n.needsRound ? L"! " : L"  ",
            static_cast<LPCTSTR>(flags),
            n.index + 1, n.pt.x, n.pt.y, n.pt.z, r.x, r.y, r.z);
        pC->AddString(s);
    }
    for (size_t ei = 0; ei < m_elbowIdx.size(); ++ei)
    {
        const AlignNodeInfo& n = m_nodes[m_elbowIdx[ei]];
        const double snap = SnapToRightAngle(n.angleDeg);
        const bool bad = std::abs(n.angleDeg - snap) > 0.5;
        if (bad) ++badElbows;

        // neighbor lock hints
        CString nb;
        if (n.index > 0 && n.index < (int)m_nodes.size())
        {
            // m_nodes is parallel to axis nodes in our fill — find by index
        }
        bool prevLock = false, nextLock = false;
        for (const auto& x : m_nodes)
        {
            if (x.index == n.index - 1) prevLock = x.isLocked;
            if (x.index == n.index + 1) nextLock = x.isLocked;
        }
        if (nextLock) nb = L" next=LOCK";
        else if (prevLock) nb = L" prev=LOCK";

        CString s;
        s.Format(L"%sElbow #%d node %d  TURN=%.2f  snap->%.0f%s",
            bad ? L"! " : L"  ",
            static_cast<int>(ei) + 1, n.index + 1, n.angleDeg, snap,
            static_cast<LPCTSTR>(nb));
        pE->AddString(s);
    }

    CString info;
    info.Format(L"Nodes: %d  Elbows: %d  |  bad XYZ: %d  bad TURN: %d",
        static_cast<int>(m_nodes.size()), static_cast<int>(m_elbowIdx.size()),
        badCoords, badElbows);
    SetDlgItemText(IDC_STATIC_INFO, info);

    if (keepC >= 0 && keepC < pC->GetCount()) pC->SetCurSel(keepC);
    if (keepE >= 0 && keepE < pE->GetCount()) pE->SetCurSel(keepE);
}

void CAlignDlg::ShowNodeEdits(int idx)
{
    m_selNode = idx;
    if (idx < 0 || idx >= static_cast<int>(m_nodes.size()))
        return;
    const AlignNodeInfo& n = m_nodes[idx];
    CString sx, sy, sz;
    sx.Format(L"%.3f", n.pt.x);
    sy.Format(L"%.3f", n.pt.y);
    sz.Format(L"%.3f", n.pt.z);
    SetDlgItemText(IDC_EDIT_X, sx);
    SetDlgItemText(IDC_EDIT_Y, sy);
    SetDlgItemText(IDC_EDIT_Z, sz);
    CString h;
    if (n.isLocked)
        h.Format(L"Node #%d is TERM/LOCK — XYZ apply may be blocked by protect.", n.index + 1);
    else
        h.Format(L"Node #%d. Edit X/Y/Z then Apply XYZ or Round.", n.index + 1);
    SetDlgItemText(IDC_STATIC_XYZ, h);
}

void CAlignDlg::HighlightElbow(int elbowListIndex)
{
    ClearHighlight();
    if (elbowListIndex < 0 || elbowListIndex >= static_cast<int>(m_elbowIdx.size()))
        return;

    const AlignNodeInfo& n = m_nodes[m_elbowIdx[elbowListIndex]];
    CString cur, ed;
    cur.Format(L"TURN: %.2f  (snap %.0f) — type ANY angle, ends fixed",
        n.angleDeg, SnapToRightAngle(n.angleDeg));
    ed.Format(L"%.2f", n.angleDeg); // keep current / allow random edit
    SetDlgItemText(IDC_STATIC_ANGLE_CUR, cur);
    SetDlgItemText(IDC_EDIT_ANGLE, ed);

    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].index == n.index)
        {
            CListBox* pC = static_cast<CListBox*>(GetDlgItem(IDC_LIST_COORDS));
            if (pC) pC->SetCurSel(static_cast<int>(i));
            ShowNodeEdits(static_cast<int>(i));
            break;
        }
    }

    if (n.oid.isNull())
        return;
    ads_name en, ss;
    if (acdbGetAdsName(en, n.oid) == Acad::eOk)
    {
        if (acedSSAdd(en, nullptr, ss) == RTNORM)
        {
            acedSSSetFirst(ss, nullptr);
            acedSSFree(ss);
            m_highlighted = n.oid;
        }
    }
}

BOOL CAlignDlg::OnInitDialog()
{
    CrashLog(L"OnInitDialog ENTER");
    CDialog::OnInitDialog();
    CheckDlgButton(IDC_CHK_ORTHO, BST_UNCHECKED); // WCS ortho OFF — keeps 3D
    CheckDlgButton(IDC_CHK_PROTECT_TERM, BST_CHECKED);
    m_bWcsOrtho = FALSE;
    m_bProtectTerm = TRUE;
    if (!RefreshFromModel())
    {
        CrashLog(L"OnInitDialog RefreshFromModel FAIL");
        AfxMessageBox(L"Cannot open pipe axis in Drag Manager.", MB_ICONERROR);
        EndDialog(IDCANCEL);
        return FALSE;
    }
    FillLists();
    CrashLog(L"OnInitDialog DONE nodes=%d elbows=%d log=Documents\\ModelStudioPipeTools\\PIPE_ALIGN_crash.log",
        (int)m_nodes.size(), (int)m_elbowIdx.size());
    return TRUE;
}

void CAlignDlg::OnCloseDlg()
{
    ClearHighlight();
    EndDialog(IDCANCEL);
}

void CAlignDlg::OnSelChangeCoords()
{
    CListBox* p = static_cast<CListBox*>(GetDlgItem(IDC_LIST_COORDS));
    if (p) ShowNodeEdits(p->GetCurSel());
}

void CAlignDlg::OnSelChangeElbows()
{
    CListBox* p = static_cast<CListBox*>(GetDlgItem(IDC_LIST_ELBOWS));
    if (p) HighlightElbow(p->GetCurSel());
}

bool CAlignDlg::ReadXyz(AcGePoint3d& pt) const
{
    CString sx, sy, sz;
    GetDlgItemText(IDC_EDIT_X, sx);
    GetDlgItemText(IDC_EDIT_Y, sy);
    GetDlgItemText(IDC_EDIT_Z, sz);
    sx.Trim(); sy.Trim(); sz.Trim();
    if (sx.IsEmpty() || sy.IsEmpty() || sz.IsEmpty())
        return false;
    sx.Replace(L',', L'.');
    sy.Replace(L',', L'.');
    sz.Replace(L',', L'.');
    pt.x = _wtof(sx); pt.y = _wtof(sy); pt.z = _wtof(sz);
    return true;
}

void CAlignDlg::OnApplyXyz()
{
    UpdateData(TRUE);
    if (m_selNode < 0 || m_selNode >= static_cast<int>(m_nodes.size()))
    {
        AfxMessageBox(L"Select a node first.", MB_ICONWARNING);
        return;
    }
    if (m_nodes[m_selNode].isEnd || m_nodes[m_selNode].isTerm)
    {
        AfxMessageBox(L"Pipe ends / terminators are immutable.", MB_ICONWARNING);
        return;
    }
    AcGePoint3d pt;
    if (!ReadXyz(pt))
    {
        AfxMessageBox(L"Bad X/Y/Z.", MB_ICONWARNING);
        return;
    }

    ClearHighlight();
    const int nodeIdx = m_nodes[m_selNode].index;
    const int keep = m_selNode;

    {
        vCSDragManagerSmart dms(m_idAxis);
        vCSDragManager* pDM = dms.operator->();
        vCS_DM_Axis* pAxis = GetAxis(pDM, m_idAxis);
        if (!pAxis)
        {
            AfxMessageBox(L"Cannot open axis.", MB_ICONERROR);
            return;
        }
        vCS_DM_Node* pNode = pAxis->GetNode(nodeIdx);
        if (!pNode || pNode->SetBasePoint(pt, true) != Acad::eOk)
        {
            AfxMessageBox(L"SetBasePoint failed.", MB_ICONERROR);
            return;
        }
        pDM->m_ReCalculate.SetAxis(m_idAxis);
        pDM->SetDragType(vCS::eReCalculate);
        if (pDM->ReCalculateModelMain() != Acad::eOk)
        {
            AfxMessageBox(L"Recalculate failed.", MB_ICONERROR);
            return;
        }
        dms.commit();
    }

    RefreshFromModel();
    FillLists();
    m_selNode = keep;
    if (keep >= 0 && keep < static_cast<int>(m_nodes.size()))
    {
        CListBox* pC = static_cast<CListBox*>(GetDlgItem(IDC_LIST_COORDS));
        if (pC) pC->SetCurSel(keep);
        ShowNodeEdits(keep);
    }
    acutPrintf(L"\nPIPE_ALIGN: XYZ applied node %d", nodeIdx + 1);
}

void CAlignDlg::OnRoundXyz()
{
    if (m_selNode < 0 || m_selNode >= static_cast<int>(m_nodes.size()))
    {
        AfxMessageBox(L"Select a node first.", MB_ICONWARNING);
        return;
    }
    AcGePoint3d pt;
    if (!ReadXyz(pt))
        pt = m_nodes[m_selNode].pt;
    pt = RoundPt(pt);
    CString sx, sy, sz;
    sx.Format(L"%.0f", pt.x);
    sy.Format(L"%.0f", pt.y);
    sz.Format(L"%.0f", pt.z);
    SetDlgItemText(IDC_EDIT_X, sx);
    SetDlgItemText(IDC_EDIT_Y, sy);
    SetDlgItemText(IDC_EDIT_Z, sz);
    OnApplyXyz();
}

void CAlignDlg::OnAlignCoords()
{
    UpdateData(TRUE);
    ClearHighlight();

    int moved = 0;
    {
        vCSDragManagerSmart dms(m_idAxis);
        vCSDragManager* pDM = dms.operator->();
        vCS_DM_Axis* pAxis = GetAxis(pDM, m_idAxis);
        if (!pAxis)
        {
            AfxMessageBox(L"Cannot open axis.", MB_ICONERROR);
            return;
        }

        for (int i = 0; i < pAxis->GetNodesCount(); ++i)
        {
            vCS_DM_Node* pNode = pAxis->GetNode(i);
            if (!pNode) continue;
            if (IsImmutableEnd(pNode, i, pAxis->GetNodesCount()))
                continue;
            const AcGePoint3d oldPt = pNode->BasePoint();
            const AcGePoint3d newPt = RoundPt(oldPt);
            if (oldPt.distanceTo(newPt) > 1e-9 &&
                pNode->SetBasePoint(newPt, true) == Acad::eOk)
                ++moved;
        }

        // Optional WORLD-axis ortho — flattens skewed 3D routes into XYZ.
        // Leave OFF to keep each elbow in its own spatial plane.
        if (m_bWcsOrtho)
        {
            const int n = pAxis->GetNodesCount();
            for (int i = 0; i + 1 < n; ++i)
            {
                vCS_DM_Node* pA = pAxis->GetNode(i);
                vCS_DM_Node* pB = pAxis->GetNode(i + 1);
                if (!pA || !pB) continue;
                if (IsImmutableEnd(pB, i + 1, n))
                    continue;
                AcGeVector3d vec = pB->BasePoint() - pA->BasePoint();
                if (vec.isZeroLength()) continue;
                bool nearCardinal = false;
                const AcGeVector3d card = NearestCardinal(vec, nearCardinal, 15.0);
                if (!nearCardinal) continue;
                const double len = (std::max)(kMinSegLen, std::round(vec.length()));
                const AcGePoint3d aligned = RoundPt(pA->BasePoint() + card * len);
                if (pB->BasePoint().distanceTo(aligned) > 1e-9 &&
                    pB->SetBasePoint(aligned, true) == Acad::eOk)
                    ++moved;
            }
        }

        if (moved == 0)
        {
            AfxMessageBox(L"Already aligned (or all remaining nodes locked).", MB_ICONINFORMATION);
            return;
        }

        pDM->m_ReCalculate.SetAxis(m_idAxis);
        pDM->SetDragType(vCS::eReCalculate);
        if (pDM->ReCalculateModelMain() != Acad::eOk)
        {
            AfxMessageBox(L"Recalculate failed.", MB_ICONERROR);
            return;
        }
        dms.commit();
    }

    RefreshFromModel();
    FillLists();
    acutPrintf(L"\nPIPE_ALIGN: coords moved %d", moved);
}

void CAlignDlg::OnAlignAllElbows()
{
    CrashLog(L"OnAlignAllElbows ENTER");
    UpdateData(TRUE);
    ClearHighlight();
    int moved = 0;
    {
        CrashLog(L"OnAlignAllElbows open DM...");
        vCSDragManagerSmart dms(m_idAxis);
        vCSDragManager* pDM = dms.operator->();
        vCS_DM_Axis* pAxis = GetAxis(pDM, m_idAxis);
        if (!pAxis)
        {
            CrashLog(L"OnAlignAllElbows FAIL no axis");
            AfxMessageBox(L"Cannot open axis.", MB_ICONERROR);
            return;
        }
        CrashLog(L"OnAlignAllElbows axis OK, loop elbows...");
        AcGePoint3d end0, endN;
        int nSnap = 0;
        SnapshotEnds(pAxis, end0, endN, nSnap);

        for (int i = pAxis->GetNodesCount() - 1; i >= 0; --i)
        {
            vCS_DM_Node* pNode = pAxis->GetNode(i);
            if (!pNode) continue;
            vCS_DM_NodeImplElbow* pElbow = pNode->IsElbow();
            if (!pElbow) continue;

            vCS_DM_Node* pA = pAxis->GetNode(i - 1);
            vCS_DM_Node* pC = pAxis->GetNode(i + 1);
            if (!pA || !pC) continue;

            const double curTurn = TurnAngleDeg(pA->BasePoint(), pNode->BasePoint(), pC->BasePoint());
            const double snapDeg = SnapToRightAngle(curTurn);
            CrashLog(L"OnAlignAllElbows elbow i=%d cur=%.2f snap=%.0f", i, curTurn, snapDeg);
            if (std::abs(curTurn - snapDeg) <= 1.0)
                continue;

            // Gentle: only rotate a free mid-leg. No elbow slide (breaks pipes).
            if (MoveForTurnAngle(pAxis, i, snapDeg))
                ++moved;

            // Let MS CorrectFixedAngle do the rest.
            CrashLog(L"OnAlignAllElbows SetAngleIsFixed eaSet i=%d ang=%.0f", i, snapDeg);
            pElbow->SetAngleIsFixed(eaSet);
            pElbow->SetFixedAngleValue(DegToRad(snapDeg));
            ++moved;
            RestoreEnds(pAxis, end0, endN);
        }

        CrashLog(L"OnAlignAllElbows moved=%d", moved);
        RestoreEnds(pAxis, end0, endN);
        if (vCS_DM_Node* p0 = pAxis->GetNode(0))
            p0->SetPositionIsFixed(true);
        if (nSnap > 0)
            if (vCS_DM_Node* pN = pAxis->GetNode(nSnap - 1))
                pN->SetPositionIsFixed(true);

        CrashLog(L"OnAlignAllElbows ReCalculate BEFORE");
        pDM->m_ReCalculate.SetAxis(m_idAxis);
        pDM->SetDragType(vCS::eReCalculate);
        const Acad::ErrorStatus es = pDM->ReCalculateModelMain();
        CrashLog(L"OnAlignAllElbows ReCalculate AFTER es=%d", (int)es);
        RestoreEnds(pAxis, end0, endN);

        CrashLog(L"OnAlignAllElbows UpdateDBEnt BEFORE");
        pDM->UpdateDBEnt();
        CrashLog(L"OnAlignAllElbows UpdateDBEnt AFTER");
        RestoreEnds(pAxis, end0, endN);

        CrashLog(L"OnAlignAllElbows commit BEFORE");
        dms.commit();
        CrashLog(L"OnAlignAllElbows commit AFTER / DM dtor next");
    }

    CrashLog(L"OnAlignAllElbows RefreshFromModel...");
    RefreshFromModel();
    FillLists();
    CrashLog(L"OnAlignAllElbows DONE moved=%d", moved);
}

void CAlignDlg::OnApplyElbowAngle()
{
    CrashLog(L"OnApplyElbowAngle ENTER");
    UpdateData(TRUE);
    CListBox* pList = static_cast<CListBox*>(GetDlgItem(IDC_LIST_ELBOWS));
    if (!pList) return;
    const int sel = pList->GetCurSel();
    if (sel < 0 || sel >= static_cast<int>(m_elbowIdx.size()))
    {
        AfxMessageBox(L"Select an elbow first.", MB_ICONWARNING);
        return;
    }

    CString angleStr;
    GetDlgItemText(IDC_EDIT_ANGLE, angleStr);
    angleStr.Trim();
    angleStr.Replace(L',', L'.');
    const double newDeg = _wtof(angleStr);
    CrashLog(L"OnApplyElbowAngle sel=%d angleStr=%s newDeg=%.3f", sel, static_cast<LPCWSTR>(angleStr), newDeg);
    if (newDeg <= 0.0 || newDeg > 180.0)
    {
        AfxMessageBox(L"Turn angle must be (0; 180].", MB_ICONWARNING);
        return;
    }

    ClearHighlight();
    const int nodeIdx = m_nodes[m_elbowIdx[sel]].index;
    CrashLog(L"OnApplyElbowAngle nodeIdx=%d open DM...", nodeIdx);

    {
        vCSDragManagerSmart dms(m_idAxis);
        CrashLog(L"OnApplyElbowAngle DM constructed");
        vCSDragManager* pDM = dms.operator->();
        vCS_DM_Axis* pAxis = GetAxis(pDM, m_idAxis);
        if (!pAxis)
        {
            CrashLog(L"OnApplyElbowAngle FAIL no axis");
            return;
        }
        vCS_DM_Node* pNode = pAxis->GetNode(nodeIdx);
        vCS_DM_NodeImplElbow* pElbow = pNode ? pNode->IsElbow() : nullptr;
        if (!pElbow)
        {
            CrashLog(L"OnApplyElbowAngle FAIL not elbow");
            return;
        }

        AcGePoint3d end0, endN;
        int nSnap = 0;
        SnapshotEnds(pAxis, end0, endN, nSnap);
        CrashLog(L"OnApplyElbowAngle ends pinned start=(%.1f %.1f %.1f) end=(%.1f %.1f %.1f)",
            end0.x, end0.y, end0.z, endN.x, endN.y, endN.z);

        // Optional gentle leg rotate (keeps length). If no free mid-node — MS FixedAngle only.
        CrashLog(L"OnApplyElbowAngle call MoveForTurnAngle...");
        const bool movedLeg = MoveForTurnAngle(pAxis, nodeIdx, newDeg);
        CrashLog(L"OnApplyElbowAngle MoveForTurnAngle movedLeg=%d", movedLeg ? 1 : 0);

        vCS_DM_Node* pA = pAxis->GetNode(nodeIdx - 1);
        vCS_DM_Node* pB = pAxis->GetNode(nodeIdx);
        vCS_DM_Node* pC = pAxis->GetNode(nodeIdx + 1);
        if (!pA || !pB || !pC)
        {
            CrashLog(L"OnApplyElbowAngle FAIL null neighbors");
            return;
        }

        const int nAll = pAxis->GetNodesCount();
        if (vCS_DM_Node* p0 = pAxis->GetNode(0))
            p0->SetPositionIsFixed(true);
        if (vCS_DM_Node* pN = pAxis->GetNode(nAll - 1))
            pN->SetPositionIsFixed(true);

        // eaSet: MS CorrectFixedAngle adjusts route to catalog/fixed angle without our slide hacks.
        CrashLog(L"OnApplyElbowAngle SetAngleIsFixed eaSet + FixedAngle=%.3f", newDeg);
        pElbow->SetAngleIsFixed(eaSet);
        pElbow->SetFixedAngleValue(DegToRad(newDeg));

        CrashLog(L"OnApplyElbowAngle ReCalculate BEFORE");
        pDM->m_ReCalculate.SetAxis(m_idAxis);
        pDM->SetDragType(vCS::eReCalculate);
        const Acad::ErrorStatus esRecalc = pDM->ReCalculateModelMain();
        CrashLog(L"OnApplyElbowAngle ReCalculate AFTER es=%d", (int)esRecalc);

        // Only restore ends — do NOT re-stamp mid points (that breaks elbow L1/L2 fittings).
        RestoreEnds(pAxis, end0, endN);

        CrashLog(L"OnApplyElbowAngle UpdateDBEnt BEFORE");
        pDM->UpdateDBEnt();
        CrashLog(L"OnApplyElbowAngle UpdateDBEnt AFTER");
        RestoreEnds(pAxis, end0, endN);

        const double kept = TurnAngleDeg(pA->BasePoint(), pB->BasePoint(), pC->BasePoint());
        CrashLog(L"OnApplyElbowAngle BEFORE commit geometric TURN=%.3f (target=%.3f)", kept, newDeg);

        CrashLog(L"OnApplyElbowAngle commit BEFORE");
        dms.commit();
        CrashLog(L"OnApplyElbowAngle commit AFTER / DM dtor next");
    }

    CrashLog(L"OnApplyElbowAngle RefreshFromModel...");
    RefreshFromModel();
    FillLists();
    double actual = newDeg;
    if (sel < static_cast<int>(m_elbowIdx.size()))
    {
        pList->SetCurSel(sel);
        HighlightElbow(sel);
        actual = m_nodes[m_elbowIdx[sel]].angleDeg;
    }
    CrashLog(L"OnApplyElbowAngle DONE requested=%.2f actualTURN=%.2f", newDeg, actual);
    if (std::abs(actual - newDeg) > 1.0)
    {
        CString msg;
        msg.Format(L"Requested TURN %.1f but model kept %.1f after ReCalculate.\nSee PIPE_ALIGN_crash.log",
            newDeg, actual);
        AfxMessageBox(msg, MB_ICONWARNING);
    }
}

// ----- InsulDlg.cpp -----
#include "aced.h"
#include "dbapserv.h"

#include "vCSAxis.h"
#include "vCSSegment.h"
#include "vCSSubSegment.h"
#include "vCSEntity.h"
#include "vCSWeld.h"
#include "MultiPipeInsulation.h"
#include "ParamsObject.h"
#include "vCSToolsStr.h"

#include <algorithm>
#include <map>

namespace
{
    struct RegionDef { const wchar_t* name; double k; double tAirOut; };

    const RegionDef kRegions[] = {
        { L"CFO-MSK", 1.00, -25.0 }, { L"CFO-YAR", 1.00, -28.0 },
        { L"SZFO-SPB", 1.00, -24.0 }, { L"SZFO-ARK", 0.96, -35.0 },
        { L"UFO-ROST", 1.00, -19.0 }, { L"UFO-VOLG", 1.00, -22.0 },
        { L"SKFO", 1.00, -16.0 }, { L"PFO-NN", 1.00, -30.0 },
        { L"PFO-SAM", 1.00, -27.0 }, { L"UFO-EKB", 0.98, -32.0 },
        { L"UFO-PERM", 0.98, -35.0 }, { L"SFO-NSK", 0.98, -37.0 },
        { L"SFO-KRS", 0.98, -40.0 }, { L"DFO-KHB", 0.96, -28.0 },
        { L"DFO-YKT", 0.96, -50.0 }, { L"CRIMEA", 1.00, -15.0 },
        { L"KALIN", 1.00, -18.0 },
    };

    const wchar_t* kRegionsRu[] = {
        L"Центральный ФО - Москва, Тула, Воронеж",
        L"Центральный ФО - Ярославль, Тверь",
        L"Северо-Западный ФО - Санкт-Петербург",
        L"Северо-Западный ФО - Архангельск, Мурманск",
        L"Южный ФО - Ростов-на-Дону, Краснодар",
        L"Южный ФО - Волгоград, Астрахань",
        L"Северо-Кавказский ФО - Ставрополь, Грозный",
        L"Приволжский ФО - Нижний Новгород, Казань",
        L"Приволжский ФО - Самара, Саратов",
        L"Уральский ФО - Екатеринбург, Челябинск",
        L"Уральский ФО - Пермь, Тюмень",
        L"Сибирский ФО - Новосибирск, Омск",
        L"Сибирский ФО - Красноярск, Иркутск",
        L"Дальневосточный ФО - Хабаровск, Владивосток",
        L"Дальневосточный ФО - Якутск, Магадан (Север)",
        L"Крым / Севастополь",
        L"Калининградская область",
    };
    constexpr int kRegionCount = (int)(sizeof(kRegions) / sizeof(kRegions[0]));

    Acad::ErrorStatus AppendToModelSpace(AcDbEntity* pEnt)
    {
        if (!pEnt) return Acad::eNullEntityPointer;
        AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
        if (!pDb) return Acad::eNoDatabase;
        AcDbBlockTable* pBT = nullptr;
        Acad::ErrorStatus es = pDb->getBlockTable(pBT, AcDb::kForRead);
        if (es != Acad::eOk || !pBT) return es;
        AcDbBlockTableRecord* pMS = nullptr;
        es = pBT->getAt(ACDB_MODEL_SPACE, pMS, AcDb::kForWrite);
        pBT->close();
        if (es != Acad::eOk || !pMS) return es;
        es = pMS->appendAcDbEntity(pEnt);
        pMS->close();
        return es;
    }

    void AppendUnique(AcDbObjectIdArray& dst, const AcDbObjectId& id)
    {
        if (id.isNull()) return;
        int pos = -1;
        if (!dst.find(id, pos)) dst.append(id);
    }

    void CollectAllAxes(AcDbObjectIdArray& axes)
    {
        axes.removeAll();
        AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
        if (!pDb) return;

        // Official MS API: axes live in named dictionary "Axis", not model space
        vCSAxis::GetAllAxis(axes, pDb);
        if (axes.length() == 0)
            vCSAxis::GetAllAxisX(axes, pDb);

        // Fallback: unique axes referenced by pipe entities in model space
        if (axes.length() == 0)
        {
            AcDbBlockTable* pBT = nullptr;
            if (pDb->getBlockTable(pBT, AcDb::kForRead) != Acad::eOk || !pBT) return;
            AcDbBlockTableRecord* pMS = nullptr;
            if (pBT->getAt(ACDB_MODEL_SPACE, pMS, AcDb::kForRead) != Acad::eOk || !pMS)
            {
                pBT->close();
                return;
            }
            AcDbBlockTableRecordIterator* pIt = nullptr;
            if (pMS->newIterator(pIt) == Acad::eOk && pIt)
            {
                for (; !pIt->done(); pIt->step())
                {
                    AcDbObjectId id;
                    pIt->getEntityId(id);
                    AcDbObject* pObj = nullptr;
                    if (acdbOpenObject(pObj, id, AcDb::kForRead) != Acad::eOk || !pObj) continue;
                    if (pObj->isKindOf(vCSEntity::desc()))
                    {
                        vCSEntity* pE = vCSEntity::cast(pObj);
                        if (pE)
                        {
                            const AcDbObjectId ax = pE->GetOIdAxis();
                            if (!ax.isNull()) AppendUnique(axes, ax);
                        }
                    }
                    else if (pObj->isKindOf(vCSAxis::desc()))
                    {
                        AppendUnique(axes, id);
                    }
                    pObj->close();
                }
                delete pIt;
            }
            pMS->close();
            pBT->close();
        }

        acutPrintf(L"\n[PIPE_INSUL] axes found: %d\n", axes.length());
    }

    void SetPartInsulationOnEntity(AcDbObjectId id, double thicknessMm, double tMediaC,
        const CString& regionName, const CString& mediaName)
    {
        vCSEntity* pEnt = nullptr;
        if (acdbOpenObject(pEnt, id, AcDb::kForWrite) != Acad::eOk || !pEnt)
            return;
        CString sD, sT;
        sD.Format(L"%.0f", thicknessMm);
        sT.Format(L"%.1f", tMediaC);
        if (CElement* pEl = pEnt->getRootElement())
        {
            pEl->SetParameter(VCSSTR_PP_INSULATION_THICKNESS, sD, L"Tolshchina", L"mm");
            pEl->SetParameter(VCSSTR_PP_INSULATION_DISABLEVIEW, L"0", L"", L"");
            pEl->SetParameter(L"TEMPERATURE", sT, L"Temperatura", L"C");
            pEl->SetParameter(L"REGION", regionName, L"Region", L"");
            pEl->SetParameter(L"INSUL_MEDIA", mediaName, L"Sreda", L"");
        }
        pEnt->updateElements(true);
        pEnt->close();
    }

    struct ChainPipe { AcDbObjectId id; double pos; };
    struct ChainWeld { AcDbObjectId id; double pos; };

    bool GetSegStartPoint(vCSSegment2* pSeg, AcGePoint3d& pt)
    {
        if (!pSeg) return false;
        AcDbObjectIdArray ss;
        pSeg->SSeg_GetArrSorted(ss);
        if (ss.length() == 0) return false;
        vCSSubSegment* pSS = nullptr;
        if (acdbOpenObject(pSS, ss.at(0), AcDb::kForRead) != Acad::eOk || !pSS) return false;
        pt = pSS->GetPt1();
        pSS->close();
        return true;
    }
}

CInsulDlg* CInsulDlg::s_pInstance = nullptr;

CInsulDlg::CInsulDlg(AcDbObjectId idAxis, InsulDlgMode mode, CWnd* pParent)
    : CDialog(IDD_INSUL, pParent)
    , m_mode(mode)
    , m_idAxis(idAxis)
{
}

bool CInsulDlg::ShowModeless(AcDbObjectId idAxis, InsulDlgMode mode, CWnd* pParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    if (s_pInstance)
    {
        s_pInstance->ClearHighlight();
        s_pInstance->DestroyWindow();
        s_pInstance = nullptr;
    }
    CInsulDlg* pDlg = new CInsulDlg(idAxis, mode, pParent);
    if (!pDlg->Create(IDD_INSUL, pParent))
    {
        delete pDlg;
        return false;
    }
    s_pInstance = pDlg;
    pDlg->ShowWindow(SW_SHOW);
    pDlg->SetActiveWindow();
    return true;
}

void CInsulDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CInsulDlg, CDialog)
    ON_BN_CLICKED(IDC_INSUL_BTN_CALC, &CInsulDlg::OnBnCalculate)
    ON_BN_CLICKED(IDC_INSUL_BTN_ASSIGN, &CInsulDlg::OnBnAssign)
    ON_CBN_SELCHANGE(IDC_INSUL_CMB_MEDIA, &CInsulDlg::OnCbnChange)
    ON_NOTIFY(TCN_SELCHANGE, IDC_INSUL_TAB, &CInsulDlg::OnTabChange)
    ON_BN_CLICKED(IDC_INSUL_BTN_REFRESH, &CInsulDlg::OnBnRefreshZones)
    ON_BN_CLICKED(IDC_INSUL_BTN_CALC_ALL, &CInsulDlg::OnBnCalcAll)
    ON_BN_CLICKED(IDC_INSUL_BTN_ASSIGN_ALL, &CInsulDlg::OnBnAssignAll)
    ON_BN_CLICKED(IDC_INSUL_BTN_APPLY_ROW, &CInsulDlg::OnBnApplyRow)
    ON_BN_CLICKED(IDC_INSUL_BTN_SEL_ALL, &CInsulDlg::OnBnSelAll)
    ON_BN_CLICKED(IDC_INSUL_BTN_SEL_NONE, &CInsulDlg::OnBnSelNone)
    ON_BN_CLICKED(IDC_INSUL_CHK_SPLIT_WELD, &CInsulDlg::OnBnRefreshZones)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_INSUL_LIST_ZONES, &CInsulDlg::OnZoneItemChanged)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

void CInsulDlg::ClearHighlight()
{
    acedSSSetFirst(nullptr, nullptr);
    m_hlAxis.setNull();
}

void CInsulDlg::HighlightPipes(const AcDbObjectIdArray& pipeIds)
{
    ClearHighlight();
    if (pipeIds.length() <= 0) return;

    ads_name ss;
    bool haveSs = false;
    for (int i = 0; i < pipeIds.length(); ++i)
    {
        ads_name en;
        if (acdbGetAdsName(en, pipeIds.at(i)) != Acad::eOk)
            continue;
        if (!haveSs)
        {
            if (acedSSAdd(en, nullptr, ss) == RTNORM)
                haveSs = true;
        }
        else
        {
            acedSSAdd(en, ss, ss);
        }
    }
    if (haveSs)
    {
        acedSSSetFirst(ss, nullptr);
        acedSSFree(ss);
    }
}

void CInsulDlg::HighlightAxis(AcDbObjectId idAxis)
{
    if (idAxis.isNull())
    {
        ClearHighlight();
        return;
    }
    AcDbObjectIdArray pipes;
    CollectAxisSegments(idAxis, pipes);

    // also pick visible segments of the axis
    vCSAxis* pAx = nullptr;
    if (acdbOpenObject(pAx, idAxis, AcDb::kForRead) == Acad::eOk && pAx)
    {
        const int n = pAx->GetSegCount();
        for (int i = 0; i < n; ++i)
        {
            const AcDbObjectId idSeg = pAx->GetSeg(i);
            if (!idSeg.isNull())
            {
                int pos = -1;
                if (!pipes.find(idSeg, pos))
                    pipes.append(idSeg);
            }
        }
        pAx->close();
    }

    HighlightPipes(pipes);
    m_hlAxis = idAxis;
}

void CInsulDlg::OnOK()
{
    // modeless: ignore Enter as OK
}

void CInsulDlg::OnCancel()
{
    ClearHighlight();
    DestroyWindow();
}

void CInsulDlg::OnClose()
{
    ClearHighlight();
    DestroyWindow();
}

void CInsulDlg::PostNcDestroy()
{
    ClearHighlight();
    CDialog::PostNcDestroy();
    if (s_pInstance == this)
        s_pInstance = nullptr;
    delete this;
}

void CInsulDlg::FillRegionMediaCombos()
{
    auto fillReg = [&](int idc)
    {
        CComboBox* p = (CComboBox*)GetDlgItem(idc);
        if (!p) return;
        p->ResetContent();
        for (int i = 0; i < kRegionCount; ++i)
            p->AddString(kRegionsRu[i]);
        p->SetCurSel(0);
    };
    auto fillMed = [&](int idc)
    {
        CComboBox* p = (CComboBox*)GetDlgItem(idc);
        if (!p) return;
        p->ResetContent();
        p->AddString(L"Вода (котловая / теплосеть)");
        p->AddString(L"Пар");
        p->SetCurSel(0);
    };
    fillReg(IDC_INSUL_CMB_REGION);
    fillReg(IDC_INSUL_CMB_REGION2);
    fillMed(IDC_INSUL_CMB_MEDIA);
    fillMed(IDC_INSUL_CMB_MEDIA2);
}

void CInsulDlg::ShowTab(int tab)
{
    m_curTab = tab;
    const bool t0 = (tab == 0);
    const int tab0[] = {
        IDC_INSUL_CMB_REGION, IDC_INSUL_CMB_MEDIA, IDC_INSUL_EDIT_T,
        IDC_INSUL_STATIC_D, IDC_INSUL_BTN_CALC, IDC_INSUL_BTN_ASSIGN,
        IDC_INSUL_STATIC_RESULT, IDC_INSUL_EDIT_REPORT
    };
    const int tab1[] = {
        IDC_INSUL_LIST_ZONES, IDC_INSUL_CHK_SPLIT_WELD,
        IDC_INSUL_CMB_REGION2, IDC_INSUL_CMB_MEDIA2, IDC_INSUL_EDIT_T2,
        IDC_INSUL_BTN_APPLY_ROW, IDC_INSUL_BTN_REFRESH,
        IDC_INSUL_BTN_SEL_ALL, IDC_INSUL_BTN_SEL_NONE,
        IDC_INSUL_BTN_CALC_ALL, IDC_INSUL_BTN_ASSIGN_ALL, IDC_INSUL_STATIC_BATCH
    };
    for (int id : tab0)
        if (CWnd* w = GetDlgItem(id)) w->ShowWindow(t0 ? SW_SHOW : SW_HIDE);
    // groupbox "Polnyy raschet" is IDC_STATIC near report - hide siblings carefully
    for (int id : tab1)
        if (CWnd* w = GetDlgItem(id)) w->ShowWindow(t0 ? SW_HIDE : SW_SHOW);
}

BOOL CInsulDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    if (!m_idAxis.isNull())
        ReadDiameters(m_idAxis, m_dOuterMm, m_dnMm);

    m_tab.SubclassDlgItem(IDC_INSUL_TAB, this);
    m_tab.InsertItem(0, L"Текущая ось");
    m_tab.InsertItem(1, L"Все оси / зоны");

    m_listZones.SubclassDlgItem(IDC_INSUL_LIST_ZONES, this);
    m_listZones.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_GRIDLINES);
    m_listZones.InsertColumn(0, L"Ось", LVCFMT_LEFT, 140);
    m_listZones.InsertColumn(1, L"Зона", LVCFMT_LEFT, 150);
    m_listZones.InsertColumn(2, L"Dnar", LVCFMT_RIGHT, 55);
    m_listZones.InsertColumn(3, L"Среда", LVCFMT_LEFT, 55);
    m_listZones.InsertColumn(4, L"t C", LVCFMT_RIGHT, 50);
    m_listZones.InsertColumn(5, L"delta мм", LVCFMT_RIGHT, 60);
    m_listZones.InsertColumn(6, L"труб", LVCFMT_RIGHT, 45);

    FillRegionMediaCombos();
    SetDlgItemText(IDC_INSUL_EDIT_T, L"90");
    SetDlgItemText(IDC_INSUL_EDIT_T2, L"90");
    if (CButton* b = (CButton*)GetDlgItem(IDC_INSUL_CHK_SPLIT_WELD))
        b->SetCheck(BST_CHECKED);

    CString s;
    s.Format(L"Dnar=%.1f mm, DN=%.1f | NR 34, pomeshchenie", m_dOuterMm, m_dnMm);
    SetDlgItemText(IDC_INSUL_STATIC_D, s);
    SetDlgItemText(IDC_INSUL_EDIT_REPORT,
        L"Вкладка 1: расчёт текущей оси.\r\n"
        L"Вкладка 2: все осевые, t/среда по зонам.\r\n"
        L"Разрез по сварному шву (Weld): до/после — разные t.");
    if (CWnd* b = GetDlgItem(IDC_INSUL_BTN_ASSIGN))
        b->EnableWindow(FALSE);

    if (m_mode == InsulDlgMode::AllAxes)
    {
        SetWindowText(L"PIPE_INSUL_ALL - vse osi (modeless)");
        m_tab.SetCurSel(1);
        ShowTab(1);
        if (CWnd* w = GetDlgItem(IDC_INSUL_TAB)) w->EnableWindow(FALSE);
        RebuildZones();
        if (!m_zones.empty())
            HighlightAxis(m_zones[0].idAxis);
    }
    else
    {
        SetWindowText(L"PIPE_INSUL - odna os (modeless)");
        m_tab.SetCurSel(0);
        ShowTab(0);
        if (CWnd* w = GetDlgItem(IDC_INSUL_TAB)) w->EnableWindow(FALSE);
        if (!m_idAxis.isNull())
            HighlightAxis(m_idAxis);
    }
    return TRUE;
}

void CInsulDlg::OnTabChange(NMHDR*, LRESULT* pResult)
{
    // mode locks the tab; keep selection in sync
    const int want = (m_mode == InsulDlgMode::AllAxes) ? 1 : 0;
    if (m_tab.GetCurSel() != want)
        m_tab.SetCurSel(want);
    ShowTab(want);
    *pResult = 0;
}

void CInsulDlg::OnCbnChange()
{
    CComboBox* pMed = (CComboBox*)GetDlgItem(IDC_INSUL_CMB_MEDIA);
    if (!pMed) return;
    SetDlgItemText(IDC_INSUL_EDIT_T, pMed->GetCurSel() == 1 ? L"250" : L"90");
}

void CInsulDlg::ReadDiameters(AcDbObjectId idAxis, double& dOuter, double& dn) const
{
    dOuter = 108.0; dn = 100.0;
    vCSAxis* pAx = nullptr;
    if (acdbOpenObject(pAx, idAxis, AcDb::kForRead) != Acad::eOk || !pAx) return;
    if (CParam* p = pAx->GetParameter(L"Diameter"))
    {
        const double v = _wtof(p->getValue());
        if (v > 1.0) { dOuter = v; dn = v; }
    }
    if (CParam* p = pAx->GetParameter(L"PART_PIPE_DIAMETER"))
    {
        const double v = _wtof(p->getValue());
        if (v > 1.0) dOuter = v;
    }
    if (CParam* p = pAx->GetParameter(L"DN"))
    {
        const double v = _wtof(p->getValue());
        if (v > 1.0) dn = v;
    }
    if (CParam* p = pAx->GetParameter(L"PART_PIPE_DN"))
    {
        const double v = _wtof(p->getValue());
        if (v > 1.0) dn = v;
    }
    pAx->close();
}

CString CInsulDlg::AxisDisplayName(AcDbObjectId idAxis) const
{
    CString name = L"Ось";
    vCSAxis* pAx = nullptr;
    if (acdbOpenObject(pAx, idAxis, AcDb::kForRead) == Acad::eOk && pAx)
    {
        if (LPCTSTR n = pAx->GetNameAxis())
        {
            if (n[0]) name = n;
        }
        if (CParam* p = pAx->GetParameter(L"LINE_NUMBER"))
        {
            CString ln = p->getValue();
            if (!ln.IsEmpty()) name.Format(L"%s [%s]", static_cast<LPCWSTR>(name), static_cast<LPCWSTR>(ln));
        }
        pAx->close();
    }
    CString handle;
    handle.Format(L" #%lu", (unsigned long)idAxis.asOldId());
    name += handle;
    return name;
}

void CInsulDlg::BuildZonesForAxis(AcDbObjectId idAxis, bool splitByWeld, std::vector<InsulZone>& out) const
{
    double dOuter = 108, dn = 100;
    ReadDiameters(idAxis, dOuter, dn);
    const CString axisName = AxisDisplayName(idAxis);

    std::vector<ChainPipe> pipes;
    std::vector<ChainWeld> welds;

    vCSAxis* pAx = nullptr;
    if (acdbOpenObject(pAx, idAxis, AcDb::kForRead) != Acad::eOk || !pAx)
        return;

    const int nSeg = pAx->GetSegCount();
    for (int si = 0; si < nSeg; ++si)
    {
        const AcDbObjectId idSeg = pAx->GetSeg(si);
        if (idSeg.isNull()) continue;
        AcDbObject* pObj = nullptr;
        if (acdbOpenObject(pObj, idSeg, AcDb::kForRead) != Acad::eOk || !pObj)
            continue;

        vCSSegment2* pSeg2 = vCSSegment2::cast(pObj);
        vCSSegment* pSeg1 = pSeg2 ? nullptr : vCSSegment::cast(pObj);

        AcDbObjectIdArray ss;
        AcDbObjectIdArray ww;
        AcGeDoubleArray wdist;
        if (pSeg2)
        {
            pSeg2->SSeg_GetArrSorted(ss);
            pSeg2->Weld_GetArr(ww);
            pSeg2->Weld_Dist(wdist);
        }
        else if (pSeg1)
        {
            // old vCSSegment: only subsegments, no Weld_* API
            pSeg1->SSeg_GetArr(ss);
        }

        for (int i = 0; i < ss.length(); ++i)
        {
            vCSSubSegment* pSS = nullptr;
            if (acdbOpenObject(pSS, ss.at(i), AcDb::kForRead) != Acad::eOk || !pSS)
                continue;
            const AcGePoint3d mid = pSS->GetPt1() + (pSS->GetPt2() - pSS->GetPt1()) * 0.5;
            // local param along first subseg of segment as fallback index
            double local = (double)i;
            // Prefer distance from segment start if available
            AcGePoint3d s0 = pSS->GetPt1();
            if (i == 0) local = 0.0;
            else
            {
                vCSSubSegment* p0 = nullptr;
                if (acdbOpenObject(p0, ss.at(0), AcDb::kForRead) == Acad::eOk && p0)
                {
                    local = p0->GetPt1().distanceTo(mid);
                    p0->close();
                }
            }
            ChainPipe cp;
            cp.id = ss.at(i);
            cp.pos = si * 1e9 + local;
            pipes.push_back(cp);
            pSS->close();
            (void)s0;
        }

        const int nw = (std::min)(ww.length(), wdist.length());
        for (int i = 0; i < nw; ++i)
        {
            ChainWeld cw;
            cw.id = ww.at(i);
            cw.pos = si * 1e9 + wdist.at(i);
            welds.push_back(cw);
        }
        // welds without dist: use BasePoint order index
        for (int i = nw; i < ww.length(); ++i)
        {
            ChainWeld cw;
            cw.id = ww.at(i);
            cw.pos = si * 1e9 + 1e6 + i;
            welds.push_back(cw);
        }

        pObj->close();
    }
    pAx->close();

    std::sort(pipes.begin(), pipes.end(), [](const ChainPipe& a, const ChainPipe& b) { return a.pos < b.pos; });
    std::sort(welds.begin(), welds.end(), [](const ChainWeld& a, const ChainWeld& b) { return a.pos < b.pos; });

    if (pipes.empty())
        return;

    if (!splitByWeld || welds.empty())
    {
        InsulZone z;
        z.idAxis = idAxis;
        z.axisName = axisName;
        z.zoneName = welds.empty() ? L"вся ось" : L"вся ось (без разреза)";
        z.zoneIndex = 0;
        z.zoneCount = 1;
        z.dOuterMm = dOuter;
        z.dnMm = dn;
        z.splitByWeld = false;
        for (const auto& p : pipes) z.pipeIds.append(p.id);
        out.push_back(z);
        return;
    }

    // N welds -> N+1 zones
    const int nZones = (int)welds.size() + 1;
    std::vector<InsulZone> zones(nZones);
    for (int zi = 0; zi < nZones; ++zi)
    {
        zones[zi].idAxis = idAxis;
        zones[zi].axisName = axisName;
        zones[zi].zoneIndex = zi;
        zones[zi].zoneCount = nZones;
        zones[zi].dOuterMm = dOuter;
        zones[zi].dnMm = dn;
        zones[zi].splitByWeld = true;
        if (zi == 0)
            zones[zi].zoneName.Format(L"до шва 1 / %d", nZones);
        else if (zi == nZones - 1)
            zones[zi].zoneName.Format(L"после шва %d / %d", (int)welds.size(), nZones);
        else
            zones[zi].zoneName.Format(L"между швами %d-%d", zi, zi + 1);
    }

    size_t wi = 0;
    for (const auto& p : pipes)
    {
        while (wi < welds.size() && welds[wi].pos <= p.pos)
            ++wi;
        const int zi = (int)wi; // pipes before first weld -> 0; after k welds passed -> k
        if (zi >= 0 && zi < nZones)
            zones[zi].pipeIds.append(p.id);
    }

    for (auto& z : zones)
    {
        if (z.pipeIds.length() > 0)
            out.push_back(z);
    }
}

void CInsulDlg::RebuildZones()
{
    m_zones.clear();
    const bool split = (IsDlgButtonChecked(IDC_INSUL_CHK_SPLIT_WELD) == BST_CHECKED);

    AcDbObjectIdArray axes;
    if (m_mode == InsulDlgMode::SingleAxis)
    {
        if (!m_idAxis.isNull())
            axes.append(m_idAxis);
    }
    else
    {
        CollectAllAxes(axes);
        if (!m_idAxis.isNull())
        {
            int pos = -1;
            if (!axes.find(m_idAxis, pos))
                axes.insertAt(0, m_idAxis);
        }
    }

    for (int i = 0; i < axes.length(); ++i)
        BuildZonesForAxis(axes.at(i), split, m_zones);

    // default t/media from tab2 controls
    CString tStr;
    GetDlgItemText(IDC_INSUL_EDIT_T2, tStr);
    double t = _wtof(tStr);
    if (t < -50 || t > 700) t = 90;
    CComboBox* pMed = (CComboBox*)GetDlgItem(IDC_INSUL_CMB_MEDIA2);
    const int media = pMed ? pMed->GetCurSel() : 0;
    for (auto& z : m_zones)
    {
        z.tMediaC = t;
        z.mediaIdx = (media < 0) ? 0 : media;
        z.selected = true;
    }

    FillZoneList();
    CString msg;
    msg.Format(L"Осей/зон: %d (разрез по Weld: %s)", (int)m_zones.size(), split ? L"??" : L"???");
    SetDlgItemText(IDC_INSUL_STATIC_BATCH, msg);
}

void CInsulDlg::FillZoneList()
{
    m_listZones.DeleteAllItems();
    for (int i = 0; i < (int)m_zones.size(); ++i)
    {
        const InsulZone& z = m_zones[i];
        const int row = m_listZones.InsertItem(i, z.axisName);
        m_listZones.SetItemText(row, 1, z.zoneName);
        CString s;
        s.Format(L"%.0f", z.dOuterMm); m_listZones.SetItemText(row, 2, s);
        m_listZones.SetItemText(row, 3, z.mediaIdx == 1 ? L"Пар" : L"Вода");
        s.Format(L"%.1f", z.tMediaC); m_listZones.SetItemText(row, 4, s);
        if (z.calculated) s.Format(L"%.0f", z.deltaMm); else s = L"-";
        m_listZones.SetItemText(row, 5, s);
        s.Format(L"%d", z.pipeIds.length()); m_listZones.SetItemText(row, 6, s);
        m_listZones.SetItemData(row, (DWORD_PTR)i);
        m_listZones.SetCheck(row, z.selected ? TRUE : FALSE);
    }
}

void CInsulDlg::SyncZoneFromListChecks()
{
    for (int i = 0; i < m_listZones.GetItemCount(); ++i)
    {
        const int zi = (int)m_listZones.GetItemData(i);
        if (zi >= 0 && zi < (int)m_zones.size())
            m_zones[zi].selected = m_listZones.GetCheck(i) ? true : false;
    }
}

void CInsulDlg::OnZoneItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW p = (LPNMLISTVIEW)pNMHDR;
    if (p && (p->uChanged & LVIF_STATE) && p->iItem >= 0)
    {
        const bool becameSelected =
            (p->uNewState & LVIS_SELECTED) && !(p->uOldState & LVIS_SELECTED);
        const int zi = (int)m_listZones.GetItemData(p->iItem);
        if (zi >= 0 && zi < (int)m_zones.size())
        {
            CString s;
            s.Format(L"%.1f", m_zones[zi].tMediaC);
            SetDlgItemText(IDC_INSUL_EDIT_T2, s);
            if (CComboBox* pMed = (CComboBox*)GetDlgItem(IDC_INSUL_CMB_MEDIA2))
                pMed->SetCurSel(m_zones[zi].mediaIdx);

            if (becameSelected)
            {
                // highlight zone pipes (or whole axis if no split)
                if (m_zones[zi].pipeIds.length() > 0)
                    HighlightPipes(m_zones[zi].pipeIds);
                else
                    HighlightAxis(m_zones[zi].idAxis);
                m_hlAxis = m_zones[zi].idAxis;
            }
        }
    }
    *pResult = 0;
}

void CInsulDlg::OnBnRefreshZones()
{
    RebuildZones();
}

void CInsulDlg::OnBnSelAll()
{
    for (int i = 0; i < m_listZones.GetItemCount(); ++i)
        m_listZones.SetCheck(i, TRUE);
}

void CInsulDlg::OnBnSelNone()
{
    for (int i = 0; i < m_listZones.GetItemCount(); ++i)
        m_listZones.SetCheck(i, FALSE);
}

void CInsulDlg::OnBnApplyRow()
{
    SyncZoneFromListChecks();
    CString tStr;
    GetDlgItemText(IDC_INSUL_EDIT_T2, tStr);
    double t = _wtof(tStr);
    if (t < -50 || t > 700)
    {
        AfxMessageBox(L"Проверьте данные.", MB_ICONWARNING);
        return;
    }
    CComboBox* pMed = (CComboBox*)GetDlgItem(IDC_INSUL_CMB_MEDIA2);
    const int media = pMed ? pMed->GetCurSel() : 0;
    int n = 0;
    for (auto& z : m_zones)
    {
        if (!z.selected) continue;
        z.tMediaC = t;
        z.mediaIdx = (media < 0) ? 0 : media;
        z.calculated = false;
        ++n;
    }
    FillZoneList();
    CString msg;
    msg.Format(L"Применено t=%.1f к %d зонам. Пересчитайте.", t, n);
    SetDlgItemText(IDC_INSUL_STATIC_BATCH, msg);
}

void CInsulDlg::OnBnCalcAll()
{
    SyncZoneFromListChecks();
    CComboBox* pReg = (CComboBox*)GetDlgItem(IDC_INSUL_CMB_REGION2);
    int iReg = pReg ? pReg->GetCurSel() : 0;
    if (iReg < 0 || iReg >= kRegionCount) iReg = 0;

    int nOk = 0;
    for (auto& z : m_zones)
    {
        if (!z.selected) continue;
        InsulCalc::Input in{};
        in.norm = InsulCalc::Norm::Nr34;
        in.loc = InsulCalc::Location::Indoor;
        in.dOuterMm = z.dOuterMm;
        in.dnMm = z.dnMm;
        in.tMediaC = z.tMediaC;
        in.tAirC = 25.0;
        in.alpha = 10.0;
        in.kLoss = (z.dnMm < 150.0) ? 1.20 : 1.15;
        in.lambda = 0.049;
        in.hoursOver5000 = true;
        in.regionK = kRegions[iReg].k;
        const InsulCalc::Result r = InsulCalc::Calculate(in);
        if (r.ok)
        {
            z.deltaMm = r.deltaMm;
            z.calculated = true;
            ++nOk;
        }
        else
        {
            z.calculated = false;
            z.deltaMm = 0;
        }
    }
    FillZoneList();
    CString msg;
    msg.Format(L"Рассчитано зон: %d", nOk);
    SetDlgItemText(IDC_INSUL_STATIC_BATCH, msg);
}

void CInsulDlg::OnBnAssignAll()
{
    SyncZoneFromListChecks();
    CComboBox* pReg = (CComboBox*)GetDlgItem(IDC_INSUL_CMB_REGION2);
    int iReg = pReg ? pReg->GetCurSel() : 0;
    if (iReg < 0 || iReg >= kRegionCount) iReg = 0;
    const CString regionName = kRegionsRu[iReg];

    // count zones per axis for writeAxisLevel decision
    std::map<Adesk::IntDbId, int> axisZones;
    for (const auto& z : m_zones)
    {
        if (!z.selected || !z.calculated) continue;
        axisZones[z.idAxis.asOldId()]++;
    }

    int nAssign = 0;
    for (auto& z : m_zones)
    {
        if (!z.selected || !z.calculated || z.deltaMm <= 0) continue;
        const CString media = (z.mediaIdx == 1) ? L"Пар" : L"Вода";
        const bool singleZoneOnAxis = (axisZones[z.idAxis.asOldId()] <= 1);
        if (AssignToPipes(z.idAxis, z.pipeIds, z.deltaMm, z.tMediaC, regionName, media, singleZoneOnAxis))
            ++nAssign;
    }

    CString msg;
    msg.Format(L"Назначено зон: %d", nAssign);
    SetDlgItemText(IDC_INSUL_STATIC_BATCH, msg);
    AfxMessageBox(msg, MB_ICONINFORMATION);
}

bool CInsulDlg::ReadUi(InsulCalc::Input& in, CString& mediaName, CString& regionName) const
{
    CComboBox* pReg = (CComboBox*)GetDlgItem(IDC_INSUL_CMB_REGION);
    CComboBox* pMed = (CComboBox*)GetDlgItem(IDC_INSUL_CMB_MEDIA);
    int iReg = pReg ? pReg->GetCurSel() : 0;
    int iMed = pMed ? pMed->GetCurSel() : 0;
    if (iReg < 0 || iReg >= kRegionCount) iReg = 0;
    if (iMed < 0) iMed = 0;
    CString tStr;
    GetDlgItemText(IDC_INSUL_EDIT_T, tStr);
    const double t = _wtof(tStr);
    if (t < -50.0 || t > 700.0) return false;
    regionName = kRegionsRu[iReg];
    mediaName = (iMed == 1) ? L"Пар" : L"Вода";
    in = InsulCalc::Input{};
    in.norm = InsulCalc::Norm::Nr34;
    in.loc = InsulCalc::Location::Indoor;
    in.dOuterMm = m_dOuterMm;
    in.dnMm = m_dnMm;
    in.tMediaC = t;
    in.tAirC = 25.0;
    in.alpha = 10.0;
    in.kLoss = (m_dnMm < 150.0) ? 1.20 : 1.15;
    in.lambda = 0.049;
    in.hoursOver5000 = true;
    in.regionK = kRegions[iReg].k;
    return true;
}

void CInsulDlg::ShowResult(const InsulCalc::Input& in, const InsulCalc::Result& r,
    const CString& mediaName, const CString& regionName)
{
    CString ru;
    ru.Format(
        L"=== RASCHET (KOTLY, NR 34) ===\r\n"
        L"Region: %s (K=%.2f)\r\n"
        L"Sreda: %s, t=%.1f C | Dnar=%.1f DN=%.1f | Kop=%0.2f\r\n"
        L"q=%.2f | dq=%.1f | doz=%.1f | crit=%s\r\n"
        L"ITOGO delta = %.0f mm\r\n",
        static_cast<LPCWSTR>(regionName), in.regionK,
        static_cast<LPCWSTR>(mediaName), in.tMediaC, in.dOuterMm, in.dnMm, in.kLoss,
        r.qNormWm, r.deltaQMm, r.deltaTsMm, r.criterion,
        r.deltaMm);
    SetDlgItemText(IDC_INSUL_EDIT_REPORT, ru);
    CString big;
    big.Format(L"Itogovaya tolshchina: %.0f mm", r.deltaMm);
    SetDlgItemText(IDC_INSUL_STATIC_RESULT, big);
}

void CInsulDlg::OnBnCalculate()
{
    InsulCalc::Input in;
    CString mediaName, regionName;
    if (!ReadUi(in, mediaName, regionName))
    {
        AfxMessageBox(L"Проверьте данные.", MB_ICONWARNING);
        return;
    }
    m_last = InsulCalc::Calculate(in);
    m_hasResult = m_last.ok;
    m_lastT = in.tMediaC;
    m_lastRegion = regionName;
    m_lastMedia = mediaName;
    ShowResult(in, m_last, mediaName, regionName);
    if (CWnd* b = GetDlgItem(IDC_INSUL_BTN_ASSIGN))
        b->EnableWindow(m_last.ok ? TRUE : FALSE);
}

bool CInsulDlg::CollectAxisSegments(AcDbObjectIdArray& pipeIds) const
{
    return CollectAxisSegments(m_idAxis, pipeIds);
}

bool CInsulDlg::CollectAxisSegments(AcDbObjectId idAxis, AcDbObjectIdArray& pipeIds) const
{
    pipeIds.removeAll();
    if (idAxis.isNull()) return false;

    vCSAxis* pAx = nullptr;
    if (acdbOpenObject(pAx, idAxis, AcDb::kForRead) == Acad::eOk && pAx)
    {
        const int n = pAx->GetSegCount();
        for (int i = 0; i < n; ++i)
        {
            const AcDbObjectId idSeg = pAx->GetSeg(i);
            if (idSeg.isNull()) continue;
            AcDbObject* pObj = nullptr;
            if (acdbOpenObject(pObj, idSeg, AcDb::kForRead) != Acad::eOk || !pObj) continue;
            if (pObj->isKindOf(vCSSegment2::desc()))
            {
                vCSSegment2* pSeg = vCSSegment2::cast(pObj);
                AcDbObjectIdArray ss;
                if (pSeg) { pSeg->SSeg_GetArr(ss); for (int j = 0; j < ss.length(); ++j) AppendUnique(pipeIds, ss.at(j)); }
            }
            else if (pObj->isKindOf(vCSSegment::desc()))
            {
                vCSSegment* pSeg = vCSSegment::cast(pObj);
                AcDbObjectIdArray ss;
                if (pSeg) { pSeg->SSeg_GetArr(ss); for (int j = 0; j < ss.length(); ++j) AppendUnique(pipeIds, ss.at(j)); }
            }
            pObj->close();
        }
        pAx->close();
    }

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (pDb)
    {
        AcDbBlockTable* pBT = nullptr;
        if (pDb->getBlockTable(pBT, AcDb::kForRead) == Acad::eOk && pBT)
        {
            AcDbBlockTableRecord* pMS = nullptr;
            if (pBT->getAt(ACDB_MODEL_SPACE, pMS, AcDb::kForRead) == Acad::eOk && pMS)
            {
                AcDbBlockTableRecordIterator* pIt = nullptr;
                if (pMS->newIterator(pIt) == Acad::eOk && pIt)
                {
                    for (; !pIt->done(); pIt->step())
                    {
                        AcDbObjectId id;
                        pIt->getEntityId(id);
                        AcDbObject* pObj = nullptr;
                        if (acdbOpenObject(pObj, id, AcDb::kForRead) != Acad::eOk || !pObj) continue;
                        if (pObj->isKindOf(vCSSubSegment::desc()))
                        {
                            vCSSubSegment* pSS = vCSSubSegment::cast(pObj);
                            if (pSS && pSS->GetOIdAxis() == idAxis)
                                AppendUnique(pipeIds, id);
                        }
                        pObj->close();
                    }
                    delete pIt;
                }
                pMS->close();
            }
            pBT->close();
        }
    }
    return pipeIds.length() > 0;
}

void CInsulDlg::WriteAxisParams(double thicknessMm, double tMediaC,
    const CString& regionName, const CString& mediaName)
{
    WriteAxisParams(m_idAxis, thicknessMm, tMediaC, regionName, mediaName);
}

void CInsulDlg::WriteAxisParams(AcDbObjectId idAxis, double thicknessMm, double tMediaC,
    const CString& regionName, const CString& mediaName)
{
    vCSAxis* pAx = nullptr;
    if (acdbOpenObject(pAx, idAxis, AcDb::kForWrite) != Acad::eOk || !pAx)
        return;
    pAx->SetInsulationThickness(thicknessMm);
    CString sT, sD;
    sT.Format(L"%.1f", tMediaC);
    sD.Format(L"%.0f", thicknessMm);
    pAx->SetParameter(VCSSTR_PP_INSULATION_THICKNESS, sD, L"Tolshchina", L"mm");
    pAx->SetParameter(VCSSTR_PP_INSULATION_DISABLEVIEW, L"0", L"", L"");
    pAx->SetParameter(L"TEMPERATURE", sT, L"Temperatura", L"C");
    pAx->SetParameter(L"REGION", regionName, L"Region", L"");
    pAx->SetParameter(L"INSUL_MEDIA", mediaName, L"Sreda", L"");
    pAx->SetParameter(L"INSUL_NORM", L"NR 34-70-118-87", L"Norma", L"");
    pAx->SetCElPropertyForChildren(VCSSTR_PP_INSULATION_THICKNESS, sD);
    pAx->SetCElPropertyForChildren(VCSSTR_PP_INSULATION_DISABLEVIEW, L"0");
    pAx->close();
}

bool CInsulDlg::AssignToPipes(AcDbObjectId idAxis, const AcDbObjectIdArray& pipeIds,
    double thicknessMm, double tMediaC, const CString& regionName, const CString& mediaName,
    bool writeAxisLevel)
{
    if (writeAxisLevel)
        WriteAxisParams(idAxis, thicknessMm, tMediaC, regionName, mediaName);
    else if (!idAxis.isNull())
    {
        // region on axis anyway; thickness/temp per pipes
        vCSAxis* pAx = nullptr;
        if (acdbOpenObject(pAx, idAxis, AcDb::kForWrite) == Acad::eOk && pAx)
        {
            pAx->SetParameter(L"REGION", regionName, L"Region", L"");
            pAx->close();
        }
    }

    for (int i = 0; i < pipeIds.length(); ++i)
        SetPartInsulationOnEntity(pipeIds.at(i), thicknessMm, tMediaC, regionName, mediaName);

    // MultiPipeInsulation only from subsegments
    AcDbObjectIdArray onlySS;
    for (int i = 0; i < pipeIds.length(); ++i)
    {
        AcDbObject* pObj = nullptr;
        if (acdbOpenObject(pObj, pipeIds.at(i), AcDb::kForRead) != Acad::eOk || !pObj) continue;
        const bool isSS = pObj->isKindOf(vCSSubSegment::desc());
        pObj->close();
        if (isSS) onlySS.append(pipeIds.at(i));
    }
    if (onlySS.length() > 0)
    {
        MultiPipeInsulation* pIns = new MultiPipeInsulation();
        if (pIns)
        {
            pIns->setFlagFull(false);
            if (pIns->SetPipeArray(onlySS) == eMpiErrOk &&
                pIns->ConstructFromPipes(thicknessMm, true) == eMpiErrOk &&
                AppendToModelSpace(pIns) == Acad::eOk)
            {
                pIns->close();
            }
            else
            {
                pIns->close();
                delete pIns;
            }
        }
    }
    return pipeIds.length() > 0;
}

bool CInsulDlg::AssignInsulation(double thicknessMm, double tMediaC,
    const CString& regionName, const CString& mediaName)
{
    AcDbObjectIdArray pipeIds;
    CollectAxisSegments(m_idAxis, pipeIds);
    return AssignToPipes(m_idAxis, pipeIds, thicknessMm, tMediaC, regionName, mediaName, true);
}

void CInsulDlg::OnBnAssign()
{
    if (!m_hasResult || !m_last.ok)
    {
        AfxMessageBox(L"Проверьте данные.", MB_ICONWARNING);
        return;
    }
    CString ask;
    ask.Format(L"Назначить изоляцию %.0f мм на текущую ось?", m_last.deltaMm);
    if (AfxMessageBox(ask, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    AssignInsulation(m_last.deltaMm, m_lastT, m_lastRegion, m_lastMedia);
    AfxMessageBox(L"Готово.", MB_ICONINFORMATION);
}

// ----- HelloNRX.cpp -----
#include "aced.h"
#include "rxmfcapi.h"
#include "VCSUtils.h"
#include "vCSTools.h"
#include "vCSEntity.h"
#include "vCSAxis.h"

class CHelloNRXApp : public CWinApp
{
public:
    virtual BOOL InitInstance() override { return TRUE; }
};
CHelloNRXApp theApp;

namespace
{
    AcDbObjectId ResolvePipeAxisId(const AcDbObjectId& idEnt)
    {
        if (idEnt.isNull())
            return AcDbObjectId::kNull;

        AcDbObjectId idAxis = CVCSUtils::getAxis(idEnt);
        if (!idAxis.isNull())
            return idAxis;

        idAxis = vCSTools::GetOIdAxisByViperEnt(idEnt);
        if (!idAxis.isNull())
            return idAxis;

        AcDbObject* pObj = nullptr;
        if (acdbOpenObject(pObj, idEnt, AcDb::kForRead) != Acad::eOk || !pObj)
            return AcDbObjectId::kNull;

        if (pObj->isKindOf(vCSEntity::desc()))
        {
            vCSEntity* pEnt = vCSEntity::cast(pObj);
            if (pEnt)
                idAxis = pEnt->GetOIdAxis();
        }
        else if (pObj->isKindOf(vCSAxis::desc()))
        {
            idAxis = idEnt;
        }
        pObj->close();
        return idAxis;
    }
}

void alignPipeCmd()
{
    PipeAlignCrashLog(L"alignPipeCmd ENTER");

    ads_name en;
    ads_point ptPick;
    PipeAlignCrashLog(L"alignPipeCmd acedEntSel BEFORE");
    const int rcSel = acedEntSel(L"\nPIPE_ALIGN: select pipe: ", en, ptPick);
    {
        wchar_t buf[128];
        swprintf_s(buf, L"alignPipeCmd acedEntSel AFTER rc=%d", rcSel);
        PipeAlignCrashLog(buf);
    }
    if (rcSel != RTNORM)
    {
        PipeAlignCrashLog(L"alignPipeCmd cancelled");
        return;
    }

    AcDbObjectId idEnt;
    if (acdbGetObjectId(idEnt, en) != Acad::eOk || idEnt.isNull())
    {
        PipeAlignCrashLog(L"alignPipeCmd bad object");
        return;
    }
    const AcDbObjectId idAxis = ResolvePipeAxisId(idEnt);
    if (idAxis.isNull())
    {
        PipeAlignCrashLog(L"alignPipeCmd axis not found");
        return;
    }

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle(adsw_acadMainWnd());
    CAlignDlg dlg(idAxis, pParent);
    dlg.DoModal();
}

// One axis: pick pipe, modeless dialog, highlight axis
void pipeInsulCmd()
{
    ads_name en;
    ads_point pt;
    if (acedEntSel(L"\nPIPE_INSUL: select pipe/axis: ", en, pt) != RTNORM)
    {
        acutPrintf(L"\nPIPE_INSUL cancelled.");
        return;
    }
    AcDbObjectId idEnt;
    if (acdbGetObjectId(idEnt, en) != Acad::eOk || idEnt.isNull())
    {
        acutPrintf(L"\nPIPE_INSUL: bad selection.");
        return;
    }
    const AcDbObjectId idAxis = ResolvePipeAxisId(idEnt);
    if (idAxis.isNull())
    {
        acutPrintf(L"\nPIPE_INSUL: axis not found.");
        return;
    }

    CWnd* pParent = CWnd::FromHandle(adsw_acadMainWnd());
    if (!CInsulDlg::ShowModeless(idAxis, InsulDlgMode::SingleAxis, pParent))
        acutPrintf(L"\nPIPE_INSUL: dialog create failed.");
    else
        acutPrintf(L"\nPIPE_INSUL: modeless form open (drawing not blocked).");
}

// All axes: modeless, list + weld zones, highlight on row select
void pipeInsulAllCmd()
{
    CWnd* pParent = CWnd::FromHandle(adsw_acadMainWnd());
    if (!CInsulDlg::ShowModeless(AcDbObjectId::kNull, InsulDlgMode::AllAxes, pParent))
        acutPrintf(L"\nPIPE_INSUL_ALL: dialog create failed.");
    else
        acutPrintf(L"\nPIPE_INSUL_ALL: modeless form open (drawing not blocked).");
}

#if defined(acrxEntryPoint)
#   undef acrxEntryPoint
#endif
#if defined(acrxGetApiVersion)
#   undef acrxGetApiVersion
#endif

#if defined(_WIN64) || defined(_AC64)
#   pragma comment(linker, "/EXPORT:ncrxEntryPoint")
#   pragma comment(linker, "/EXPORT:acrxEntryPoint")
#   pragma comment(linker, "/EXPORT:ncrxGetApiVersion")
#   pragma comment(linker, "/EXPORT:acrxGetApiVersion")
#else
#   pragma comment(linker, "/EXPORT:ncrxEntryPoint=_ncrxEntryPoint@8")
#   pragma comment(linker, "/EXPORT:acrxEntryPoint=_acrxEntryPoint@8")
#   pragma comment(linker, "/EXPORT:ncrxGetApiVersion=_ncrxGetApiVersion@0")
#   pragma comment(linker, "/EXPORT:acrxGetApiVersion=_acrxGetApiVersion@0")
#endif

extern "C" __declspec(dllexport) const ACHAR* __stdcall acrxGetApiVersion()
{
#if defined(ACRX_API_VERSION)
    return _T(ACRX_API_VERSION);
#elif defined(ACRXAPI_REVISION)
    return _T(ACRXAPI_REVISION);
#else
    return _T("1");
#endif
}

extern "C" __declspec(dllexport) const ACHAR* __stdcall ncrxGetApiVersion()
{
    return acrxGetApiVersion();
}

extern "C" __declspec(dllexport) AcRx::AppRetCode __stdcall ncrxEntryPoint(AcRx::AppMsgCode msg, void* appId)
{
    switch (msg)
    {
    case AcRx::kInitAppMsg:
        PipeAlignCrashLog(L"ncrxEntryPoint kInitAppMsg");
        acrxDynamicLinker->unlockApplication(appId);
        acrxDynamicLinker->registerAppMDIAware(appId);
        acedRegCmds->addCommand(L"PIPE_GROUP", L"_PIPE_ALIGN", L"PIPE_ALIGN", ACRX_CMD_MODAL, alignPipeCmd);
        // TRANSPARENT: command returns immediately; modeless dialog keeps drawing interactive
        acedRegCmds->addCommand(L"PIPE_GROUP", L"_PIPE_INSUL", L"PIPE_INSUL",
            ACRX_CMD_TRANSPARENT, pipeInsulCmd);
        acedRegCmds->addCommand(L"PIPE_GROUP", L"_PIPE_INSUL_ALL", L"PIPE_INSUL_ALL",
            ACRX_CMD_TRANSPARENT, pipeInsulAllCmd);
        PipeAlignCrashLog(L"ncrxEntryPoint commands registered");
        acutPrintf(L"\nHelloNRX: PIPE_ALIGN, PIPE_INSUL, PIPE_INSUL_ALL");
        acutPrintf(L"\n  PIPE_INSUL     - one axis (pick) + modeless + highlight");
        acutPrintf(L"\n  PIPE_INSUL_ALL - all axes modeless + highlight on select");
        break;
    case AcRx::kUnloadAppMsg:
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        if (CInsulDlg* p = CInsulDlg::GetInstance())
            p->DestroyWindow();
        acedRegCmds->removeGroup(L"PIPE_GROUP");
        break;
    }
    }
    return AcRx::kRetOK;
}

extern "C" __declspec(dllexport) AcRx::AppRetCode __stdcall acrxEntryPoint(AcRx::AppMsgCode msg, void* appId)
{
    return ncrxEntryPoint(msg, appId);
}

