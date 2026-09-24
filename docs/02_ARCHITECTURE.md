# 02 — Arquitectura

## 2.1 Las cinco macros

Casi todo el proyecto se declara con macros. Entenderlas es entender el código.

### `MAKE_SIGNATURE` — firmas de bytes
`src/Utils/SignatureManager/SignatureManager.h`, usadas en `src/SDK/L4D2/Signatures.h`.

```cpp
MAKE_SIGNATURE(INetChannel_SendNetMsg, "engine.dll", "55 8B EC 56 8B F1 8D 8E ? ? ? ?", 0);
// uso: Signatures::INetChannel_SendNetMsg.Get()
```

El último parámetro es un desplazamiento en bytes desde el match (puede ser **negativo**, p.ej.
`CMatSystemSurface_StartDrawing` usa `-27` porque la firma matchea en el medio de la función).
`?` = comodín de un byte.

### `MAKE_INTERFACE_VERSION` / `MAKE_INTERFACE_SIGNATURE` / `MAKE_INTERFACE_NULL`
`src/Utils/InterfaceManager/InterfaceManager.h`.

```cpp
MAKE_INTERFACE_VERSION(IBaseClientDLL, BaseClientDLL, "client.dll", "VClient016");
MAKE_INTERFACE_SIGNATURE(CClientState, ClientState, "engine.dll", "A1 ? ? ? ? 83 C0 ? C3", 1, 2);
MAKE_INTERFACE_NULL(IViewRender, ViewRender);   // se llena a mano desde un hook
```

Declaran `I::Nombre` y se auto-registran en `U::InterfaceManager` mediante un objeto estático a
nivel de archivo. `InitializeAllInterfaces()` los resuelve todos de una.
En la variante por firma, los dos últimos parámetros son `offset` y `dereferenceCount`.

### `MAKE_HOOK` — hooks
`src/Utils/HookManager/HookManager.h`, sobre MinHook.

```cpp
MAKE_HOOK(
    CL_Move,
    Memory::ResolveFn("CL_Move", "engine.dll", "55 8B EC 81 EC ...", 0x07D120),
    void, __cdecl, float flAccumulatedExtraSamples, int bFinalTick)
{
    // ...
    CALL_ORIGINAL(flAccumulatedExtraSamples, bFinalTick);
}
```

Genera `Hooks::CL_Move::{Init, Hook, fn, Func}` y auto-registra el `Init` en `U::HookManager`.
`CALL_ORIGINAL` es azúcar para `Hook.Original<fn>()`.

La dirección puede ser:
- `Memory::ResolveFn(nombre, modulo, firma, rvaFallback)` — para funciones sueltas
- `Memory::GetVFunc(I::Interfaz, indice)` — para métodos virtuales
- `Signatures::Algo.Get()` — para una firma ya declarada

**`CHook::Create` ignora direcciones nulas a propósito.** Antes, hookear un `nullptr` era un no-op
silencioso de MinHook y la feature quedaba muerta sin ninguna señal. Ahora se saltea explícitamente
y `Memory::ResolveFn` ya dejó en el log **por qué** dio nulo.

### `Memory::ResolveFn` — el resolvedor endurecido
`src/Utils/Memory/Memory.h`. Es la pieza que hace que una actualización del juego sea
diagnosticable en vez de un misterio:

1. Intenta la firma.
2. Si falla, cae a `base_del_modulo + rvaFallback` (el último offset conocido bueno) **si** el
   primer byte todavía coincide con el primer opcode de la firma. Si no coincide, rechaza
   (`FB-REJECT`) en vez de saltar a basura.
3. **Siempre** loguea el resultado: `OK` + RVA resuelta / `FALLBACK` / `FAIL`.

`rvaFallback == 0` significa "todavía no hay offset conocido" — se completa leyendo un log de una
inyección buena.

### `NETVAR` — campos de red por nombre
`src/SDK/L4D2/NetVars/NetVars.h`.

```cpp
NETVAR(m_iHealth, int, "CTerrorPlayer", "m_iHealth");
// uso: pPlayer->m_iHealth() = 100;   // devuelve REFERENCIA, se puede escribir
```

Resuelve el offset una sola vez (`static int nOffset`) recorriendo las `RecvTable` del cliente y lo
cachea. Es el mecanismo **preferido** frente a offsets crudos: sobrevive a parches del juego.
Los offsets crudos solo se usan cuando el campo no es un netvar (estado interno del motor).

### `MAKE_SINGLETON` / `MAKE_SINGLETON_SCOPED`
`src/Utils/Singleton/Singleton.h`. Genera el puntero global. `_SCOPED` lo mete en un namespace
(`U::`, `H::`, `F::`).

---

## 2.2 Ciclo de vida

`src/App/App.cpp::Start()`, en este orden **y el orden importa**:

```
1. U::SignatureManager->InitializeAllSignatures()
2. U::BytePatcheManager->InitializeAllBytePatches()
3. U::InterfaceManager->InitializeAllInterfaces()
4. H::Draw->UpdateScreenSize()  +  H::Fonts->Reload()
5. (si ya está en partida) H::Entities->UpdateModelIndexes()
6. CrashHandler::Install()          <- ANTES de los hooks: a partir de acá un fallo deja
                                       reporte en ByxStorm/crashes en vez de cerrar mudo
7. U::HookManager->InitializeAllHooks()
8. Hooks::WINAPI_WndProc::Init()
9. EnemyChat::Init() / KillNotifier::Init() / HitSounds::Init()   <- listeners de eventos
10. Features::ConfigRegistry::RegisterAll()    <- REGISTRAR
11. Config::LoadNamed("default")               <- luego CARGAR
12. Features::Lua::AutoStart()                 <- último: cada script restaura sus propios
                                                  controles desde su archivo de settings
```

**Por qué 10 va antes que 11:** `Config::Load` recorre las variables **registradas** y busca su
clave en el JSON. Lo que no esté registrado se saltea sin decir nada. Registrar después de cargar
= la config no se aplica y no hay error.

`Shutdown()` deshace todo en orden inverso, y además:
- `Config::SaveNamed("default")` — los ajustes de la sesión sobreviven a la descarga sin tener que
  apretar Guardar. Las configs con nombre **solo** se escriben con el botón explícito.
- `Effects::Restore()` — los cvars de gibs son estado **global del juego**; sin esto el jugador se
  queda sin pedazos de props aunque desinyecte.
- `Thirdperson::Restore()` — el flag de cámara vive dentro de `CInput`, no en nuestro DLL.
- Destrucción del contexto ImGui + shutdown de los backends DX9/Win32 (ver `00_OVERVIEW.md`).

---

## 2.3 Los hooks — 18 vía `MAKE_HOOK` + `WndProc` aparte

Los 18 primeros usan `MAKE_HOOK` (MinHook) y viven uno por archivo en `src/App/Hooks/`.
El `WndProc` es el único que no pasa por MinHook: se instala con `SetWindowLongPtr` y se restaura a
mano en `Shutdown`.

| Hook | Objetivo | Cómo se resuelve |
|---|---|---|
| `CL_Move` | `engine.dll` | firma + RVA `0x07D120` |
| `CL_SendMove` | `engine.dll` | firma + RVA `0x7CEC0` |
| `INetChannel_SendNetMsg` | `engine.dll` | `Signatures::` |
| `Copy_Command` | `client.dll` | firma + RVA `0x10B410` |
| `CBasePlayer_CalcPlayerView` | `client.dll` | firma + RVA `0x020750` |
| `Set_Host` | `client.dll` | firma + RVA `0x145010` |
| `Draw_Effect` | `client.dll` | firma + RVA `0x15C1F0` |
| `CBaseHudChat_ChatPrintf` | `client.dll` | `EnemyChat::ResolveChatPrintfOnce()` (ver R7) |
| `CViewRender_RenderView` | `client.dll` | `Signatures::` |
| `IBaseClientDLL_FrameStageNotify` | vtable | `BaseClientDLL` idx **34** |
| `IBaseClientDLL_LevelShutdown` | vtable | `BaseClientDLL` idx **6** |
| `IEngineVGuiInternal_Paint` | vtable | `EngineVGui` idx **14** |
| `ISurface_LockCursor` | vtable | `MatSystemSurface` idx **59** |
| `ISurface_OnScreenSizeChanged` | vtable | `MatSystemSurface` idx **108** |
| `IVModelRender_DrawModelEx` | vtable | `ModelRender` idx **16** |
| `IVModelRender_DrawModelExecute` | vtable | `ModelRender` idx **19** |
| `EndScene` | vtable | `D3D9Device` idx **42** |
| `IDirect3DDevice9_Reset` | vtable | `D3D9Device` idx **16** |
| `IClientMode_OverrideView` | vtable | `ClientModeShared` idx **19** |
| `WINAPI_WndProc` | `SetWindowLongPtr` | no es MinHook |

> `IDirect3DDevice9_Reset` **es obligatorio**. Su ausencia causó el freeze con ReShade: sin liberar
> los objetos `D3DPOOL_DEFAULT` de ImGui, todo `Reset` del device fallaba y el juego se congelaba.

---

## 2.4 Pipeline de ejecución

### `Copy_Command` — una vez por comando de usuario (orden real y deliberado)

```
1. SpeedHack::bExtraTick -> quita IN_ATTACK|IN_ATTACK2  (SpeedHack es solo movimiento)
2. RapidFire::OnCommand(pCmd)          <- algoritmo CRapidFire, pacea IN_ATTACK
3. Reference::OnCommand(pCmd)          <- par A/B de referencia, escribe después a propósito
4. RapidFireCustom -> weapon+2400 = 0.f   (cooldown a cero)
5. AutoDuck -> Bunnyhop -> AutoStrafe
6. FreeLook              <- ANTES del aimbot: si el aimbot engancha, sus ángulos ganan
7. AutoShove             <- ANTES del aimbot: shove y disparo en el mismo comando se pelean;
                            el aimbot lee AutoShove::bShovedThisTick y se abstiene
8. Aimbot -> AimInfected  (AimInfected después: gana pCmd->viewangles si ambos activan)
9. ChatSpam, NameStealer, DamageIndicator, MovementRecorder
10. RollExploit, NoFallDamage, AirStuck, NameBug
11. DisableInterp, AutoPistol, AutoClicker
12. Lua::OnCreateMove(pCmd)   <- ÚLTIMO de los que construyen el comando: un script gana
                                 sobre cualquier feature (es la intención más específica)
13. NoSpread::Apply(...)  si IN_ATTACK  (después de AutoPistol para ver los botones finales)
14. CALL_ORIGINAL
```

### `CL_Move` — una vez por frame real; el corazón de los exploits

```
1. Update() de cada exploit (cada uno maneja su propio keybind, no hay dispatcher compartido)
2. RapidFire::BeginFrame() / Reference::BeginFrame()
3. Bloque Bleed/Choke: escribe clientstate+19016 y hace UN spoof compartido de ack/choked
   -> va ANTES del CALL_ORIGINAL real para que el paquete real también vea el valor inflado
4. Fakelag: sube m_fClearTime a 1e9 para cerrar el gate de envío
5. CALL_ORIGINAL(...)          <- el único envío que el motor espera este frame
6. Restaura m_fClearTime
7. Calcula extras por bucket: SpeedHack | CustomSpeedHack | RapidFire | Reference
8. Loop de llamadas extra: por cada iteración fuerza los 3 gates, llama a CL_Move otra vez con
   bFinalTick=1, restaura ack/choked solo si nadie los cambió, y corre CL_RunPrediction()
9. Detección de "backlog wrap": si curtime salta hacia atrás >1s, corta la ráfaga
```

Los **tres gates** que hay que forzar para que una llamada extra a `CL_Move` transmita de verdad
(los tres descubiertos con IDA, cada uno explicando un síntoma distinto):

| Gate | Dónde | Síntoma si no se fuerza |
|---|---|---|
| `CNetChannel::m_fClearTime` (+0xB8) | netchannel | `CanPacket()` falso: ninguna llamada extra llega al servidor |
| `clientstate + 112` (double) | client state | retransmite el **mismo** comando; `curtime` plano |
| `bFinalTick` (parámetro) | argumento | la ráfaga entera cae en `SetChoked()`: outseq sube, `curtime` congelado |

Y `CL_RunPrediction()` después de cada llamada extra: sin eso los comandos llegan (el movimiento
funciona) pero el estado de disparo predicho nunca se re-simula, así que los tiros extra **parecen**
salir y no registran.

### `IEngineVGuiInternal_Paint` — dibujo del juego (VGUI/surface)

Solo si `mode & PAINT_UIPANELS`, en partida y sin pantalla de carga:

```
CALL_ORIGINAL
UpdateW2SMatrix()
MatSystemSurface->StartDrawing()
    ESP::Render() -> ItemESP::Render() -> OffScreen::Render()
    DamageIndicator::Paint() -> VirtualKeyboard::Paint() -> MovementRecorder::Paint()
    Lua::OnPaint()          <- byx.draw_* de surface solo vale acá dentro
    ViewModel::Apply()
    círculo de FOV del aimbot
MatSystemSurface->FinishDrawing()
```

### `EndScene` — dibujo de ImGui

Pipeline **separado y no intercambiable** con el anterior. `CMenuImgui::Run()` corre acá.
Todo lo que necesite ser una ventana ImGui real (movible, redimensionable) va acá:
Network Info, Watchers, el menú, los overlays de scripts encolados con `byx.draw_*`
(que se dibujan en el **background drawlist**, así el menú siempre queda encima).

> **Las dos pipelines de dibujo no se mezclan.** La surface de VGUI no puede hospedar ventanas
> ImGui, y el drawlist de ImGui solo existe dentro del ciclo `NewFrame`/`Render` de `EndScene`.
> Elegir la equivocada es un error recurrente.

---

## 2.5 Sistema de configuración

`src/Utils/Config/Config.h` + `src/App/Features/ConfigRegistry.cpp`.

Dos formas de declarar algo persistente:

```cpp
CFGVAR(bAlgo, false);                    // declara una variable NUEVA y la registra
Config::Register("clave.json", &var);    // registra una variable YA EXISTENTE
```

En la práctica **casi todo usa `Register`**, porque las features guardan sus ajustes como globales
`inline` o como miembros de un struct de config, declarados mucho antes de que existiera este
sistema. `ConfigRegistry.cpp` es el **único** lugar donde eso se conecta:

```cpp
#define CFG(name, var) Config::Register(name, &var)
CFG("exp.speedhack.nValue", Features::Exploits::SpeedHack::nValue);
```

Las features grandes tienen su propio registrador (`Features::Aimbot::RegisterConfig()`,
`ESP::RegisterConfig()`, `Glow`, `Chams`, `Lua`, `Binds`, `Menu`) porque tienen decenas de campos.

Tipos soportados: `bool`, `int`, `float`, `Color_t`, `std::string`. Nada más.

### Tres trampas del sistema de config

1. **El nombre es formato en disco.** Renombrar una clave huerfaniza el valor guardado en silencio
   (`Load` saltea las claves que no encuentra). Tratalos como estables.
2. **`Register` guarda el PUNTERO al nombre, no una copia.** Usá siempre literales de cadena. Un
   intento de construir claves en runtime dentro de un `std::vector<std::string>` se rompió: claves
   de 15 caracteres caben en el buffer SSO de MSVC, o sea que viven *dentro* del objeto string, y
   el `c_str()` registrado queda colgando en cuanto el vector realoca.
3. **No se registra el estado de runtime.** Flags de "tecla apretada", caches por frame y toggles de
   diagnóstico que loguean por frame quedan fuera a propósito: restaurar un "activo" al arrancar
   dejaría un exploit prendido sin que nadie apriete nada.

`Load` tiene doble guarda try/catch: una global (config truncada/corrupta no crashea) y una por
campo (una entrada vieja con otro tipo no aborta la carga entera).

---

## 2.6 Scripting Lua

- Motor: **LuaJIT** (2.x, incluidas las rolling releases de 2.1).
- `lua51.dll` de **32 bits** se resuelve **en runtime** (`GetModuleHandle` → `LoadLibrary`), no se
  linkea. `src/App/Features/Lua/LuaShim.cpp` resuelve cada símbolo por nombre y falla con un
  mensaje claro si falta alguno. Sin el DLL, la pestaña Lua avisa y el resto del cheat funciona.
- Scripts en `<juego>/ByxStorm/scripts/*.lua`. Ajustes por script en `scripts/settings/<n>.json`.
- **Los scripts NO están aislados**: tienen `io` y `os` completos.
- Callbacks: `on_load`, `on_unload`, `on_create_move(cmd)`, `on_paint`, `on_menu`,
  `on_frame_stage(stage)`, `on_level_shutdown`, `on_event(name)`.
- API completa documentada en `_reference/lua/API.md`, con 7 ejemplos en `_reference/lua/examples/`.

---

## 2.7 Otros subsistemas

| Subsistema | Archivo | Nota |
|---|---|---|
| Byte patches | `Utils/BytePatchManager/` | aplica/restaura parches de bytes; se restauran en `Shutdown` |
| Crash handler | `Utils/CrashHandler/` | vuelca a `<juego>/ByxStorm/crashes/` |
| Log de debug | `Utils/DebugLog.h` | `Debug::Log`, archivo reescrito en cada inyección, `fflush` por línea |
| Dibujo VGUI | `SDK/Helpers/Draw/` | `H::Draw`, W2S, primitivas, texto |
| Entidades | `SDK/Helpers/Entities/` | `H::Entities->GetLocal()`, cache de índices de modelo |
| Fuentes | `SDK/Helpers/Fonts/` | `H::Fonts->Reload()` |
| Input | `SDK/Helpers/Input/` | `H::Input->IsGameFocused()` |
