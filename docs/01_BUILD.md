# 01 — Compilar y verificar

## Toolchain exacto

| Pieza | Valor |
|---|---|
| IDE / MSBuild | Visual Studio **18** Community — `C:\Program Files\Microsoft Visual Studio\18\Community\` |
| MSBuild | `...\18\Community\MSBuild\Current\Bin\MSBuild.exe` |
| Toolset | `PlatformToolset = ClangCL` → `...\VC\Tools\Llvm\x64\bin\clang-cl.exe` |
| Windows SDK | `WindowsTargetPlatformVersion = 10.0` |
| Estándar C++ | `stdcpp20` · C: `Default` |
| Charset | MultiByte |
| Configuraciones | `Debug\|Win32`, `Release\|Win32`, `Debug\|x64`, `Release\|x64` — **solo interesa `Release\|Win32`** |

Las configuraciones x64 existen pero son inútiles: el juego es de 32 bits.

## El comando

### PowerShell — la forma verificada, usá esta

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"; & $msbuild "C:\Byx_storm-L4D2\ByxStorm.sln" /t:Build /p:Configuration=Release /p:Platform=x86 /m /v:minimal
```

Rebuild completo (≈10 s gracias a `/m`): reemplazá `/t:Build` por `/t:Rebuild`.

### ⚠️ Git Bash mangla los switches de MSBuild

Correr el mismo comando desde Git Bash **falla**. MSYS aplica conversión de rutas a cualquier
argumento que empiece con `/`:

```
/t:Build  ->  t:Build
/m        ->  M:/
```

y MSBuild aborta con:

```
MSBUILD : error MSB1008: Only one project can be specified.
```

Es un error engañoso: no tiene nada que ver con el proyecto ni con la solución.

Si necesitás compilar desde bash, desactivá la conversión (probado, funciona):

```bash
MSYS2_ARG_CONV_EXCL="*" "/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" "C:\Byx_storm-L4D2\ByxStorm.sln" /t:Build /p:Configuration=Release /p:Platform=x86 /m /v:minimal
```

### Notas que evitan perder tiempo

- **`/p:Platform=x86`, no `Win32`.** El `.sln` declara la plataforma `x86` y el `.vcxproj` la mapea
  internamente a `Win32`. Pasar `Win32` a nivel de solución falla.
- **La salida pesa ~140 KB en un rebuild completo.** Redirigila a archivo y greppeala; no la
  vuelques al contexto. (Un build incremental sin cambios son ~120 bytes.)
- **Señal de éxito:** `error :` == 0 **y** la línea final
  `ByxStorm.vcxproj -> C:\Byx_storm-L4D2\Release\ByxStorm.dll`.
- **Ojo con el exit code en bash:** en el caso del mangling de arriba, `grep -c 'error :'` da **0**
  (el error de MSBuild tiene otro formato, `MSBUILD : error MSB1008`) mientras el exit code es 1.
  Chequeá **las dos cosas**: exit code Y la línea final del `.dll`. Contar `error :` solo no alcanza.
- **Y no greppeés `error` a secas:** este proyecto tiene `c_terror_player.h`, `c_terror_gun.h`,
  `c_terror_weapon.h`, `CTerrorPlayer`… La palabra **"error" está dentro de "terror"**, así que un
  `grep -i error` da falsos positivos en cada build. Usá el patrón con espacio y dos puntos:
  `error :` (y `warning :` para los warnings).

## Los warnings NO son señal de regresión

Todos los warnings del proyecto son ruido de headers de terceros, emitido **una vez por unidad de
traducción**:

- `json.hpp` — `-Wdeprecated-literal-operator`, 2 por TU
- `ISurface.h:97` — `'thiscall' attribute only applies to functions`, 1 por TU

O sea: el total escala con la cantidad de `.cpp` y sube cada vez que se parte una feature en su
propio archivo. Fue 116 el 15/07, 230 el 08/08 y **248 el 13/08**, sin ninguna regresión en el
medio — solo se agregaron archivos.

Para chequear de verdad si *tu* edición introdujo algo, agrupá por archivo:

```bash
grep 'warning :' log.txt | sed -E 's/\(.*//' | sort | uniq -c | sort -rn
```

y buscá tu archivo en la lista. Si no aparece, no introdujiste warnings.

El `.vcxproj` ya silencia una tanda de warnings de Clang vía `AdditionalOptions`
(`-Wno-unused-private-field`, `-Wno-pragma-once-outside-header`, etc.).

## Agregar un archivo nuevo al proyecto

**No hay globbing.** El `.vcxproj` lista archivo por archivo. Un `.cpp` nuevo que no esté listado
simplemente no se compila y la feature queda muerta sin ningún error.

Hay que tocar **dos** archivos:

1. `ByxStorm.vcxproj` → `<ClCompile Include="src\...\Foo.cpp" />` y
   `<ClInclude Include="src\...\Foo.h" />`
2. `ByxStorm.vcxproj.filters` → la misma entrada, con su `<Filter>` para que aparezca ordenado en
   el árbol de Visual Studio.

Los dos usan **rutas con backslash relativas a la raíz del proyecto**.

> Ya hubo una sesión entera dedicada a arreglar el desincronizado entre `.vcxproj` y `.filters`
> (21/07). Mantenelos parejos.

Para excluir un archivo de la compilación pero dejarlo en el árbol, se usa `<None Include=... />`
— es lo que se hace con `src/Storm/Hooks/HooksStorm.cpp`.

## Probar el DLL

1. Compilar.
2. Inyectar `Release/ByxStorm.dll` en `left4dead2.exe` con cualquier inyector manual x86.
3. **INSERT** abre el menú. **F11** descarga el DLL limpiamente.
4. Leer `<carpeta del juego>/ByxStorm_debug.txt` — se reescribe entero en cada inyección, así que
   siempre corresponde a la corrida actual.

Al iniciar, el log confirma qué se resolvió y qué no. `Memory::ResolveFn` imprime `OK` + RVA
resuelta, `FALLBACK` (usó la RVA hardcodeada) o `FAIL`. Un `FAIL` significa que esa feature está
muerta: `CHook::Create` salta la instalación en vez de hookear una dirección nula.

## Verificación sin juego: el harness headless de menú

Dentro de `src/App/Features/Menu/ImguiMenu.cpp` hay un bloque de **UI pura sin ninguna dependencia
del juego**: los helpers de estilo y widgets (`BeginCard`/`EndCard`, `PillButton`, `Toggle`,
`Help`, `KeyName`, `KeybindButton`, `Mix`, `RefreshPalette`…). La única función que toca el juego
en medio de ese bloque es `VisualsOverlays`, que se saltea. Eso permite:

1. Extraer ese rango con `sed` a un `widgets.inc`.

> **Identificá el rango vos mismo antes de extraerlo.** Cuando el harness se construyó por primera
> vez el rango era ~58..712, pero el archivo se reescribió a fondo después (rediseño del menú del
> 11/08) y esos números **ya no corresponden** — la línea 58 hoy es un `#include`. Buscá dónde
> arranca el primer helper de widget y dónde termina el último antes de la primera función que
> use `I::` o `H::`.
2. Compilarlo en una consola headless contra las fuentes de `ImGui/` del propio proyecto — sin D3D9,
   sin ventana, sin juego.
3. Dos shims cubren todo lo que referencia:
   `struct CMenuImgui{static void LoadFonts();static void RegisterConfig();};` y un
   `Config::Register` template que no hace nada.
4. Manejarlo con `ImGui::NewFrame()`/`Render()` + `io.AddMousePosEvent`/`AddMouseButtonEvent`, y
   assertear contra internals de ImGui (`GetID`, `TempInputIsActive`, `OpenPopupStack`, `WorkRect`,
   `IsItemVisible`).

Se construye con `cl` vía `VsDevCmd.bat -arch=amd64`. La carga de fuentes funciona headless (lee de
verdad `C:\Windows\Fonts` y arma un atlas de 512×512).

Esta técnica encontró **3 bugs reales de UI** que la lectura del código no había encontrado.
Llegó a 228 checks en verde.

> **El harness vivía en un scratchpad de sesión y se perdió.** Si vas a tocar el menú en serio,
> reconstruirlo es barato comparado con lo que detecta. Los detalles de los 3 bugs que encontró
> están en `docs/06_TRAPS.md`.
