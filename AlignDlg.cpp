#include "stdafx.h"
#include "AlignDlg.h"
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
