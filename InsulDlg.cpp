#include "stdafx.h"
#include "InsulDlg.h"
#include "aced.h"
#include "dbapserv.h"

#include "vCSAxis.h"
#include "vCSObject.h"
#include "vCSSegment.h"
#include "vCSSubSegment.h"
#include "vCSEntity.h"
#include "vCSNode.h"
#include "vCSInLine.h"
#include "vCSWeld.h"
#include "vCSInsulation.h"
#include "MultiPipeInsulation.h"
#include "ParamsObject.h"
#include "vCSToolsStr.h"

#include <algorithm>
#include <map>
#include <memory>

namespace
{
    // Indoor-only (NR 34): regionK=1, tAir=25 C
    constexpr double kIndoorRegionK = 1.0;
    constexpr double kIndoorTAirC = 25.0;

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

    void CollectAxesFromModelSpace(AcDbDatabase* pDb, AcDbObjectIdArray& axes)
    {
        if (!pDb) return;
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

                if (pObj->isKindOf(vCSAxis::desc()))
                {
                    AppendUnique(axes, id);
                }
                else if (pObj->isKindOf(vCSEntity::desc()))
                {
                    if (vCSEntity* pE = vCSEntity::cast(pObj))
                    {
                        const AcDbObjectId ax = pE->GetOIdAxis();
                        if (!ax.isNull()) AppendUnique(axes, ax);
                    }
                }
                else if (pObj->isKindOf(vCSSubSegment::desc()))
                {
                    if (vCSSubSegment* pSS = vCSSubSegment::cast(pObj))
                    {
                        const AcDbObjectId ax = pSS->GetOIdAxis();
                        if (!ax.isNull()) AppendUnique(axes, ax);
                    }
                }
                pObj->close();
            }
            delete pIt;
        }
        pMS->close();
        pBT->close();
    }

    void CollectAxesFromNodDict(AcDbDatabase* pDb, AcDbObjectIdArray& axes)
    {
        if (!pDb) return;
        AcDbDictionary* pNod = nullptr;
        if (pDb->getNamedObjectsDictionary(pNod, AcDb::kForRead) != Acad::eOk || !pNod)
            return;

        AcDbObjectId idDict;
        // try several possible dict names
        const wchar_t* names[] = { VCSSTR_DICTNAME_AXIS, L"Axis", L"AXIS", L"Axes" };
        for (const wchar_t* nm : names)
        {
            if (pNod->getAt(nm, idDict) != Acad::eOk || idDict.isNull())
                continue;
            AcDbDictionary* pAxisDict = nullptr;
            if (acdbOpenObject(pAxisDict, idDict, AcDb::kForRead) != Acad::eOk || !pAxisDict)
                continue;
            for (std::unique_ptr<AcDbDictionaryIterator> it(pAxisDict->newIterator());
                it && !it->done(); it->next())
            {
                const AcDbObjectId id = it->objectId();
                if (id.isNull()) continue;
                // accept if opens as vCSAxis, or just store id
                AppendUnique(axes, id);
            }
            pAxisDict->close();
            break;
        }
        pNod->close();
    }

    void CollectAllAxes(AcDbObjectIdArray& axes)
    {
        axes.removeAll();
        AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
        if (!pDb)
        {
            acutPrintf(L"\n[PIPE_INSUL_ALL] no working database\n");
            return;
        }

        // 1) MS API
        vCSAxis::GetAllAxis(axes, pDb);
        acutPrintf(L"\n[PIPE_INSUL_ALL] GetAllAxis=%d", axes.length());
        if (axes.length() == 0)
        {
            vCSAxis::GetAllAxisX(axes, pDb);
            acutPrintf(L" GetAllAxisX=%d", axes.length());
        }

        // 2) dictionary helper
        {
            AcDbObjectIdArray tmp;
            vCSObject2::GetAllObjectsX(VCSSTR_DICTNAME_AXIS, tmp, pDb);
            for (int i = 0; i < tmp.length(); ++i)
                AppendUnique(axes, tmp.at(i));
            if (tmp.length() > 0)
                acutPrintf(L" GetAllObjectsX=%d", tmp.length());
        }

        // 3) manual NOD "Axis"
        {
            const int before = axes.length();
            CollectAxesFromNodDict(pDb, axes);
            if (axes.length() > before)
                acutPrintf(L" NOD=%d", axes.length() - before);
        }

        // 4) always merge axes from model space pipes/subsegs
        {
            const int before = axes.length();
            CollectAxesFromModelSpace(pDb, axes);
            if (axes.length() > before)
                acutPrintf(L" ModelSpace+=%d", axes.length() - before);
        }

        acutPrintf(L" TOTAL=%d\n", axes.length());
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

CInsulDlg::CInsulDlg(AcDbObjectId idAxis, InsulDlgMode mode, CWnd* pParent)
    : CDialog(IDD_INSUL, pParent)
    , m_mode(mode)
    , m_idAxis(idAxis)
{
}

INT_PTR CInsulDlg::ShowModal(AcDbObjectId idAxis, InsulDlgMode mode, CWnd* pParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CInsulDlg dlg(idAxis, mode, pParent);
    return dlg.DoModal();
}

void CInsulDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CInsulDlg, CDialog)
    ON_BN_CLICKED(IDC_INSUL_BTN_CALC, &CInsulDlg::OnBnCalculate)
    ON_BN_CLICKED(IDC_INSUL_BTN_ASSIGN, &CInsulDlg::OnBnAssign)
    ON_CBN_SELCHANGE(IDC_INSUL_CMB_MEDIA, &CInsulDlg::OnCbnChange)
    ON_BN_CLICKED(IDC_INSUL_BTN_REFRESH, &CInsulDlg::OnBnRefreshZones)
    ON_BN_CLICKED(IDC_INSUL_BTN_CALC_ALL, &CInsulDlg::OnBnCalcAll)
    ON_BN_CLICKED(IDC_INSUL_BTN_ASSIGN_ALL, &CInsulDlg::OnBnAssignAll)
    ON_BN_CLICKED(IDC_INSUL_BTN_APPLY_ROW, &CInsulDlg::OnBnApplyRow)
    ON_BN_CLICKED(IDC_INSUL_BTN_SEL_ALL, &CInsulDlg::OnBnSelAll)
    ON_BN_CLICKED(IDC_INSUL_BTN_SEL_NONE, &CInsulDlg::OnBnSelNone)
    ON_BN_CLICKED(IDC_INSUL_CHK_SPLIT_WELD, &CInsulDlg::OnBnRefreshZones)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_INSUL_LIST_ZONES, &CInsulDlg::OnZoneItemChanged)
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
    // Keep dialog open; Close button uses IDCANCEL
}

void CInsulDlg::OnCancel()
{
    ClearHighlight();
    CDialog::OnCancel();
}

void CInsulDlg::FillMediaCombos()
{
    auto fillMed = [&](int idc)
    {
        CComboBox* p = (CComboBox*)GetDlgItem(idc);
        if (!p) return;
        p->ResetContent();
        p->AddString(L"\x0412\x043E\x0434\x0430\x0020\x0028\x043A\x043E\x0442\x043B\x043E\x0432\x0430\x044F\x0020\x002F\x0020\x0442\x0435\x043F\x043B\x043E\x0441\x0435\x0442\x044C\x0029");
        p->AddString(L"\x041F\x0430\x0440");
        p->SetCurSel(0);
    };
    fillMed(IDC_INSUL_CMB_MEDIA);
    fillMed(IDC_INSUL_CMB_MEDIA2);
}

void CInsulDlg::ApplyModeLayout()
{
    const bool single = (m_mode == InsulDlgMode::SingleAxis);
    const int singleIds[] = {
        IDC_INSUL_STATIC_LBL_MEDIA, IDC_INSUL_CMB_MEDIA, IDC_INSUL_STATIC_LBL_T, IDC_INSUL_EDIT_T,
        IDC_INSUL_BTN_CALC, IDC_INSUL_BTN_ASSIGN, IDC_INSUL_STATIC_RESULT, IDC_INSUL_EDIT_REPORT
    };
    const int allIds[] = {
        IDC_INSUL_LIST_ZONES, IDC_INSUL_CHK_SPLIT_WELD,
        IDC_INSUL_STATIC_LBL_MEDIA2, IDC_INSUL_CMB_MEDIA2, IDC_INSUL_STATIC_LBL_T2, IDC_INSUL_EDIT_T2,
        IDC_INSUL_BTN_APPLY_ROW, IDC_INSUL_BTN_REFRESH,
        IDC_INSUL_BTN_SEL_ALL, IDC_INSUL_BTN_SEL_NONE,
        IDC_INSUL_BTN_CALC_ALL, IDC_INSUL_BTN_ASSIGN_ALL, IDC_INSUL_STATIC_BATCH
    };
    for (int id : singleIds)
        if (CWnd* w = GetDlgItem(id)) w->ShowWindow(single ? SW_SHOW : SW_HIDE);
    for (int id : allIds)
        if (CWnd* w = GetDlgItem(id)) w->ShowWindow(single ? SW_HIDE : SW_SHOW);

    SetDlgItemText(IDC_INSUL_STATIC_TITLE,
        single ? L"\x0420\x0435\x0436\x0438\x043C\x003A\x0020\x043E\x0434\x043D\x0430\x0020\x043E\x0441\x044C" : L"\x0420\x0435\x0436\x0438\x043C\x003A\x0020\x0432\x0441\x0435\x0020\x043E\x0441\x0438\x0020\x002F\x0020\x0437\x043E\x043D\x044B");
}

BOOL CInsulDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    if (!m_idAxis.isNull())
        ReadDiameters(m_idAxis, m_dOuterMm, m_dnMm);

    m_listZones.SubclassDlgItem(IDC_INSUL_LIST_ZONES, this);
    m_listZones.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_GRIDLINES);
    m_listZones.InsertColumn(0, L"\x041E\x0441\x044C", LVCFMT_LEFT, 150);
    m_listZones.InsertColumn(1, L"\x0417\x043E\x043D\x0430", LVCFMT_LEFT, 160);
    m_listZones.InsertColumn(2, L"\x0044\x043D\x0430\x0440", LVCFMT_RIGHT, 55);
    m_listZones.InsertColumn(3, L"\x0421\x0440\x0435\x0434\x0430", LVCFMT_LEFT, 55);
    m_listZones.InsertColumn(4, L"\x0074\x0020\x0043", LVCFMT_RIGHT, 50);
    m_listZones.InsertColumn(5, L"\x0064\x0065\x006C\x0074\x0061\x0020\x043C\x043C", LVCFMT_RIGHT, 55);
    m_listZones.InsertColumn(6, L"\x0442\x0440\x0443\x0431", LVCFMT_RIGHT, 45);

    FillMediaCombos();
    SetDlgItemText(IDC_INSUL_EDIT_T, L"90");
    SetDlgItemText(IDC_INSUL_EDIT_T2, L"90");
    if (CButton* b = (CButton*)GetDlgItem(IDC_INSUL_CHK_SPLIT_WELD))
        b->SetCheck(BST_CHECKED);

    CString s;
    if (m_mode == InsulDlgMode::SingleAxis)
        s.Format(L"\x0044\x043D\x0430\x0440\x003D\x0025\x002E\x0031\x0066\x0020\x043C\x043C\x002C\x0020\x0044\x004E\x003D\x0025\x002E\x0031\x0066\x0020\x0020\x007C\x0020\x0020\x041D\x0420\x0020\x0033\x0034\x002C\x0020\x043F\x043E\x043C\x0435\x0449\x0435\x043D\x0438\x0435\x002C\x0020\x0074\x0432\x043E\x0437\x0434\x003D\x0032\x0035\x0020\x0043", m_dOuterMm, m_dnMm);
    else
        s = L"\x041D\x0420\x0020\x0033\x0034\x002C\x0020\x043F\x043E\x043C\x0435\x0449\x0435\x043D\x0438\x0435\x002C\x0020\x0074\x0432\x043E\x0437\x0434\x003D\x0032\x0035\x0020\x0043\x0020\x0020\x007C\x0020\x0020\x043E\x0442\x043C\x0435\x0442\x044C\x0442\x0435\x0020\x043E\x0441\x0438\x002F\x0437\x043E\x043D\x044B";
    SetDlgItemText(IDC_INSUL_STATIC_D, s);
    SetDlgItemText(IDC_INSUL_EDIT_REPORT,
        L"\x0420\x0430\x0441\x0447\x0451\x0442\x0020\x043F\x043E\x0020\x041D\x0420\x0020\x0033\x0034\x002D\x0037\x0030\x002D\x0031\x0031\x0038\x002D\x0038\x0037\x0020\x0434\x043B\x044F\x0020\x043F\x043E\x043C\x0435\x0449\x0435\x043D\x0438\x044F\x002E" L"\r\n"
        L"\x0420\x0435\x0433\x0438\x043E\x043D\x0020\x043D\x0435\x0020\x043D\x0443\x0436\x0435\x043D\x003A\x0020\x004B\x003D\x0031\x002C\x0020\x0074\x0020\x0432\x043E\x0437\x0434\x0443\x0445\x0430\x0020\x003D\x0020\x0032\x0035\x0020\x0043\x002E" L"\r\n"
        L"\x0418\x0442\x043E\x0433\x0020\x0064\x0065\x006C\x0074\x0061\x0020\x003D\x0020\x006D\x0061\x0078\x0028\x043F\x043E\x0020\x0442\x0435\x043F\x043B\x043E\x043F\x043E\x0442\x0435\x0440\x044F\x043C\x0020\x0071\x002C\x0020\x043F\x043E\x0020\x0074\x0020\x043F\x043E\x0432\x0435\x0440\x0445\x043D\x043E\x0441\x0442\x0438\x0029\x002E");
    if (CWnd* b = GetDlgItem(IDC_INSUL_BTN_ASSIGN))
        b->EnableWindow(FALSE);

    ApplyModeLayout();

    if (m_mode == InsulDlgMode::AllAxes)
    {
        SetWindowText(L"\x004D\x0073\x0050\x0069\x0070\x0065\x0049\x007A\x006F\x006C\x0020\x2014\x0020\x0432\x0441\x0435\x0020\x043E\x0441\x0438\x0020\x0028\x043F\x043E\x043C\x0435\x0449\x0435\x043D\x0438\x0435\x0029");
        RebuildZones();
        if (!m_zones.empty())
            HighlightAxis(m_zones[0].idAxis);
    }
    else
    {
        SetWindowText(L"\x004D\x0073\x0050\x0069\x0070\x0065\x0049\x007A\x006F\x006C\x0020\x2014\x0020\x043E\x0434\x043D\x0430\x0020\x043E\x0441\x044C\x0020\x0028\x043F\x043E\x043C\x0435\x0449\x0435\x043D\x0438\x0435\x0029");
        if (!m_idAxis.isNull())
            HighlightAxis(m_idAxis);
    }
    return TRUE;
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
    CString name = L"\x041E\x0441\x044C";
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

    // fallback: collect pipes by axis id from model (axis may have empty GetSeg)
    if (pipes.empty())
    {
        AcDbObjectIdArray fb;
        CollectAxisSegments(idAxis, fb);
        for (int i = 0; i < fb.length(); ++i)
        {
            ChainPipe cp;
            cp.id = fb.at(i);
            cp.pos = (double)i;
            pipes.push_back(cp);
        }
    }

    if (!splitByWeld || welds.empty() || pipes.empty())
    {
        InsulZone z;
        z.idAxis = idAxis;
        z.axisName = axisName;
        z.zoneName = L"axis";
        z.zoneIndex = 0;
        z.zoneCount = 1;
        z.dOuterMm = dOuter;
        z.dnMm = dn;
        z.splitByWeld = false;
        for (const auto& p : pipes) z.pipeIds.append(p.id);
        out.push_back(z); // always show axis in list
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
            zones[zi].zoneName.Format(L"\x0434\x043E\x0020\x0448\x0432\x0430\x0020\x0031\x0020\x002F\x0020\x0025\x0064", nZones);
        else if (zi == nZones - 1)
            zones[zi].zoneName.Format(L"\x043F\x043E\x0441\x043B\x0435\x0020\x0448\x0432\x0430\x0020\x0025\x0064\x0020\x002F\x0020\x0025\x0064", (int)welds.size(), nZones);
        else
            zones[zi].zoneName.Format(L"\x043C\x0435\x0436\x0434\x0443\x0020\x0448\x0432\x0430\x043C\x0438\x0020\x0025\x0064\x002D\x0025\x0064", zi, zi + 1);
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

    bool any = false;
    for (auto& z : zones)
    {
        if (z.pipeIds.length() > 0)
        {
            out.push_back(z);
            any = true;
        }
    }
    if (!any)
    {
        InsulZone z;
        z.idAxis = idAxis;
        z.axisName = axisName;
        z.zoneName = L"axis";
        z.zoneIndex = 0;
        z.zoneCount = 1;
        z.dOuterMm = dOuter;
        z.dnMm = dn;
        for (const auto& p : pipes) z.pipeIds.append(p.id);
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
    msg.Format(L"\x0417\x043E\x043D\x002F\x043E\x0441\x0435\x0439\x003A\x0020\x0025\x0064\x0020\x0028\x0440\x0430\x0437\x0440\x0435\x0437\x0020\x043F\x043E\x0020\x0057\x0065\x006C\x0064\x003A\x0020\x0025\x0073\x0029", (int)m_zones.size(), split ? L"\x0434\x0430" : L"\x043D\x0435\x0442");
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
        m_listZones.SetItemText(row, 3, z.mediaIdx == 1 ? L"\x041F\x0430\x0440" : L"\x0412\x043E\x0434\x0430");
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
        AfxMessageBox(L"\x041F\x0440\x043E\x0432\x0435\x0440\x044C\x0442\x0435\x0020\x0434\x0430\x043D\x043D\x044B\x0435\x002E", MB_ICONWARNING);
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
    msg.Format(L"\x041F\x0440\x0438\x043C\x0435\x043D\x0435\x043D\x043E\x0020\x0074\x003D\x0025\x002E\x0031\x0066\x0020\x043A\x0020\x0025\x0064\x0020\x0437\x043E\x043D\x0430\x043C\x002E\x0020\x041F\x0435\x0440\x0435\x0441\x0447\x0438\x0442\x0430\x0439\x0442\x0435\x002E", t, n);
    SetDlgItemText(IDC_INSUL_STATIC_BATCH, msg);
}

void CInsulDlg::OnBnCalcAll()
{
    SyncZoneFromListChecks();

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
        in.tAirC = kIndoorTAirC;
        in.alpha = 10.0;
        in.kLoss = (z.dnMm < 150.0) ? 1.20 : 1.15;
        in.lambda = 0.049;
        in.hoursOver5000 = true;
        in.regionK = kIndoorRegionK;
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
    msg.Format(L"\x0420\x0430\x0441\x0441\x0447\x0438\x0442\x0430\x043D\x043E\x0020\x0437\x043E\x043D\x003A\x0020\x0025\x0064", nOk);
    SetDlgItemText(IDC_INSUL_STATIC_BATCH, msg);
}

void CInsulDlg::OnBnAssignAll()
{
    SyncZoneFromListChecks();

    // auto-calc selected zones if needed
    bool needCalc = false;
    for (const auto& z : m_zones)
    {
        if (z.selected && !z.calculated)
        {
            needCalc = true;
            break;
        }
    }
    if (needCalc)
        OnBnCalcAll();

    // one assign per axis (full route: pipes + elbows), use max delta among selected zones
    struct AxisJob
    {
        AcDbObjectId idAxis;
        double deltaMm = 0;
        double tMediaC = 90;
        int mediaIdx = 0;
        AcDbObjectIdArray pipeIds;
    };
    std::map<Adesk::IntDbId, AxisJob> jobs;
    for (const auto& z : m_zones)
    {
        if (!z.selected || !z.calculated || z.deltaMm <= 0 || z.idAxis.isNull())
            continue;
        AxisJob& j = jobs[z.idAxis.asOldId()];
        j.idAxis = z.idAxis;
        if (z.deltaMm > j.deltaMm)
        {
            j.deltaMm = z.deltaMm;
            j.tMediaC = z.tMediaC;
            j.mediaIdx = z.mediaIdx;
        }
        for (int i = 0; i < z.pipeIds.length(); ++i)
            AppendUnique(j.pipeIds, z.pipeIds.at(i));
    }

    int nAssign = 0;
    for (auto& kv : jobs)
    {
        AxisJob& j = kv.second;
        if (AssignToPipes(j.idAxis, j.pipeIds, j.deltaMm, true))
            ++nAssign;
    }

    CString msg;
    msg.Format(L"\x041D\x0430\x0437\x043D\x0430\x0447\x0435\x043D\x043E\x0020\x043E\x0441\x0435\x0439\x003A\x0020\x0025\x0064", nAssign);
    SetDlgItemText(IDC_INSUL_STATIC_BATCH, msg);
    AfxMessageBox(msg, nAssign > 0 ? MB_ICONINFORMATION : MB_ICONWARNING);
}

bool CInsulDlg::ReadUi(InsulCalc::Input& in, CString& mediaName) const
{
    CComboBox* pMed = (CComboBox*)GetDlgItem(IDC_INSUL_CMB_MEDIA);
    int iMed = pMed ? pMed->GetCurSel() : 0;
    if (iMed < 0) iMed = 0;
    CString tStr;
    GetDlgItemText(IDC_INSUL_EDIT_T, tStr);
    const double t = _wtof(tStr);
    if (t < -50.0 || t > 700.0) return false;
    mediaName = (iMed == 1) ? L"\x041F\x0430\x0440" : L"\x0412\x043E\x0434\x0430";
    in = InsulCalc::Input{};
    in.norm = InsulCalc::Norm::Nr34;
    in.loc = InsulCalc::Location::Indoor;
    in.dOuterMm = m_dOuterMm;
    in.dnMm = m_dnMm;
    in.tMediaC = t;
    in.tAirC = kIndoorTAirC;
    in.alpha = 10.0;
    in.kLoss = (m_dnMm < 150.0) ? 1.20 : 1.15;
    in.lambda = 0.049;
    in.hoursOver5000 = true;
    in.regionK = kIndoorRegionK;
    return true;
}

void CInsulDlg::ShowResult(const InsulCalc::Input& in, const InsulCalc::Result& r,
    const CString& mediaName)
{
    CString ru;
    ru.Format(
        L"\x003D\x003D\x003D\x0020\x0420\x0410\x0421\x0427\x0401\x0422\x0020\x0028\x041D\x0420\x0020\x0033\x0034\x002C\x0020\x043F\x043E\x043C\x0435\x0449\x0435\x043D\x0438\x0435\x0029\x0020\x003D\x003D\x003D" L"\r\n"
        L"\x0420\x0430\x0441\x043F\x043E\x043B\x043E\x0436\x0435\x043D\x0438\x0435\x003A\x0020\x043F\x043E\x043C\x0435\x0449\x0435\x043D\x0438\x0435\x0020\x0028\x004B\x003D\x0025\x002E\x0032\x0066\x002C\x0020\x0074\x0432\x043E\x0437\x0434\x003D\x0025\x002E\x0030\x0066\x0020\x0043\x0029" L"\r\n"
        L"\x0421\x0440\x0435\x0434\x0430\x003A\x0020\x0025\x0073\x002C\x0020\x0074\x003D\x0025\x002E\x0031\x0066\x0020\x0043\x0020\x007C\x0020\x0044\x043D\x0430\x0440\x003D\x0025\x002E\x0031\x0066\x0020\x0044\x004E\x003D\x0025\x002E\x0031\x0066\x0020\x007C\x0020\x004B\x043E\x043F\x003D\x0025\x002E\x0032\x0066" L"\r\n"
        L"\x0071\x003D\x0025\x002E\x0032\x0066\x0020\x0412\x0442\x002F\x043C\x0020\x007C\x0020\x0064\x0071\x003D\x0025\x002E\x0031\x0066\x0020\x007C\x0020\x0064\x0074\x0073\x003D\x0025\x002E\x0031\x0066\x0020\x007C\x0020\x043A\x0440\x0438\x0442\x0435\x0440\x0438\x0439\x003A\x0020\x0025\x0073" L"\r\n"
        L"\x0418\x0422\x041E\x0413\x041E\x0020\x0064\x0065\x006C\x0074\x0061\x0020\x003D\x0020\x0025\x002E\x0030\x0066\x0020\x043C\x043C" L"\r\n",
        in.regionK, in.tAirC,
        static_cast<LPCWSTR>(mediaName), in.tMediaC, in.dOuterMm, in.dnMm, in.kLoss,
        r.qNormWm, r.deltaQMm, r.deltaTsMm, r.criterion,
        r.deltaMm);
    SetDlgItemText(IDC_INSUL_EDIT_REPORT, ru);
    CString big;
    big.Format(L"\x0418\x0442\x043E\x0433\x043E\x0432\x0430\x044F\x0020\x0442\x043E\x043B\x0449\x0438\x043D\x0430\x003A\x0020\x0025\x002E\x0030\x0066\x0020\x043C\x043C", r.deltaMm);
    SetDlgItemText(IDC_INSUL_STATIC_RESULT, big);
}

void CInsulDlg::OnBnCalculate()
{
    InsulCalc::Input in;
    CString mediaName;
    if (!ReadUi(in, mediaName))
    {
        AfxMessageBox(L"\x041F\x0440\x043E\x0432\x0435\x0440\x044C\x0442\x0435\x0020\x0434\x0430\x043D\x043D\x044B\x0435\x0020\x0028\x0442\x0435\x043C\x043F\x0435\x0440\x0430\x0442\x0443\x0440\x0430\x0029\x002E", MB_ICONWARNING);
        return;
    }
    m_last = InsulCalc::Calculate(in);
    m_hasResult = m_last.ok;
    m_lastT = in.tMediaC;
    m_lastMedia = mediaName;
    ShowResult(in, m_last, mediaName);
    if (CWnd* b = GetDlgItem(IDC_INSUL_BTN_ASSIGN))
        b->EnableWindow(m_last.ok ? TRUE : FALSE);
}

bool CInsulDlg::CollectAxisSegments(AcDbObjectIdArray& pipeIds) const
{
    return CollectAxisSegments(m_idAxis, pipeIds);
}

bool CInsulDlg::CollectAxisSegments(AcDbObjectId idAxis, AcDbObjectIdArray& pipeIds) const
{
    // Full route: pipes (subsegs) + elbows/nodes + inlines
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
            AppendUnique(pipeIds, idSeg);

            AcDbObject* pObj = nullptr;
            if (acdbOpenObject(pObj, idSeg, AcDb::kForRead) != Acad::eOk || !pObj) continue;
            if (pObj->isKindOf(vCSSegment2::desc()))
            {
                vCSSegment2* pSeg = vCSSegment2::cast(pObj);
                if (pSeg)
                {
                    AcDbObjectIdArray ss;
                    pSeg->SSeg_GetArr(ss);
                    for (int j = 0; j < ss.length(); ++j)
                        AppendUnique(pipeIds, ss.at(j));
                    AcDbObjectIdArray ils;
                    pSeg->GetAllInlinesSorted(ils);
                    for (int j = 0; j < ils.length(); ++j)
                        AppendUnique(pipeIds, ils.at(j));
                }
            }
            else if (pObj->isKindOf(vCSSegment::desc()))
            {
                vCSSegment* pSeg = vCSSegment::cast(pObj);
                if (pSeg)
                {
                    AcDbObjectIdArray ss;
                    pSeg->SSeg_GetArr(ss);
                    for (int j = 0; j < ss.length(); ++j)
                        AppendUnique(pipeIds, ss.at(j));
                }
            }
            pObj->close();
        }

        // elbows / terminators / nodes
        const int nNodes = pAx->GetNodesCount();
        for (int i = 0; i < nNodes; ++i)
        {
            const AcDbObjectId idN = pAx->GetNode(i);
            if (!idN.isNull())
                AppendUnique(pipeIds, idN);
        }
        pAx->close();
    }

    // model space: any entity linked to this axis
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

                        AcDbObjectId ax;
                        if (pObj->isKindOf(vCSEntity::desc()))
                        {
                            if (vCSEntity* pE = vCSEntity::cast(pObj))
                                ax = pE->GetOIdAxis();
                        }
                        else if (pObj->isKindOf(vCSSubSegment::desc()))
                        {
                            if (vCSSubSegment* pSS = vCSSubSegment::cast(pObj))
                                ax = pSS->GetOIdAxis();
                        }
                        else if (pObj->isKindOf(vCSNode::desc()))
                        {
                            // nodes: try GetOIdAxis if available via cast to entity-like API
                            if (vCSEntity* pE = vCSEntity::cast(pObj))
                                ax = pE->GetOIdAxis();
                        }

                        if (!ax.isNull() && ax == idAxis)
                            AppendUnique(pipeIds, id);
                        pObj->close();
                    }
                    delete pIt;
                }
                pMS->close();
            }
            pBT->close();
        }
    }

    acutPrintf(L"\n[PIPE_INSUL] route items on axis: %d\n", pipeIds.length());
    return pipeIds.length() > 0;
}

bool CInsulDlg::AssignToPipes(AcDbObjectId idAxis, const AcDbObjectIdArray& pipeIds,
    double thicknessMm, bool writeAxisLevel)
{
    // No axis/entity parameters — only 3D insulation objects.

    AcDbObjectIdArray ents;
    for (int i = 0; i < pipeIds.length(); ++i)
        AppendUnique(ents, pipeIds.at(i));

    AcDbObjectIdArray elbowNodes;
    if (!idAxis.isNull())
    {
        AcDbObjectIdArray allRoute;
        CollectAxisSegments(idAxis, allRoute);

        vCSAxis* pAx = nullptr;
        if (acdbOpenObject(pAx, idAxis, AcDb::kForRead) == Acad::eOk && pAx)
        {
            const int nNodes = pAx->GetNodesCount();
            for (int i = 0; i < nNodes; ++i)
            {
                const AcDbObjectId idN = pAx->GetNode(i);
                if (idN.isNull()) continue;
                vCSNode* pNode = nullptr;
                if (acdbOpenObject(pNode, idN, AcDb::kForRead) != Acad::eOk || !pNode)
                    continue;
                const bool isElb = (pNode->IsElbow() != nullptr) || (pNode->IsElbowOffset() != nullptr);
                pNode->close();
                if (isElb)
                {
                    AppendUnique(elbowNodes, idN);
                    AppendUnique(allRoute, idN);
                }
            }
            pAx->close();
        }

        if (writeAxisLevel || ents.length() == 0)
            ents = allRoute;
        else
        {
            for (int i = 0; i < elbowNodes.length(); ++i)
                AppendUnique(ents, elbowNodes.at(i));
        }
    }

    AcDbObjectIdArray pipeEnts;
    AcDbObjectIdArray elbowEnts;
    {
        std::vector<AcDbObjectId> vp, ve, vs;
        if (CInsulModel::get_valid_pipes(ents, vp) == Acad::eOk)
            for (const auto& id : vp) pipeEnts.append(id);
        if (CInsulModel::get_valid_elbows(ents, ve) == Acad::eOk)
            for (const auto& id : ve) elbowEnts.append(id);
        if (pipeEnts.length() == 0 && CInsulModel::get_valid_segs(ents, vs) == Acad::eOk)
            for (const auto& id : vs) pipeEnts.append(id);
        for (int i = 0; i < elbowNodes.length(); ++i)
            AppendUnique(elbowEnts, elbowNodes.at(i));
        if (pipeEnts.length() == 0 && elbowEnts.length() == 0)
            pipeEnts = ents;
    }

    acutPrintf(L"\n[PIPE_INSUL] assign ents=%d pipes=%d elbows=%d (IsElbow=%d) delta=%.0f\n",
        ents.length(), pipeEnts.length(), elbowEnts.length(), elbowNodes.length(), thicknessMm);

    HMODULE h = ::GetModuleHandleW(L"ViperCSObj.nrx");
    if (!h) h = ::GetModuleHandleW(L"ViperCSObj.dbx");
    if (!h) h = ::GetModuleHandleW(L"ViperCSObj.dll");

    using PfnCreateInsul = Acad::ErrorStatus(__cdecl*)(
        const AcDbObjectIdArray&, const long&, double, AcDbObjectId&, double, double, const CElement*);
    using PfnCreateElbow = Acad::ErrorStatus(__cdecl*)(
        const AcDbObjectIdArray&, const long&, double, double, double);

    static PfnCreateInsul s_pfnPipe = nullptr;
    static PfnCreateElbow s_pfnElbow = nullptr;
    static bool s_resolved = false;
    if (h && !s_resolved)
    {
        s_resolved = true;
        s_pfnPipe = reinterpret_cast<PfnCreateInsul>(::GetProcAddress(h,
            "?create_insul@CInsulModel@@SA?AW4ErrorStatus@Nano@@"
            "AEBV?$NcArray@VNcDbObjectId@@V?$HostObjectsAllocator@VNcDbObjectId@@@@@@"
            "AEBJNAEAVNcDbObjectId@@NNPEBVCElement@@@Z"));
        s_pfnElbow = reinterpret_cast<PfnCreateElbow>(::GetProcAddress(h,
            "?create_insul_elbow@CInsulModel@@SA?AW4ErrorStatus@Nano@@"
            "AEBV?$NcArray@VNcDbObjectId@@V?$HostObjectsAllocator@VNcDbObjectId@@@@@@"
            "AEBJNNN@Z"));
        acutPrintf(L"\n[PIPE_INSUL] exports pipe=%d elbow=%d\n",
            s_pfnPipe ? 1 : 0, s_pfnElbow ? 1 : 0);
    }

    long flagsPipe = (long)CInsulModel::n_InsulFull;
    if (writeAxisLevel)
        flagsPipe |= (long)CInsulModel::n_InsulParallel;
    long flagsElbow = (long)CInsulModel::n_InsulFull;

    bool ok = false;
    AcDbObjectId idModel;
    double tickUsed = thicknessMm;

    if (s_pfnPipe && pipeEnts.length() > 0)
    {
        Acad::ErrorStatus es = s_pfnPipe(pipeEnts, flagsPipe, thicknessMm, idModel, 0.0, 5.0, nullptr);
        if (es != Acad::eOk)
        {
            tickUsed = thicknessMm * 0.001;
            es = s_pfnPipe(pipeEnts, flagsPipe, tickUsed, idModel, 0.0, 5.0, nullptr);
        }
        else
            tickUsed = thicknessMm;
        if (es == Acad::eOk)
        {
            ok = true;
            acutPrintf(L"\n[PIPE_INSUL] create_insul OK pipes=%d model=%lu tick=%.4f\n",
                pipeEnts.length(), (unsigned long)idModel.asOldId(), tickUsed);
        }
        else
            acutPrintf(L"\n[PIPE_INSUL] create_insul FAIL es=%d pipes=%d\n", (int)es, pipeEnts.length());
    }
    else if (!s_pfnPipe)
        acutPrintf(L"\n[PIPE_INSUL] create_insul export missing\n");

    // Elbows: one-by-one with same tick as pipes; then batch; then vCSInsulation fallback
    bool elbowsOk = false;
    if (elbowEnts.length() > 0 && s_pfnElbow)
    {
        int nOne = 0;
        for (int i = 0; i < elbowEnts.length(); ++i)
        {
            AcDbObjectIdArray one;
            one.append(elbowEnts.at(i));
            Acad::ErrorStatus e1 = s_pfnElbow(one, flagsElbow, tickUsed, 0.0, 5.0);
            if (e1 != Acad::eOk)
                e1 = s_pfnElbow(one, flagsElbow, thicknessMm, 0.0, 5.0);
            if (e1 != Acad::eOk)
                e1 = s_pfnElbow(one, flagsElbow, thicknessMm * 0.001, 0.0, 5.0);
            if (e1 == Acad::eOk)
                ++nOne;
            else
                acutPrintf(L"\n[PIPE_INSUL] create_insul_elbow FAIL one=%d es=%d\n", i, (int)e1);
        }
        if (nOne > 0)
        {
            elbowsOk = true;
            acutPrintf(L"\n[PIPE_INSUL] create_insul_elbow OK %d/%d\n", nOne, elbowEnts.length());
        }
        else
        {
            Acad::ErrorStatus es = s_pfnElbow(elbowEnts, flagsElbow, tickUsed, 0.0, 5.0);
            if (es != Acad::eOk)
                es = s_pfnElbow(elbowEnts, flagsElbow, thicknessMm * 0.001, 0.0, 5.0);
            if (es == Acad::eOk)
            {
                elbowsOk = true;
                acutPrintf(L"\n[PIPE_INSUL] create_insul_elbow batch OK elbows=%d\n", elbowEnts.length());
            }
            else
                acutPrintf(L"\n[PIPE_INSUL] create_insul_elbow batch FAIL es=%d\n", (int)es);
        }
    }
    else if (elbowEnts.length() > 0 && !s_pfnElbow)
        acutPrintf(L"\n[PIPE_INSUL] create_insul_elbow export missing\n");

    if (elbowEnts.length() > 0 && !elbowsOk)
    {
        vCSInsulation* pIns = new vCSInsulation();
        if (AppendToModelSpace(pIns) == Acad::eOk)
        {
            pIns->setPipeItemReferences(elbowEnts);
            pIns->setThickness(tickUsed);
            pIns->setFlagFull(true);
            EMpiError err = pIns->constructFromMultyElbows();
            if (err != eMpiErrOk)
            {
                pIns->setThickness(thicknessMm * 0.001);
                err = pIns->constructFromMultyElbows();
            }
            if (err != eMpiErrOk)
                err = pIns->constructFromElbowsProjOobj();
            pIns->close();
            if (err == eMpiErrOk)
            {
                elbowsOk = true;
                acutPrintf(L"\n[PIPE_INSUL] vCSInsulation elbows OK n=%d\n", elbowEnts.length());
            }
            else
                acutPrintf(L"\n[PIPE_INSUL] vCSInsulation elbows FAIL err=%d\n", (int)err);
        }
        else
            delete pIns;
    }

    if (elbowEnts.length() > 0 && !elbowsOk)
    {
        MultiPipeInsulation* pMp = new MultiPipeInsulation();
        if (AppendToModelSpace(pMp) == Acad::eOk)
        {
            pMp->setFlagFull(true);
            pMp->SetElbowArray(elbowEnts, tickUsed);
            pMp->close();
            elbowsOk = true;
            acutPrintf(L"\n[PIPE_INSUL] MultiPipeInsulation::SetElbowArray OK n=%d\n", elbowEnts.length());
        }
        else
        {
            delete pMp;
            acutPrintf(L"\n[PIPE_INSUL] all elbow methods FAILED\n");
        }
    }

    if (elbowEnts.length() == 0)
        acutPrintf(L"\n[PIPE_INSUL] no elbows (IsElbow=%d)\n", elbowNodes.length());

    return ok || elbowsOk;
}

bool CInsulDlg::AssignInsulation(double thicknessMm, double /*tMediaC*/, const CString& /*mediaName*/)
{
    AcDbObjectIdArray pipeIds;
    CollectAxisSegments(m_idAxis, pipeIds);
    return AssignToPipes(m_idAxis, pipeIds, thicknessMm, true);
}

void CInsulDlg::OnBnAssign()
{
    if (!m_hasResult || !m_last.ok)
    {
        AfxMessageBox(L"\x0421\x043D\x0430\x0447\x0430\x043B\x0430\x0020\x0432\x044B\x043F\x043E\x043B\x043D\x0438\x0442\x0435\x0020\x0440\x0430\x0441\x0447\x0451\x0442\x002E", MB_ICONWARNING);
        return;
    }
    CString ask;
    ask.Format(L"\x041D\x0430\x0437\x043D\x0430\x0447\x0438\x0442\x044C\x0020\x0438\x0437\x043E\x043B\x044F\x0446\x0438\x044E\x0020\x0025\x002E\x0030\x0066\x0020\x043C\x043C\x0020\x043D\x0430\x0020\x0442\x0435\x043A\x0443\x0449\x0443\x044E\x0020\x043E\x0441\x044C\x003F", m_last.deltaMm);
    if (AfxMessageBox(ask, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    if (AssignInsulation(m_last.deltaMm, m_lastT, m_lastMedia))
        AfxMessageBox(L"\x0413\x043E\x0442\x043E\x0432\x043E\x002E", MB_ICONINFORMATION);
    else
        AfxMessageBox(L"\x041D\x0435\x0020\x0443\x0434\x0430\x043B\x043E\x0441\x044C\x0020\x043D\x0430\x0437\x043D\x0430\x0447\x0438\x0442\x044C\x0020\x0438\x0437\x043E\x043B\x044F\x0446\x0438\x044E\x002E", MB_ICONWARNING);
}
