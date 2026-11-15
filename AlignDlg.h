#pragma once

#include "resource.h"
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
