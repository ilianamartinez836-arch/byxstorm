#include "../../SDK/SDK.h"
#include "../Features/MiscTools/EnemyChat/EnemyChat.h"

// CBaseHudChat::ChatPrintf - el punto por donde el juego escribe CADA linea del chat.
//
// POR QUE HACE FALTA ESTE HOOK (bug reportado 06/08: "el Enemy Chat Spy sigue copiando los
// mensajes que mandan a global, no solo los que mandan a solo equipo"):
//
// El evento `player_say` llega al cliente para TODOS los mensajes - ese es justamente el agujero
// que hace posible el chat spy. Pero el evento NO dice si el mensaje fue al chat global o al de
// equipo: se extrajo `resource/serverevents.res` del propio pak01_dir.vpk y su definicion completa
// es solo dos campos:
//
//     "player_say" { "userid" "short"   "text" "string" }
//
// O sea que el `GetBool("teamonly")` que tenia el codigo leia un campo INEXISTENTE y devolvia
// siempre el default (false) - por eso ni la etiqueta "(TEAM)" aparecia nunca ni habia forma de
// filtrar. (Lo mismo pasaba con "player_name", que tampoco existe; ese ya caia bien de casualidad
// porque habia un fallback a GetPlayerInfo.)
//
// Como el evento no alcanza, la pregunta se da vuelta: en vez de "fue de equipo?", preguntamos
// "el juego ya lo mostro por su cuenta?". Si el juego lo imprimio, el mensaje era visible para vos
// y copiarlo es la duplicacion de la que se queja el reporte; si no lo imprimio, era de equipo
// enemigo y nuestra copia es la unica.
//
// ORDEN: el server manda primero el usermessage SayText2 (lo que hace imprimir al juego) y despues
// dispara el evento, y el cliente procesa los mensajes del datagrama en ese mismo orden, asi que
// para cuando corre nuestro FireGameEvent la linea del juego ya paso por aca.
//
// La direccion sale de EnemyChat::ResolveChatPrintfOnce(), que es ahora el UNICO lugar del
// proyecto que escanea esta firma.
//
// BUG (11/08): antes cada uno resolvia por su cuenta el mismo patron. Este hook corre primero y
// resuelve bien, pero enseguida MinHook le pisa el prologo con un jmp - asi que cuando EnemyChat
// escaneaba despues para poder LLAMAR a la funcion, ya no encontraba nada y se desactivaba el
// chat entero (el log mostraba "OK" al arrancar y "FB-REJECT / FAIL" mas tarde, con la misma
// direccion). Resolver una sola vez, ANTES del parche, y compartir el resultado.
//
// Ojo con la convencion: es variadica, asi que `this` viaja como primer argumento de pila y no por
// ECX (MSVC no puede combinar thiscall con varargs). Ver la nota larga en EnemyChat.cpp.
MAKE_HOOK(
	CBaseHudChat_ChatPrintf,
	Features::MiscTools::EnemyChat::ResolveChatPrintfOnce(),
	void, __cdecl, void *pThis, int iPlayerIndex, int iFilter, const char *fmt, ...)
{
	char szLine[512] = {};
	if (fmt)
	{
		va_list args;
		va_start(args, fmt);
		_vsnprintf_s(szLine, sizeof(szLine), _TRUNCATE, fmt, args);
		va_end(args);
	}

	// Nuestras propias lineas no cuentan como "el juego lo mostro" - si se registraran, la primera
	// copia que imprimimos haria que la siguiente se descarte sola.
	if (!Features::MiscTools::EnemyChat::IsPrintingOurOwn())
		Features::MiscTools::EnemyChat::NoteGamePrinted(szLine);

	// Se reenvia ya formateado con "%s": una funcion variadica no puede pasarle sus propios
	// argumentos a otra variadica (no existe una version vChatPrintf), y como el texto va como
	// ARGUMENTO y no como formato, cualquier '%' que traiga el mensaje de un jugador se imprime
	// literal en vez de interpretarse - que ademas es lo seguro.
	CALL_ORIGINAL(pThis, iPlayerIndex, iFilter, "%s", szLine);
}
