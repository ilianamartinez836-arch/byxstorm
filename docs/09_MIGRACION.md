# 09 — Migración a otra herramienta / otro modelo

Guía para seguir el proyecto en Cursor (u otro agente) con DeepSeek u otro modelo.
Pasos en orden.

---

## Paso 0 — Entender qué se pierde y qué no

| Se pierde | Se conserva |
|---|---|
| El archivo de memoria persistente del agente anterior (~75 notas, 680 KB) | El código: `src/` completo |
| El historial de conversaciones | La documentación de `docs/` + `AGENTS.md` (este paquete) |
| Los permisos de herramientas ya aprobados | `_reference/` (análisis y tabla de netvars) |
| El harness headless de menú (vivía en un scratchpad) | `_backups/` (7 snapshots) |

**Lo crítico ya está portado**: reglas, arquitectura, offsets verificados y trampas conocidas están
en `docs/`. Ese es el punto de este paquete.

---

## Paso 1 — Verificar que el paquete está completo

```
C:\Byx_storm-L4D2\
├── AGENTS.md          (7,7 KB)  <- núcleo, siempre cargado
├── .cursorrules       (espejo corto para Cursor)
├── .cursor/rules/byxstorm.mdc   (regla always-apply de Cursor)
└── docs\
    ├── 00_OVERVIEW.md      5,8 KB
    ├── 01_BUILD.md         5,6 KB
    ├── 02_ARCHITECTURE.md 14,6 KB
    ├── 03_FEATURES.md      8,6 KB
    ├── 04_OFFSETS.md      10,1 KB
    ├── 05_RULES.md         9,7 KB
    ├── 06_TRAPS.md        14,2 KB
    ├── 07_EXTERNALS.md     7,3 KB
    ├── 08_ESTADO.md        6,1 KB
    └── 09_MIGRACION.md     (este)
```

Total ≈ 90 KB ≈ **22 000 tokens** si se cargara todo de golpe. Por eso está partido: `AGENTS.md`
solo son ~2 000 tokens y es lo único que conviene tener siempre en contexto.

---

## Paso 2 — Configurar el modelo

### DeepSeek en Cursor

1. **Cursor Settings → Models.**
2. Desactivá los modelos que no vayas a usar.
3. Agregá tu API key de DeepSeek. Cursor permite sobrescribir la Base URL con un endpoint
   compatible con OpenAI: usá `https://api.deepseek.com` y el nombre de modelo que corresponda al
   que contrataste (el catálogo de DeepSeek cambia; mirá su documentación para el ID exacto).
4. Verificá la conexión con el botón de test antes de seguir.

> La UI de Cursor cambia seguido. Si estos nombres de menú no coinciden, el concepto es el mismo:
> *proveedor compatible con OpenAI + base URL + API key + nombre de modelo*.

**Limitación real que hay que tener en cuenta:** el contexto de DeepSeek (≈128 K) es bastante más
chico que con el que se venía trabajando, y en agentes tipo Cursor el contexto efectivo por request
es aún menor. Adaptá el flujo: ver Paso 6.

### Otras herramientas

| Herramienta | Archivo de reglas que lee |
|---|---|
| Cursor | `.cursorrules` (legacy) y `.cursor/rules/*.mdc` (moderno). También lee `AGENTS.md` |
| opencode / Cline / Roo / Aider y la mayoría de los agentes nuevos | `AGENTS.md` |
| Claude Code | `CLAUDE.md` — si volvés, creá un `CLAUDE.md` con una línea: `Ver @AGENTS.md` |

`AGENTS.md` es el formato con más adopción. Los otros dos archivos son espejos delgados que apuntan
a él, no copias — así no se desincronizan.

---

## Paso 3 — Conectar IDA (esto es lo más importante que no es texto)

Casi todo el conocimiento técnico del proyecto salió de desensamblar `engine.dll` y `client.dll`.
Sin acceso a IDA, el agente nuevo solo puede leer código, no descubrir offsets.

1. Abrí IDA Professional 9.3 con `left4dead2/bin/client.dll` (o `engine.dll`).
2. Confirmá que el plugin MCP está cargado (`%APPDATA%\Hex-Rays\IDA Pro\plugins\mcp_plugin.py`).
3. Probá que el servidor responde:

```bash
curl -s -X POST http://127.0.0.1:13337/mcp -H "Content-Type: application/json" -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"server_health","arguments":{}},"id":1}'
```

4. En Cursor, creá `.cursor/mcp.json` apuntando al servidor. Si el plugin expone HTTP directo,
   alcanza con la URL; si expone stdio, apuntá al comando correspondiente. Consultá la
   documentación del plugin: la interfaz la define él, no Cursor.

**Alternativa sin MCP** (funciona en cualquier herramienta con acceso a shell): Python con `pefile`
+ `capstone` sobre el DLL en disco. Ya se usó para verificar que una firma matchee exactamente una
vez en `.text`. Es más lento pero no depende de que IDA esté abierto.

---

## Paso 4 — Aprobar los comandos que el agente va a repetir

Configurá el allowlist de tu herramienta para que no te pregunte cada vez por:

- El comando de MSBuild (`docs/01_BUILD.md`).
- Lecturas dentro de `C:\Program Files\Microsoft Visual Studio\18\Community\`.
- `curl` contra `127.0.0.1:13337`.
- `python3` para parsear PE / minidumps.

---

## Paso 5 — El archivo de memoria histórico

Existen **~75 notas (680 KB)** con el detalle completo de cada investigación: cada sesión de IDA,
cada bug, cada decisión y su porqué. Están en:

```
C:\Users\Anthony\.claude\projects\C--Byx-storm-L4D2\memory\
```

### ⚠️ NO lo copies dentro de `C:\Byx_storm-L4D2\`

Esas notas contienen la **atribución externa completa** — nombres de personas, de cheats y de
archivos compartidos — que la regla R2 prohíbe explícitamente dentro del proyecto. Meterlas en
`docs/` anularía años de scrub.

### Qué hacer

Copiar el archivo a una ubicación privada **fuera** del árbol del proyecto:

```bash
cp -r "/c/Users/Anthony/.claude/projects/C--Byx-storm-L4D2/memory" "/c/ByxStorm_memoria_privada"
```

Y usarlo así:

- **No lo abras en la ventana de Cursor del proyecto.** Abrilo en una ventana aparte cuando
  necesites consultar historia.
- Es un archivo **buscable**, no material de contexto: 680 KB son ~170 000 tokens, no entra en
  ninguna ventana. Buscá por palabra clave y leé la nota puntual.
- Empezá por `MEMORY.md`: es un índice de una línea por nota.
- Si el agente nuevo saca algo de ahí y lo lleva al código o a `docs/`, **tiene que reescribirlo en
  lenguaje neutro** (R2).

---

## Paso 6 — Adaptar el flujo a un contexto más chico

Con menos contexto, el patrón que funcionaba antes ("cargá todo y razoná") deja de funcionar.
Recomendaciones concretas para este proyecto:

1. **`AGENTS.md` siempre; el resto bajo demanda.** El índice al final de `AGENTS.md` está pensado
   para que el agente sepa qué abrir sin tener que abrirlo todo.
2. **Antes de tocar un área, cargá su doc.** Exploits → `06_TRAPS.md §C` + `04_OFFSETS.md §4.3`.
   UI → `06_TRAPS.md §D`. Un feature nuevo → `05_RULES.md` R8.
3. **Nunca vuelques el log de MSBuild al contexto.** Son ~140 KB. Redirigí a archivo y greppeá.
4. **Cuidado con los archivos grandes.** `ImguiMenu.cpp` tiene 3325 líneas, `LuaEngine.cpp` 2299 y
   `HookStormOthers.hpp` 2155. Leé rangos, no archivos enteros.
5. **Tareas más chicas.** Un modelo con menos contexto rinde mejor con "arreglá X en el archivo Y"
   que con "auditá todo el aimbot". Las auditorías grandes de este proyecto se hicieron con mucho
   más contexto disponible.
6. **`docs/06_TRAPS.md` es el que más rinde por token.** Si solo podés cargar un doc además del
   núcleo, que sea ese: previene reintroducir bugs que ya costaron días.

---

## Paso 7 — Primer trabajo de prueba

Antes de darle algo importante al setup nuevo, validalo con una tarea de bajo riesgo que ejercite
toda la cadena:

1. Compilar sin tocar nada → exit code 0 y la línea `-> ...\Release\ByxStorm.dll`.
   **Verificado el 13/08/2026 desde PowerShell.** Si tu agente corre comandos por Git Bash, va a
   fallar con `MSB1008` hasta que use `MSYS2_ARG_CONV_EXCL="*"` — ver `docs/01_BUILD.md`.
2. Pedirle al agente que explique el orden de los tres gates de `CL_Move` **sin** leer
   `06_TRAPS.md`, solo con `AGENTS.md` + el código. Si acierta, la documentación en código sigue
   siendo suficiente. Si no, hacé que abra el doc — es exactamente para lo que está.
3. Pedirle un cambio chico y verificable (mover un slider de pestaña, agregar un `CFG(...)`
   faltante) y comprobá que **actualiza `.vcxproj` y `.filters`** si crea archivos.

---

## Paso 8 — Mantener este paquete vivo

`docs/` solo sirve si no se pudre. Reglas mínimas:

- Un hallazgo nuevo de IDA → `docs/04_OFFSETS.md`.
- Un bug cuya causa era no obvia → `docs/06_TRAPS.md`. **Este es el que más importa.**
- Una feature nueva → `docs/03_FEATURES.md`.
- Una preferencia nueva del usuario → `docs/05_RULES.md`.
- Un cambio de estado (probado in-game, feature eliminada) → `docs/08_ESTADO.md`.

Escribí siempre **el porqué y el síntoma**, no solo la conclusión. Lo que hace útiles a estos
documentos es que un lector reconozca su propio síntoma en la página.

Y por sobre todo: **mantené la regla R2**. Todo lo que entre a `docs/` va en lenguaje neutro.
