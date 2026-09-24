# 07 — Dependencias, herramientas y material externo

## 7.1 Librerías vendorizadas (viven dentro de `src/`, se compilan con el proyecto)

| Librería | Versión | Ubicación | Para qué |
|---|---|---|---|
| **Dear ImGui** | `1.92.0 WIP` | `src/App/Features/Menu/ImGui/` | menú y overlays |
| ↳ backend DX9 | — | `.../ImGui/dx9/imgui_impl_dx9.{h,cpp}` | render |
| ↳ backend Win32 | — | `.../ImGui/dx9/imgui_impl_win32.{h,cpp}` | input |
| **MinHook** | vendorizado con `hde32`/`hde64` | `src/Utils/HookManager/MinHook/` | motor de hooks (trampolín) |
| **nlohmann/json** | `3.9.1` (single header) | `src/Utils/Config/json.hpp` | serializar configs |
| **SDK de Source (L4D2)** | adaptado a mano | `src/SDK/L4D2/` (~130 headers) | interfaces del motor |

> Los únicos warnings del build salen de `json.hpp` y de `ISurface.h:97`. Son inofensivos y
> conocidos — ver `docs/01_BUILD.md`.

### Notas sobre ImGui
- El atlas se hornea a **1.5×** y se le mergea **Segoe UI Symbol** (para los iconos vectoriales de la
  barra lateral).
- `LoadFonts` se llama **entre** `CreateContext` y `ImGui_ImplDX9_Init`, en
  `src/App/Hooks/EndScene.cpp`. El orden importa.
- Sin rangos explícitos, ImGui usa `0x20-0xFF` por defecto: cubre ñ, ·, y todas las vocales
  acentuadas. Las etiquetas del menú pueden llevar español real.
- Detalles de comportamiento verificados de esta versión concreta: `IsItemHovered()` después de
  `EndChild()` funciona (1.92.0 WIP setea `ImGuiItemStatusFlags_HoveredWindow` en `EndChild`);
  ctrl+click en un slider abre un input temporal **sembrado** con el valor.

## 7.2 Dependencias de runtime (no se linkean)

| Qué | Cómo se obtiene | Obligatorio |
|---|---|---|
| **LuaJIT** (`lua51.dll`, **32 bits**) | se resuelve en runtime con `GetModuleHandle`→`LoadLibrary` desde la carpeta del juego | **No.** Sin él, la pestaña Lua avisa y el resto funciona |
| **Direct3D 9** | del sistema / del juego | sí (`d3d9.h`, hook de `EndScene`) |

### LuaJIT
- Fuente completo en `LuaJIT/` (raíz del proyecto) y **otra copia** en `_reference/lua/LuaJIT/`.
- `_reference/lua/build_luajit.bat` construye el `lua51.dll` de 32 bits.
- `_reference/lua/check_lua51.ps1` verifica que un `lua51.dll` dado sea válido y de 32 bits.
- `src/App/Features/Lua/LuaShim.cpp` resuelve cada símbolo por nombre y da un error claro si falta.
- Sirve **cualquier LuaJIT 2.x**, incluidas las rolling releases de 2.1. La pestaña muestra
  `jit.version`.

> Las dos copias de LuaJIT (raíz y `_reference/lua/`) son redundantes. Si molestan, se puede
> consolidar — pero ninguna se compila con el proyecto, así que no afecta al build.

## 7.3 Herramientas de desarrollo

| Herramienta | Versión / ruta | Para qué |
|---|---|---|
| **Visual Studio** | 18 Community — `C:\Program Files\Microsoft Visual Studio\18\Community\` | MSBuild + ClangCL |
| **ClangCL** | `...\VC\Tools\Llvm\x64\bin\clang-cl.exe` | el compilador real |
| **IDA Professional** | `9.3` — `C:\Program Files\IDA Professional 9.3\` | reversear `engine.dll` / `client.dll` |
| **ida-pro-mcp** | plugin en `%APPDATA%\Hex-Rays\IDA Pro\plugins\mcp_plugin.py`, servidor JSON-RPC en **`http://127.0.0.1:13337/mcp`** | darle a un agente acceso programático a IDA |
| **Python 3** | con `pefile` y `capstone` disponibles | parsear PE, desensamblar, triaje de minidumps |
| Inyector x86 | cualquiera | meter el DLL en `left4dead2.exe` |

### El MCP de IDA — la herramienta más importante del proyecto

Casi todo lo que este proyecto sabe salió de desensamblar el binario real. El plugin expone IDA por
JSON-RPC. Chequeo rápido de que está vivo:

```bash
curl -s -X POST http://127.0.0.1:13337/mcp -H "Content-Type: application/json" -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"server_health","arguments":{}},"id":1}'
```

Herramientas usadas históricamente: `server_health`, `imports_query`, `find_regex`,
`list_functions`, decompilación por dirección, `xrefs_to`.

**Si la herramienta nueva soporta MCP** (Cursor sí, vía `.cursor/mcp.json`), conectala —
sin esto el proyecto pierde su fuente principal de verdad. Ver `docs/09_MIGRACION.md`.

Alternativa sin MCP: parsear el PE en disco con Python (`pefile` + `capstone`) y buscar patrones en
`.text`. Ya se hizo así para verificar que la firma de `ChatPrintf` matcheara **exactamente una vez**
en `left4dead2/bin/client.dll`.

## 7.4 Material de referencia (`_reference/`, no se compila)

### `_reference/analisis_externo/`
Análisis de un volcado de fuente de un cheat comercial de Source (1354 archivos), realizado en el
proyecto. Tres documentos:

- `01_nucleo_l4d2.md` — hooks, orden de create_move, exploits, aimbot, auto-bash, tickbase, gestor
  de paquetes, structs de entidad, inventario de features, tabla de brechas contra ByxStorm.
- `02_visuales_framework_loader.md` — recetas de chams/glow, filtro de traza, autostrafe, ESP,
  playerlist, protecciones, loader.
- **`l4d2_netvars_offsets.md` — 1616 netvars + 23 offsets base / índices de vtable.
  El artefacto más valioso del proyecto.** Verificado contra el mismo build de L4D2 en cuatro
  valores independientes. **Consultalo antes de reversear un offset a mano.**

De ese análisis ya se portó: `CTraceFilterAim` y el arreglo de multi-punto/exigir-hitgroup en Rage.

Técnicas identificadas, en orden de valor. Estado al 2026-08-13:
1. ~~SpeedHack por `*move_ret_addr -= 5`~~ — **PORTADO** 2026-08-13: rebobinado de return address en el
   SpeedHack (opt-in, `SpeedHack::bRewind`), con verificación del call site (`E8` = `call rel32`) y
   fallback al loop clásico. Pendiente de verificar en servidor dedicado.
2. ~~`localplayer->tick_base()--` en comandos sintéticos~~ — **PORTADO** 2026-08-13 en `CL_Move.cpp`
   (después de cada `CL_RunPrediction` en el loop y en la re-entrada del rebobinado).
3. ~~`net->choked_packets -= extra_cmd` en el reemplazo de SendMove~~ — **PORTADO** (ya estaba en
   `SendMoveOverride.cpp`; 2026-08-13 se agregó el guard `choked_packets > 0` de la referencia).
4. `blacklist_time = time + latency_in + latency_out + 2 ticks` en el aimbot: no re-apuntar a algo
   ya disparado. Es lo opuesto al Sticky Target actual y reparte mejor el daño.
5. Filtro de auto-bash por nombre de secuencia (`"Shoved_"`, `"Idle_"`, `"Walk_Neutral"`) y
   **el Charger no es shoveable** (el AutoShove actual no lo contempla).
6. `can_use_fast_path() = false` (= `m_flFrozen + 0xC`) para forzar `DrawModelExecute`.
7. Quitar `FL_FROZEN` dentro del hook de decodificación de props.

Extra portado 2026-08-13 (de `ApplyExtraScaling` del RapidFire de referencia): multiplicador
`RapidFire::nInterpolate` que estira `m_flSimulationTime` por `1 + extraCommands * factor`.

**No copiar de ahí:** su build sin CRT/STL con arquitectura de generador; su DRM
(Themida/RTP/syscalls); el truco del `command_number` mágico para escopetas (solo funciona porque
reemplazan todo el buffer de usercmd — este proyecto ya lo probó y falla exactamente por eso).

### `_reference/lua/`
- `API.md` — documentación completa de la API de Lua expuesta a los scripts.
- `examples/` — 7 scripts: `drawdemo`, `esp`, `hud`, `killfeed`, `showcase`, `test`, `tracers`.
- `build_luajit.bat`, `check_lua51.ps1`.

## 7.5 Recursos del juego (VPK)

Los nombres de materiales y de partículas que usan Chams y Effects **no se adivinaron**: se extrajo
el contenido de `pak01_dir.vpk` de esta instalación y se leyó su tabla de cadenas, para tener los
nombres reales.

**Eso fue una herramienta de análisis puntual, no vive en el proyecto.** Lo que quedó son los
resultados, volcados como comentarios en:
- `src/App/Features/Visuals/Chams/Chams.h` — materiales disponibles en este install
- `src/App/Features/Effects/Effects.h` — nombres reales de `particles/tank_fx.pcf` y compañía

Si necesitás nombres nuevos, hay que volver a abrir el VPK. Usá siempre esta ruta antes que
inventar un nombre de material: un `FindMaterial` con nombre inexistente devuelve un material que
falla en silencio.

## 7.6 Lo que NO existe (y quizá deberías extrañar)

- **No hay git.** Ver R1.
- **No hay tests automatizados** más allá del harness headless de menú (que además vivía en un
  scratchpad de sesión y **se perdió** — ver `docs/01_BUILD.md` para reconstruirlo).
- **No hay CI.**
- **No hay debugger instalado** (ni WinDbg ni cdb). El triaje de crashes se hace parseando el
  `.mdmp` a mano con Python.
