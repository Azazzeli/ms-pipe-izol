#include "stdafx.h"
#include "aced.h"
#include "rxmfcapi.h"
#include "InsulDlg.h"
#include "InsulCalc.h"

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
        acrxDynamicLinker->unlockApplication(appId);
        acrxDynamicLinker->registerAppMDIAware(appId);
        acedRegCmds->addCommand(L"PIPE_GROUP", L"_PIPE_INSUL", L"PIPE_INSUL",
            ACRX_CMD_TRANSPARENT, pipeInsulCmd);
        acedRegCmds->addCommand(L"PIPE_GROUP", L"_PIPE_INSUL_ALL", L"PIPE_INSUL_ALL",
            ACRX_CMD_TRANSPARENT, pipeInsulAllCmd);
        acutPrintf(L"\nMsPipeIzol: PIPE_INSUL, PIPE_INSUL_ALL");
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
