# 00 — Panorama general

## Qué es ByxStorm

Un DLL de C++ que se inyecta en el proceso de **Left 4 Dead 2** (motor Source, build de 32 bits) y
modifica el comportamiento del cliente hookeando funciones de `engine.dll`, `client.dll` y
`vguimatsurface.dll`, además de la cadena de render de Direct3D 9.

- **Nombre del binario:** `ByxStorm.dll`
- **Arquitectura obligatoria:** x86 (32 bits). El juego es de 32 bits; un DLL x64 no inyecta.
- **Configuración de release:** `Release | Win32` (el `.sln` la llama `x86`).
- **Estándar:** C++20 (`stdcpp20`), toolset **ClangCL**, charset MultiByte.
- **Tipo de proyecto:** `DynamicLibrary`.
- **Antigüedad:** desarrollo continuo desde ~mayo 2026; ~338 archivos fuente.

## Cómo entra y cómo sale

`src/DllMain.cpp`:

1. `DllMain` en `DLL_PROCESS_ATTACH` lanza `MainThread` y cachea `Client_Module` / `Engine_Module`.
2. `MainThread` espera a que exista `mss32.dll` (indicador de que el motor terminó de cargar) y
   recién ahí llama a `App->Start()`.
3. Queda en loop esperando **F11**; al presionarla llama a `App->Shutdown()` y hace
   `FreeLibraryAndExitThread`.

El `Shutdown` es un desarme ordenado y **importa**: guarda `default.json`, restaura cvars globales
de efectos, vuelve a primera persona, restaura los byte patches, saca los hooks, restaura el
`WndProc` y — clave — destruye el contexto de ImGui y sus buffers `D3DPOOL_DEFAULT`. Si esos
buffers no se liberan, cualquier `Reset` posterior del device D3D9 falla **para siempre** y el juego
queda condenado a congelarse aunque desinyectes.

## Mapa de carpetas

```
C:\Byx_storm-L4D2\
├── README.md                  <- puerta de entrada para humanos
├── AGENTS.md                  <- núcleo de instrucciones para el agente
├── .cursorrules               <- espejo para Cursor
├── .cursor/rules/byxstorm.mdc <- regla always-apply de Cursor
├── docs/                      <- esta documentación
│   └── _historico/            <- los .txt viejos de la raíz, con su estado de validez
├── ByxStorm.sln
├── ByxStorm.vcxproj           <- lista de archivos a compilar (hay que mantenerla a mano)
├── ByxStorm.vcxproj.filters   <- árbol visible en Visual Studio (mantener en sync)
├── Release/ByxStorm.dll       <- salida
├── _backups/                  <- snapshots pre-cambio-grande (7 al día de hoy)
├── _reference/                <- material de análisis, NO se compila
│   ├── analisis_externo/      <- 3 docs; incluye tabla de 1616 netvars verificados
│   └── lua/                   <- API.md, ejemplos .lua, build de LuaJIT
├── LuaJIT/                    <- fuente de LuaJIT (para generar lua51.dll de 32 bits)
├── .vs/                       <- cache de Visual Studio. REGENERABLE, pesa GB. Ver §Basura
├── ByxStorm/                   <- intermedios de un build Debug OBSOLETO. Ver §Basura
└── src/
    ├── DllMain.cpp
    ├── App/
    │   ├── App.cpp/.h         <- Start() / Shutdown()
    │   ├── Hooks/             <- 18 hooks, uno por archivo
    │   └── Features/
    │       ├── ConfigRegistry.cpp  <- ÚNICO lugar donde se registra qué persiste
    │       ├── Aimbot/  Camera/  ESP/  Effects/  Exploits/  Lua/
    │       ├── Menu/    (+ Menu/ImGui/ vendorizado)
    │       ├── Misc/    MiscTools/  Movement/  Visuals/  Weapon/
    ├── SDK/
    │   ├── SDK.h              <- include maestro + namespace G::
    │   ├── L4D2/              <- ~130 headers del SDK de Source, adaptados a L4D2
    │   │   ├── Signatures.h   <- firmas con MAKE_SIGNATURE
    │   │   ├── NetVars/       <- resolución de netvars por nombre
    │   │   └── tf_shareddefs.h<- enum de class IDs
    │   └── Helpers/           <- Draw, Entities, Fonts, Input
    ├── Storm/                 <- LEGADO. HooksStorm.cpp es un comentario gigante (marcado
    │                             <None> en el .vcxproj). UtilsStorm.hpp SÍ se usa.
    └── Utils/                 <- infraestructura propia
        ├── HookManager/       (+ MinHook vendorizado)
        ├── InterfaceManager/  SignatureManager/  BytePatchManager/
        ├── Config/            (+ json.hpp de nlohmann)
        ├── CrashHandler/  DebugLog.h  Memory/  Math/  Vector/  Color/  Hash/  Singleton/
```

## Convenciones de namespace

| Prefijo | Significado | Ejemplo |
|---|---|---|
| `I::` | interfaz del juego resuelta al iniciar | `I::EngineClient`, `I::MatSystemSurface` |
| `H::` | helper propio (singleton) | `H::Draw`, `H::Entities`, `H::Fonts`, `H::Input` |
| `U::` | manager de infraestructura (singleton) | `U::HookManager`, `U::SignatureManager` |
| `F::` | feature con singleton | `F::MenuImgui` |
| `G::` | estado global compartido entre hooks | `G::bSilentAngles`, `G::View` |
| `Features::` | namespace de casi todas las features | `Features::Exploits::RapidFire` |
| `Hooks::` | generado por `MAKE_HOOK` | `Hooks::CL_Move::Func` |
| `Signatures::` | generado por `MAKE_SIGNATURE` | `Signatures::CViewRender_RenderView.Get()` |

## Documentos históricos

Los `.txt` que vivían sueltos en la raíz se movieron a **`docs/_historico/`** el 13/08/2026.
No se borró nada; el `README.md` de esa carpeta dice cuál sigue siendo válido y cuál no.

Resumen: `BYX_STORM_PARTE2.txt` conserva valor (su tabla de offsets y class IDs sigue siendo
correcta, re-verificada en `docs/04_OFFSETS.md`); `FEATURES_LIST.txt` está desactualizado.
Ambos ya pasaron por el scrub de atribución (R2) — sus encabezados de sección son temáticos, no de
procedencia. Si los editás, mantenelos así.

## Basura regenerable (no es parte del proyecto)

Dos directorios en la raíz ocupan **~4,9 GB** y no aportan nada:

| Directorio | Tamaño | Qué es |
|---|---|---|
| `.vs/` | ~4,8 GB | cache de IntelliSense de Visual Studio. Se regenera solo al abrir el `.sln` |
| `ByxStorm/` | ~98 MB | intermedios (`.obj`) de un build **Debug obsoleto y fallido**. Contiene objetos de módulos que ya no existen (`Aimbot_Rage.obj`, `Aimbot_Legit.obj` — borrados en la reescritura del aimbot del 15/07) y un marcador `unsuccessfulbuild`. Nada del build actual pasa por acá: `Release\|Win32` escribe en `Release/` |

Se pueden borrar con Visual Studio **cerrado**:

```powershell
Remove-Item -Recurse -Force "C:\Byx_storm-L4D2\.vs", "C:\Byx_storm-L4D2\ByxStorm"
```

> El nombre `ByxStorm/` confunde: el directorio de **runtime** (configs, scripts, crashes) también
> se llama `ByxStorm` pero vive en la carpeta del juego, no acá.
