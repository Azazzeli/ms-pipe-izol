#include "stdafx.h"
#include "InsulDlg.h"
#include "aced.h"
#include "dbapserv.h"

#include "vCSAxis.h"
#include "vCSSegment.h"
#include "vCSSubSegment.h"
#include "vCSEntity.h"
#include "vCSWeld.h"
#include "vCSInsulation.h"
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

    // New MS path = PIPE_MULTIPIPE_INSULATION: CInsulModel::create_insul
    // Creates real insulation object (CInsulModel + solids) on top of pipes.
    AcDbObjectIdArray ents;
    for (int i = 0; i < pipeIds.length(); ++i)
        AppendUnique(ents, pipeIds.at(i));

    std::vector<AcDbObjectId> validPipes;
    if (CInsulModel::get_valid_pipes(ents, validPipes) == Acad::eOk && !validPipes.empty())
    {
        ents.removeAll();
        for (const AcDbObjectId& id : validPipes)
            ents.append(id);
    }
    else
    {
        std::vector<AcDbObjectId> validSegs;
        if (CInsulModel::get_valid_segs(ents, validSegs) == Acad::eOk && !validSegs.empty())
        {
            ents.removeAll();
            for (const AcDbObjectId& id : validSegs)
                ents.append(id);
        }
    }

    if (ents.length() > 0)
    {
        // Lib exports create_insul(long const& flags); header Int32 may mangle as int — resolve by name.
        using PfnCreateInsul = Acad::ErrorStatus(__cdecl*)(
            const AcDbObjectIdArray&,
            const long&,
            double,
            AcDbObjectId&,
            double,
            double,
            const CElement*);

        static PfnCreateInsul s_pfn = nullptr;
        if (!s_pfn)
        {
            HMODULE h = ::GetModuleHandleW(L"ViperCSObj.nrx");
            if (!h) h = ::GetModuleHandleW(L"ViperCSObj.dbx");
            if (!h) h = ::GetModuleHandleW(L"ViperCSObj.dll");
            if (h)
            {
                s_pfn = reinterpret_cast<PfnCreateInsul>(::GetProcAddress(h,
                    "?create_insul@CInsulModel@@SA?AW4ErrorStatus@Nano@@"
                    "AEBV?$NcArray@VNcDbObjectId@@V?$HostObjectsAllocator@VNcDbObjectId@@@@@@"
                    "AEBJNAEAVNcDbObjectId@@NNPEBVCElement@@@Z"));
            }
        }

        long flags = (long)CInsulModel::n_InsulFull;
        if (writeAxisLevel)
            flags |= (long)CInsulModel::n_InsulParallel;

        AcDbObjectId idModel;
        Acad::ErrorStatus es = Acad::eInvalidInput;
        if (s_pfn)
        {
            es = s_pfn(ents, flags, thicknessMm, idModel, 0.0, 5.0, nullptr);
            if (es != Acad::eOk)
                es = s_pfn(ents, flags, thicknessMm * 0.001, idModel, 0.0, 5.0, nullptr);
        }
        else
        {
            acutPrintf(L"\n[PIPE_INSUL] create_insul export not found in ViperCSObj\n");
        }

        if (es == Acad::eOk && !idModel.isNull())
            acutPrintf(L"\n[PIPE_INSUL] CInsulModel::create_insul OK delta=%.0f mm ents=%d model=%lu\n",
                thicknessMm, ents.length(), (unsigned long)idModel.asOldId());
        else
            acutPrintf(L"\n[PIPE_INSUL] CInsulModel::create_insul FAIL es=%d ents=%d pfn=%p\n",
                (int)es, ents.length(), (void*)s_pfn);
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
