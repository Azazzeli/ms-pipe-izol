#pragma once

#include "resource.h"
#include "InsulCalc.h"
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
