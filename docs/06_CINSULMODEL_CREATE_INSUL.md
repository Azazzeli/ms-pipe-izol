# Новый способ назначения изоляции: `CInsulModel::create_insul`

Это тот же путь, что команда Model Studio **`PIPE_MULTIPIPE_INSULATION`**  
(«Добавить совместную изоляцию») — создаётся **объект изоляции поверх трубы**, а не только параметры `PART_*`.

---

## Что было не так

| Способ | Класс / API | Результат |
|--------|-------------|-----------|
| Старый | `MultiPipeInsulation::SetPipeArray` + `ConstructFromPipes` | устаревший solid |
| Почти старый | `vCSInsulation` вручную + `constructFromPipes` | не полный путь команды |
| Утилита | `CInsulUtils::sortedObjByPiece` | не то же самое, что UI-команда |
| **Новый** | **`CInsulModel::create_insul`** | объект изоляции как в MS |

Параметры на оси/трубах (`PART_INSULATION_THICKNESS`, t, регион) можно писать отдельно — это атрибуты.  
3D-объект даёт только `create_insul`.

---

## Где в SDK

Файл: `Model Studio/Source/ViperCSObj/vCSInsulation.h`

```cpp
class CInsulModel : public AcDbEntity
{
public:
    enum _N_FLAG_STATE
    {
        n_InsulFull     = 0x01, // полная изоляция
        n_InsulParallel = 0x02, // совместная (параллельные участки)
        n_AxisIndep     = 0x04  // параллельность только между разными осями
    };

    // Создание целой изоляции (как PIPE_MULTIPIPE_INSULATION)
    static Acad::ErrorStatus create_insul(
        AcDbObjectIdArray const& idEnt,
        Adesk::Int32 const& uFlags,
        double dTick,
        AcDbObjectId& idObjModel,
        double dDist = 0.,
        double dAngle = 5.,
        const CElement* pRootParameters = nullptr);

    static Acad::ErrorStatus create_insul_elbow(...);
    static Acad::ErrorStatus rebuild_insul(AcDbObjectIdArray const& _insulArray);

    static Acad::ErrorStatus get_valid_pipes(
        AcDbObjectIdArray const& in_seg,
        std::vector<AcDbObjectId>& out_seg);
    static Acad::ErrorStatus get_valid_segs(
        AcDbObjectIdArray const& in_seg,
        std::vector<AcDbObjectId>& out_seg);
};
```

Команда в cfg: `Support/PIPING/ms_pipe.cfg` → `PIPE_MULTIPIPE_INSULATION`.

---

## Важно про линковку

В `ViperCSObj.lib` флаг экспортируется как **`long const&`** (манглинг `AEBJ`),  
а в заголовке часто `Adesk::Int32` → `int` (`AEBH`). Прямой вызов `CInsulModel::create_insul` может дать **LNK2001**.

Рабочий вариант — `GetProcAddress` по имени из lib:

```text
?create_insul@CInsulModel@@SA?AW4ErrorStatus@Nano@@
AEBV?$NcArray@VNcDbObjectId@@V?$HostObjectsAllocator@VNcDbObjectId@@@@@@
AEBJNAEAVNcDbObjectId@@NNPEBVCElement@@@Z
```

Модуль: `ViperCSObj.nrx` (должен быть уже загружен Model Studio).

---

## Реализация в MsPipeIzol (`InsulDlg.cpp`)

Схема:

1. Собрать id труб/подсегментов зоны или оси.  
2. Отфильтровать через `get_valid_pipes` / `get_valid_segs`.  
3. Вызвать `create_insul` с флагами.  
4. Параллельно записать `PART_*` / температуру / регион (как раньше).

### Фрагмент (назначение 3D)

```cpp
#include "vCSInsulation.h"

// Resolve export (long const& flags) from loaded ViperCSObj.nrx
using PfnCreateInsul = Acad::ErrorStatus(__cdecl*)(
    const AcDbObjectIdArray&,
    const long&,
    double,
    AcDbObjectId&,
    double,
    double,
    const CElement*);

static PfnCreateInsul ResolveCreateInsul()
{
    HMODULE h = ::GetModuleHandleW(L"ViperCSObj.nrx");
    if (!h) h = ::GetModuleHandleW(L"ViperCSObj.dbx");
    if (!h) return nullptr;
    return reinterpret_cast<PfnCreateInsul>(::GetProcAddress(h,
        "?create_insul@CInsulModel@@SA?AW4ErrorStatus@Nano@@"
        "AEBV?$NcArray@VNcDbObjectId@@V?$HostObjectsAllocator@VNcDbObjectId@@@@@@"
        "AEBJNAEAVNcDbObjectId@@NNPEBVCElement@@@Z"));
}

bool Assign3dInsulation(const AcDbObjectIdArray& pipeIds, double thicknessMm, bool jointParallel)
{
    AcDbObjectIdArray ents = pipeIds;

    std::vector<AcDbObjectId> valid;
    if (CInsulModel::get_valid_pipes(ents, valid) == Acad::eOk && !valid.empty())
    {
        ents.removeAll();
        for (const auto& id : valid) ents.append(id);
    }
    else if (CInsulModel::get_valid_segs(ents, valid) == Acad::eOk && !valid.empty())
    {
        ents.removeAll();
        for (const auto& id : valid) ents.append(id);
    }

    if (ents.isEmpty())
        return false;

    long flags = (long)CInsulModel::n_InsulFull;
    if (jointParallel)
        flags |= (long)CInsulModel::n_InsulParallel;

    PfnCreateInsul pfn = ResolveCreateInsul();
    if (!pfn)
        return false;

    AcDbObjectId idModel;
    Acad::ErrorStatus es = pfn(ents, flags, thicknessMm, idModel, 0.0, 5.0, nullptr);

    // если в сборке tick в метрах — второй заход
    if (es != Acad::eOk)
        es = pfn(ents, flags, thicknessMm * 0.001, idModel, 0.0, 5.0, nullptr);

    return (es == Acad::eOk && !idModel.isNull());
}
```

### Флаги

- `n_InsulFull` — изоляция по выбранным элементам (целая).  
- `n_InsulParallel` — режим «совместно», как в команде MS.  
- В MsPipeIzol: для всей оси `Full | Parallel`, для зоны — `Full`.

### Параметры (отдельно от 3D)

На entity / ось по-прежнему:

- `PART_INSULATION_THICKNESS`  
- `PART_INSULATION_DISABLEVIEW`  
- `TEMPERATURE`, `REGION`, `INSUL_MEDIA`  
- `vCSAxis::SetInsulationThickness`

Без `create_insul` в модели не появится объект «Изоляция» поверх трубы.

---

## Проверка в nanoCAD

1. `APPLOAD` → `deploy/MsPipeIzol.nrx` (или актуальный билд).  
2. `PIPE_INSUL` или `PIPE_INSUL_ALL` → расчёт → **Назначить**.  
3. В командной строке:  
   `[PIPE_INSUL] CInsulModel::create_insul OK ...`  
4. В модели — объект изоляции на трубе (как после ручной `PIPE_MULTIPIPE_INSULATION`).

---

## Связанные файлы проекта

- `InsulDlg.cpp` — `AssignToPipes`  
- `InsulCalc.cpp` — расчёт δ  
- `HelloNRX.cpp` — команды `PIPE_INSUL` / `PIPE_INSUL_ALL`  
- Решение: `MsPipeIzol.sln`
