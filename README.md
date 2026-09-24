# ByxStorm

DLL de C++20 (x86) que se inyecta en **Left 4 Dead 2**. Hookea `engine.dll`, `client.dll`,
`vguimatsurface.dll` y Direct3D 9 para agregar aimbot, ESP, exploits de red, visuales, scripting
Lua y un menú ImGui.

**Salida:** `Release/ByxStorm.dll` · **Menú:** `INSERT` · **Descargar el DLL:** `F11`

---

## Empezá acá

| Sos… | Leé |
|---|---|
| **Un agente de IA** (Cursor, DeepSeek, Claude Code, opencode…) | **[`AGENTS.md`](AGENTS.md)** — es tu archivo. Cargalo siempre; tiene el índice del resto |
| Alguien que va a **configurar Cursor** | [`docs/10_CURSOR.md`](docs/10_CURSOR.md) — casi todo ya está hecho |
| Alguien que va a **estrenar un agente nuevo** en este proyecto | [`docs/PROMPT_INICIAL.md`](docs/PROMPT_INICIAL.md) — prompt listo para pegar |
| Una persona que vuelve al proyecto | Esta página, después [`docs/08_ESTADO.md`](docs/08_ESTADO.md) para ver dónde quedó todo |
| Alguien que quiere entender el código | [`docs/02_ARCHITECTURE.md`](docs/02_ARCHITECTURE.md) |
| Alguien que está debuggeando algo raro | [`docs/06_TRAPS.md`](docs/06_TRAPS.md) — ~30 trampas conocidas con síntoma y causa |

---

## Compilar

Desde **PowerShell** (Git Bash mangla los switches de MSBuild — ver `docs/01_BUILD.md`):

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Byx_storm-L4D2\ByxStorm.sln" /t:Build /p:Configuration=Release /p:Platform=x86 /m /v:minimal
```

Éxito = exit code 0 **y** la línea final `-> C:\Byx_storm-L4D2\Release\ByxStorm.dll`.

Después: inyectar el DLL en `left4dead2.exe` con cualquier inyector x86 y leer
`<carpeta del juego>/ByxStorm_debug.txt`.

---

## Documentación

```
AGENTS.md            núcleo de reglas e instrucciones (siempre cargado)
docs/
  00_OVERVIEW        qué es, ciclo de vida, mapa de carpetas
  01_BUILD           toolchain, comandos, agregar archivos al .vcxproj
  02_ARCHITECTURE    macros, hooks, pipelines, config, Lua
  03_FEATURES        inventario por pestaña
  04_OFFSETS         offsets, firmas, vtables, class IDs verificados
  05_RULES           las 8 reglas del proyecto, con su porqué
  06_TRAPS           trampas conocidas  <- el de mayor valor
  07_EXTERNALS       dependencias y herramientas
  08_ESTADO          qué está hecho, qué falta, qué sigue
  09_MIGRACION       cambiar de herramienta o de modelo
  _historico/        los .txt viejos, con su estado de validez
```

---

## Las 3 cosas que más rápido te hacen perder tiempo

1. **No hay git.** Hacé backup en `_backups/pre_<motivo>_<fecha>/` antes de cambios grandes.
2. **Un archivo nuevo tiene que ir en `ByxStorm.vcxproj` *y* en `ByxStorm.vcxproj.filters`.**
   No hay globbing: un `.cpp` sin listar no se compila y **no da error**.
3. **Un listen server no valida nada de red.** Los exploits solo se verifican en servidor dedicado.

Las 8 reglas completas están en [`docs/05_RULES.md`](docs/05_RULES.md).

---

## Estructura

```
src/App/Hooks/        18 hooks (MinHook) + WndProc
src/App/Features/     todas las features, una carpeta por feature
src/SDK/L4D2/         SDK de Source adaptado (127 headers) — ojo: tiene defectos conocidos
src/Utils/            hooks, interfaces, firmas, config, crash handler
_reference/           material de análisis, NO se compila
_backups/             snapshots pre-cambio-grande
```
