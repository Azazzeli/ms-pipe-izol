# Как Model Studio хранит и отдаёт оси

## Где лежат оси

Оси **не** являются обычными «пачками линий только из model space» для перечисления.

В SDK:

```cpp
#define VCSSTR_DICTNAME_AXIS _T("Axis")  // vCSAxis.h
```

Оси регистрируются в **именованном словаре** чертежа `"Axis"` (Named Objects Dictionary).

Базовый API словарей (`vCSObject` / `vCSObject2`):

```cpp
vCSObject::GetDict(const CString& sName);
vCSObject::GetAllObjects(const CString& sName, AcDbObjectIdArray& arr);
vCSObject::GetAllObjectsX(const CString& sName, AcDbObjectIdArray& arr, AcDbDatabase* pDb);

vCSObject2::GetAllObjects(...);   // для сущностей-наследников AcDbEntity
vCSObject2::GetAllObjectsX(...);
```

Специализация для осей:

```cpp
vCSAxis::GetAllAxis(AcDbObjectIdArray& oIdArrAxis, AcDbDatabase* pdb = workingDatabase());
vCSAxis::GetAllAxisX(AcDbObjectIdArray& oIdArrAxis, AcDbDatabase* pDb); // без транзакций
```

Класс: `vCSAxis : public AcDbEntity` — ось одновременно entity и запись словаря.

## Как сейчас собирает HelloNRX (`InsulDlg.cpp` → `CollectAllAxes`)

1. `vCSAxis::GetAllAxis(axes, pDb)`
2. если пусто → `vCSAxis::GetAllAxisX(axes, pDb)`
3. если пусто → обход **model space**: у `vCSEntity` взять `GetOIdAxis()`, либо сам объект `vCSAxis`

В командной строке печатается: `[PIPE_INSUL] axes found: N`.

## Почему список может быть пуст

Типичные причины:

1. **Словарь `"Axis"` пуст / GetAllAxis вернул 0** — модуль ViperCSObj не отдал оси (транзакции, document lock, другая БД).
2. **Оси нашлись, но зоны отфильтрованы** — в `BuildZonesForAxis` зона добавляется только если есть подсегменты (`pipeIds.length() > 0`). Ось без труб в списке не появится.
3. **Обход model space не видит axis-entity** — оси живут в словаре; в MS трубы ссылаются на ось через `GetOIdAxis()`.

## Рекомендуемый надёжный сбор (для следующего фикса)

```cpp
// 1) Официальный API
vCSAxis::GetAllAxis(axes, pDb);
if (axes.isEmpty()) vCSAxis::GetAllAxisX(axes, pDb);

// 2) Прямо словарь NOD → "Axis"
AcDbDictionary* pNod = nullptr;
pDb->getNamedObjectsDictionary(pNod, AcDb::kForRead);
AcDbObjectId idAxisDict;
if (pNod && pNod->getAt(VCSSTR_DICTNAME_AXIS, idAxisDict) == Acad::eOk) {
    AcDbDictionary* pAxisDict = nullptr;
    if (acdbOpenObject(pAxisDict, idAxisDict, AcDb::kForRead) == Acad::eOk) {
        for (std::unique_ptr<AcDbDictionaryIterator> it(pAxisDict->newIterator());
             it && !it->done(); it->next())
            AppendUnique(axes, it->objectId());
        pAxisDict->close();
    }
}
if (pNod) pNod->close();

// 3) Альтернатива SDK
vCSObject2::GetAllObjectsX(VCSSTR_DICTNAME_AXIS, axes, pDb);

// 4) Fallback: уникальные GetOIdAxis() у труб/подсегментов в model space
```

После сбора осей: если сегменты пусты — fallback `CollectAxisSegments` (скан `vCSSubSegment` по `GetOIdAxis`), и **показывать ось в списке даже с 0 труб** (чтобы было видно имя).

## Resolve одной оси по клику

Уже реализовано в `HelloNRX.cpp` → `ResolvePipeAxisId`:

1. `CVCSUtils::getAxis`
2. `vCSTools::GetOIdAxisByViperEnt`
3. `vCSEntity::GetOIdAxis` / если кликнули саму ось — её id
