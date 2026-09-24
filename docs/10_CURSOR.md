# 10 — Configurar Cursor

Todo lo que se puede dejar hecho por adelantado **ya está hecho**. Este documento explica qué es
cada archivo y qué te queda por hacer a mano (que es poco).

---

## Lo que ya está en el repo

| Archivo | Qué hace |
|---|---|
| `.cursor/rules/byxstorm.mdc` | reglas del proyecto con `alwaysApply: true` — se inyectan en **cada** request sin que hagas nada |
| `.cursorrules` | mismo contenido en el formato legacy, por si tu versión de Cursor todavía lo prefiere |
| `AGENTS.md` | el archivo de instrucciones completo. Cursor lo lee, y también lo leen opencode/Cline/Aider |
| `.cursorignore` | saca del alcance ~4,9 GB de basura de build |
| `.cursorindexingignore` | saca del **índice** `_backups/` y `LuaJIT/`, pero los deja legibles |
| `.cursor/mcp.json` | apunta al servidor MCP de IDA Pro |

### Por qué los dos ignore son importantes

Sin ellos, Cursor indexa esto:

```
.vs/        4,8 GB   cache de IntelliSense
ByxStorm/    98 MB   .obj de un build Debug obsoleto
_backups/    29 MB   7 snapshots completos del árbol
```

`_backups/` es el peligroso: son **1363 archivos fuente duplicados** contra los **330 reales** de
`src/`. Con eso indexado, una búsqueda semántica devuelve mayormente versiones viejas del código, y
en el peor caso el agente edita una copia de backup creyendo que es el código vivo.

`_backups/` va en `.cursorindexingignore` y no en `.cursorignore` a propósito: la regla R1 usa
diffs contra los snapshots para verificar cambios grandes, así que tiene que seguir siendo legible
cuando se lo pedís explícitamente.

`_reference/analisis_externo/` **sí** se indexa: la tabla de 1616 netvars es consulta constante.

---

## Lo que tenés que hacer vos

### 1. Abrir la carpeta correcta

`File → Open Folder…` → `C:\Byx_storm-L4D2`

Tiene que ser **la raíz**, no `src/`. Si abrís una subcarpeta, Cursor no ve `AGENTS.md` ni las
reglas ni los ignore.

### 2. Configurar DeepSeek

`Cursor Settings → Models`. Desactivá los modelos que no vayas a usar, agregá tu API key de
DeepSeek y sobrescribí la Base URL con `https://api.deepseek.com` (su API es compatible con
OpenAI). El nombre del modelo depende del que contrataste — miralo en la documentación de DeepSeek,
su catálogo cambia. Probá la conexión con el botón de test **antes** de seguir.

> Si los nombres de menú no coinciden, la UI de Cursor cambió: el concepto sigue siendo
> *proveedor compatible con OpenAI + base URL + API key + nombre de modelo*.

### 3. Construir el índice

`Cursor Settings → Indexing` (o Codebase Indexing). Con los ignore puestos indexa ~6,5 MB de código
real en vez de 5 GB. Esperá a que termine antes de la primera pregunta seria.

### 4. Verificar que las reglas se están aplicando

En un chat nuevo, preguntá algo trivial y fijate si la respuesta respeta las reglas (por ejemplo,
si te da el comando de build, tiene que ser el de PowerShell). Si no, revisá que
`.cursor/rules/byxstorm.mdc` tenga `alwaysApply: true` en su frontmatter.

### 5. Conectar IDA (opcional pero muy recomendado)

Casi todo el conocimiento técnico de este proyecto salió de desensamblar el juego. Sin esto el
agente puede leer código, pero no descubrir offsets.

1. Abrí **IDA Professional 9.3** con `left4dead2/bin/client.dll` o `engine.dll`.
2. Confirmá que el servidor responde:
   ```bash
   curl -s -X POST http://127.0.0.1:13337/mcp -H "Content-Type: application/json" -d "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"server_health\",\"arguments\":{}},\"id\":1}"
   ```
3. `.cursor/mcp.json` ya apunta ahí. Revisá `Cursor Settings → MCP`: el servidor `ida-pro` tiene que
   aparecer en verde.

> **Si no conecta:** el paquete instalado es `ida-pro-mcp` **2.0.0** y trae su propio instalador,
> que escribe la configuración en el formato exacto que espera cada cliente. Corré
> `ida-pro-mcp --install` (el ejecutable está en el directorio `Scripts` de tu Python) y dejá que
> él genere la config, en vez de pelearte con el `mcp.json` a mano. El endpoint es JSON-RPC sobre
> HTTP plano, no SSE, así que según la versión de Cursor puede necesitar un puente stdio — eso es
> justo lo que resuelve el instalador.
>
> Esta parte **no está verificada desde acá**: el servidor MCP responde, pero no pude comprobar la
> negociación con Cursor. Es el único punto del setup que puede necesitar ajuste.

---

## Cómo usarlo día a día

### Estrenar un agente

Pegá el prompt de `docs/PROMPT_INICIAL.md` como primer mensaje. Una sola vez.

### Modos

- **Agent** para trabajo real: puede leer, editar y correr comandos.
- **Ask** para preguntas sobre el código sin riesgo de que edite nada.

### Traer contexto a mano con `@`

El índice ayuda, pero para trabajo dirigido conviene adjuntar explícitamente:

| Escribís | Traés |
|---|---|
| `@AGENTS.md` | las reglas, si sospechás que las está ignorando |
| `@docs/06_TRAPS.md` | antes de debuggear algo raro |
| `@docs/04_OFFSETS.md` | antes de tocar offsets o netvars |
| `@src/App/Hooks/CL_Move.cpp` | antes de tocar cualquier exploit de red |
| `@_reference/analisis_externo/l4d2_netvars_offsets.md` | antes de reversear un offset a mano |

### Cuando empiece a ignorar las reglas

Pasa siempre en conversaciones largas: el contexto se llena y las reglas se diluyen. Una línea
alcanza:

```
Releé AGENTS.md antes de seguir, sobre todo R9 (registrar en docs/).
```

Si ya viene arrastrando errores, es más barato abrir un chat nuevo que discutir. **El contexto de
DeepSeek (~128 K) es bastante más chico que el que se venía usando en este proyecto**: chats cortos
y tareas acotadas rinden mucho mejor que una sesión larga.

### Comandos

Cursor corre los comandos por la terminal integrada. Si tu terminal por defecto es **Git Bash**, el
build va a fallar con `MSB1008` — ver `docs/01_BUILD.md`. Poné PowerShell como terminal por defecto,
o dejá que el agente use el prefijo `MSYS2_ARG_CONV_EXCL="*"`.

---

## Higiene recomendada antes de empezar

Con Visual Studio y Cursor cerrados:

```powershell
Remove-Item -Recurse -Force "C:\Byx_storm-L4D2\.vs", "C:\Byx_storm-L4D2\ByxStorm"
```

Los ignore ya evitan que Cursor los toque, pero son ~4,9 GB de disco que no sirven para nada.
`.vs/` se regenera solo la próxima vez que abras el `.sln`.
