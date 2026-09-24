#include "../../SDK/SDK.h"
#include "../../SDK/L4D2/inetchannel.h"
#include "../../Utils/DebugLog.h"
#include "../../Utils/Memory/Memory.h"
#include "../Features/Exploits/SpeedHack/SpeedHack.h"
#include "../Features/Exploits/CustomSpeedHack/CustomSpeedHack.h"
#include "../Features/Exploits/RapidFire/RapidFire.h"
#include "../Features/Exploits/BleedExploit/BleedExploit.h"
#include "../Features/Exploits/BleedExploitImproved/BleedExploitImproved.h"
#include "../Features/Exploits/ChokeExploit/ChokeExploit.h"
#include "../Features/Exploits/ChokeExploit2/ChokeExploit2.h"
#include "../Features/Exploits/RapidFireCustom/RapidFireCustom.h"
#include "../Features/Exploits/Reference/SpeedHackRef.h"
#include "../Features/Exploits/Reference/RapidFireRef.h"
#include "../Features/Exploits/Reference/RefDriver.h"
#include "../Features/Exploits/SendMoveOverride/SendMoveOverride.h"
#include "../Features/Exploits/Fakelag/Fakelag.h"
#include "../Features/Exploits/ChargerTurn/ChargerTurn.h"
#include "../Features/Camera/Thirdperson.h"
#include "../Features/Exploits/ExploitsDiag.h"
#include <cmath>

// ── SpeedHack por rebobinado de return address (2026-08-13) ──────────────────────────────
// Tecnica portada de la implementacion de referencia (su `*move_ret_addr -= 0x5`). En vez de
// generar los comandos sinteticos del SpeedHack con un `for` de CALL_ORIGINAL (que obliga a pelear
// contra los tres gates a mano), se REBOBINA el return address para que el CPU re-ejecute el
// `call CL_Move` del propio motor: CL_Move se vuelve a ejecutar entero, con su contabilidad de
// secuencia consistente. Ver docs/07_EXTERNALS.md §7.4.
//
// SEGURIDAD: solo se rebobina si el call site es un `call rel32` (opcode E8, 5 bytes). Si el motor
// llamara a CL_Move por otro encoding (FF 15 / FF D0), rebobinar 5 saltaria al medio de la
// instruccion y corromperia el flujo - por eso se verifica el byte antes, y si no es E8 se cae al
// loop clasico (comportamiento de siempre, sin rebobinado). El rebobinado es OPT-IN
// (SpeedHack::bRewind, apagado por defecto) hasta que se verifique en servidor dedicado.
namespace
{
	inline bool IsRewindableCallSite()
	{
		void *pRetSlot = _AddressOfReturnAddress();
		uintptr_t nRet = *reinterpret_cast<uintptr_t *>(pRetSlot);
		uintptr_t nCallSite = nRet - 5;
		return *reinterpret_cast<unsigned char *>(nCallSite) == 0xE8;
	}

	inline void RewindReturnAddress()
	{
		void *pRetSlot = _AddressOfReturnAddress();
		*reinterpret_cast<uintptr_t *>(pRetSlot) -= 5;
	}

	// Telemetria de "basura" (2026-08-13): anillo de timestamps de wraps del buffer de historial
	// de comandos del cliente, para calcular wraps/segundo (leido por Network Info). Vive aca y no
	// en una feature porque es estado propio del hook de CL_Move.
	static float s_aWrapTimes[64] = {};
	static int   s_nWrapHead = 0;
	static int   s_nWrapFilled = 0;
}

// engine.dll CL_Move(float flAccumulatedExtraSamples, bool bFinalTick): the per-frame function
// that samples input, builds a usercmd and hands it to the net channel to be sent to the server.
// The server advances the local player one tick of movement per command it receives. So calling
// the original N extra times per real frame = N extra ticks of movement per frame = speedhack.
MAKE_HOOK(
	CL_Move, Memory::ResolveFn("CL_Move", "engine.dll", "55 8B EC 81 EC ? ? ? ? A1 ? ? ? ? 33 C5 89 45 FC 56 E8 ? ? ? ? 8B F0 83 7E 68 02 0F 8C", 0x07D120),
	void, __cdecl, float flAccumulatedExtraSamples, int bFinalTick)
{
	static bool bLogged = false;
	if (!bLogged) { Debug::Log("[Byx] CL_Move hooked.\n"); bLogged = true; }

	// engine.dll CL_RunPrediction() - confirmed by decompiling the real engine.dll (telemetry
	// string "CL_RunPrediction" / "cl_pred.cpp" at this exact address). CL_Move only samples
	// input and builds/sends the usercmd; the actual client-side prediction (weapon fire
	// cooldown, movement simulation) that makes the extra shots register has to be re-run
	// separately after every synthetic extra CL_Move call, or the extra commands still reach the
	// server (movement works) but the weapon's predicted fire state never re-simulates, so extra
	// shots only ever look like they fired locally and don't register.
	static auto pCL_RunPrediction = reinterpret_cast<void(__cdecl *)()>(
		Memory::ResolveFn("CL_RunPrediction", "engine.dll", "55 8B EC 83 EC 18 A1 ? ? ? ? 33 C9 89 4D F8 89 4D FC 8B 00 3B C1 74 27 8B 50 50", 0x80DA0));

	static bool bLoggedResolve = false;
	if (!bLoggedResolve)
	{
		Debug::Log("[Byx] pCL_RunPrediction resolved to 0x%p\n", (void *)pCL_RunPrediction);
		bLoggedResolve = true;
	}

	// SECOND gate found decompiling CL_Move itself (engine.dll cl_main.cpp:2585, sub_1007D120):
	// separate from CanPacket()/m_fClearTime (which controls whether a built command actually
	// transmits), CL_Move has its own earlier gate deciding whether to sample a NEW command at
	// all this call: `v4 = GetClientState(); if (IsLoopback-ish-check) skip = (v4+112 > net_time
	// || !CanPacket() || !bFinalTick);`. On any real (non-loopback) server that first disjunct is
	// always true, so it reduces to `v4+112 > net_time` once CanPacket() is fixed (see below) -
	// confirmed by debug log: after the m_fClearTime fix, choked/ack finally tracked correctly
	// (real transmission was happening), but curtime stayed completely flat across every extra
	// call in a burst - every extra call was retransmitting the SAME already-sampled command
	// instead of a new one, because this gate was still closing after the first. `v4+112` is
	// "next allowed real-wall-clock sample time", paced forward by CL_Move itself after every
	// successful sample using `1/cl_cmdrate` - normal pacing for one command per real frame, but
	// it blocks every subsequent extra call within the same tight loop since real time barely
	// advances between them. `v4` is produced by a tiny accessor (`return someGlobalPtr + 8;`,
	// decompiled at engine.dll+0x82CA0) - resolved and called directly, same pattern as
	// pCL_RunPrediction above, rather than hardcoding the data address (which isn't
	// signature-scannable and would break on any patch that moves static data).
	static auto pGetClientState = reinterpret_cast<uintptr_t(__cdecl *)()>(
		Memory::ResolveFn("GetClientState", "engine.dll", "A1 ? ? ? ? 83 C0 08 C3", 0x82CA0));

	static bool bLoggedClientState = false;
	if (!bLoggedClientState)
	{
		Debug::Log("[Byx] pGetClientState resolved to 0x%p\n", (void *)pGetClientState);
		bLoggedClientState = true;
	}

	// ── RE-ENTRADA del rebobinado de SpeedHack (2026-08-13) ───────────────────────────────
	// Cuando SpeedHack usa el rebobinado, el CPU re-ejecuta el `call CL_Move` del motor y vuelve a
	// entrar aca. Esta rama genera UN comando sintetico del SpeedHack (movimiento puro) por
	// re-entrada, y vuelve a rebobinar hasta agotar el cupo. Tiene que estar ANTES de todo el
	// trabajo por-frame de abajo (Updates, Bleed/Choke, CALL_ORIGINAL real), porque ese trabajo
	// NO es idempotente y solo debe correr en la entrada real del frame.
	static int s_nRewindLeft = 0;
	static int s_nBleedChokeWriteValue = 0;   // valor de Bleed/Choke capturado en la entrada real
	if (s_nRewindLeft > 0)
	{
		--s_nRewindLeft;
		Features::Exploits::SpeedHack::bExtraTick = true;

		auto pNetChan = reinterpret_cast<CNetChannel *>(I::EngineClient->GetNetChannelInfo());
		int nRealAck = 0, nRealChoked = 0;
		if (pNetChan)
		{
			pNetChan->m_fClearTime = 0.0;
			nRealAck = pNetChan->m_nOutSequenceNrAck;
			nRealChoked = pNetChan->m_nChokedPackets;
			pNetChan->m_nOutSequenceNrAck = -1;
			pNetChan->m_nChokedPackets = 255;
		}

		if (pGetClientState)
			if (uintptr_t v4 = pGetClientState())
			{
				*reinterpret_cast<double *>(v4 + 112) = 0.0;
				*reinterpret_cast<int *>(v4 + 19016) = s_nBleedChokeWriteValue;
			}

		CALL_ORIGINAL(flAccumulatedExtraSamples, 1);
		Features::Exploits::SpeedHack::bExtraTick = false;

		if (pNetChan)
		{
			if (pNetChan->m_nOutSequenceNrAck == -1)
				pNetChan->m_nOutSequenceNrAck = nRealAck;
			if (pNetChan->m_nChokedPackets == 255)
				pNetChan->m_nChokedPackets = nRealChoked;
		}

		if (pCL_RunPrediction)
			pCL_RunPrediction();

		if (auto pLocalTick = H::Entities->GetLocal())
			pLocalTick->m_nTickBase()--;

		if (s_nRewindLeft > 0)
			RewindReturnAddress();
		return;
	}

	// Refresh each feature's own keybind toggle once per real frame (before the extra calls). Each
	// feature owns its own rising-edge tracking - no shared dispatcher between them.
	Features::Exploits::SpeedHack::Update();
	Features::Exploits::CustomSpeedHack::Update();
	Features::Exploits::BleedExploit::Update();
	Features::Exploits::BleedExploitImproved::Update();
	Features::Exploits::ChokeExploit::Update();
	Features::Exploits::ChokeExploit2::Update();
	Features::Exploits::RapidFireCustom::Update();
	Features::Exploits::SpeedHackRef::Update();
	Features::Exploits::SendMoveOverride::Update();
	// Byte patch sobre client.dll, no toca comandos - se aplica/restaura al cambiar el toggle.
	Features::Exploits::ChargerTurn::Update();
	// Toggle por tecla. La camara en si la mueve el hook de IClientMode::OverrideView (indice 19).
	Features::Thirdperson::Update();

	// Resets CRapidFire's per-frame output slot (see RapidFire.h for the full writeup). The real
	// per-frame command below is what drives RunSurvivor's is_initial branch (via
	// Copy_Command.cpp's RapidFire::OnCommand), which is what actually populates the extra-call
	// count read further down.
	Features::Exploits::RapidFire::BeginFrame();

	// Idem para el par de referencia (SpeedHackRef + RapidFireRef), que corre con estado propio y
	// aporta UN solo contador para los dos - ver Features/Exploits/Reference/RefDriver.h.
	Features::Exploits::Reference::BeginFrame();

	// Bandera que escribe el SpeedHack de referencia dentro de Copy_Command (ver SDK.h). Se limpia
	// al principio de cada frame para que refleje el frame actual y no quede pegada; no la consume
	// nadie hoy porque el loop de abajo ya fuerza los mismos gates en cada iteracion.
	G::shouldForceSendPackets = false;

	// Bleed Exploit(s) / Choke Exploit(s) - see Exploits.h for the full writeup. Different
	// mechanism from SpeedHack/RapidFire below: no extra CL_Move calls at all, just writes into
	// the engine's own per-frame bookkeeping so IT batches the extra commands into one packet
	// instead of us sending N separate ones.
	//
	// OFFSET, SETTLED FOR REAL 2026-07-18: v4Bleed (this function's own pGetClientState()) is NOT
	// the same base an external reference script / an external compiled Choke Exploit call
	// "client_state". Decompiled our own engine.dll's GetClientState (engine.dll+0x82CA0) directly:
	// `return dword_104268EC + 8;` - dword_104268EC's RVA is 0x4268EC = 4352236 in decimal,
	// EXACTLY that reference's `eng + 4352236`. So v4Bleed == that reference's client_state + 8 -
	// our accessor adds a constant +8 the raw pointer read doesn't. Confirmed independently that
	// the external compiled Choke Exploit reference does NOT go through an equivalent wrapper - it
	// reads the raw `*(engine_base+4352236)` directly, no +8, then does `+19024` on THAT.
	// Therefore the correct offset relative to OUR v4Bleed is `19024 - 8 = 19016` - the ORIGINAL
	// value this file had before a same-day "fix" (undone here) mistakenly changed it to 19024
	// without accounting for the accessor's built-in +8. Both Bleed and Choke Exploit now
	// correctly target the exact same real field (client_state+19024, "Choked_Commands") via
	// `v4Bleed+19016` - kept as separate toggles (not merged) so raw/improved/choke1/choke2
	// remain independently combinable, per the user's explicit choice.
	//
	// TIMING FIX (2026-07-18, user report: combining SpeedHack + the reference exploit doesn't
	// multiply "rate" the way it does in the reference - PPS stays flat, only ~3x rate instead of
	// the reference's much bigger jump): decompiled the real packet-builder (engine.dll's
	// sub_1007CEC0) - it reads `client_state+19016` FRESH every time it runs and uses
	// `min(that_value+1, 15)` as how many backup/redundant commands to bundle into THIS packet
	// (matches the Storm reference's `Commands_Queue` formula exactly, just relative to our own
	// +8'd base). CL_Move itself (sub_1007D120) resets `client_state+19016 = 0` immediately after
	// EVERY real send. This block used to run AFTER the real CALL_ORIGINAL below, meaning the
	// REAL per-frame packet never saw the inflated value at all (it already sent using leftover
	// state from last frame) - and the very next thing that ran, the extra-call loop, immediately
	// zeroed the field again on its first iteration before any of ITS calls could use it either.
	// Net effect once SpeedHack/RapidFire/etc. were ALSO active: Bleed/Choke's bundling was
	// completely defeated, every frame - explains "no multiplication" exactly. Fixed by moving
	// this block BEFORE the real call (so it sees the inflated value too) and, in the extra-call
	// loop below, re-applying the inflated value for SpeedHack/CustomSpeedHack-attributed
	// iterations instead of unconditionally zeroing - RapidFire-attributed iterations
	// keep the original unconditional zero (see the loop for why: RapidFire has its own separate,
	// more fragile per-command accumulator that a previous session confirmed breaks - "curtime
	// freezes solid" - when this same field is left inflated during ITS OWN extra calls).
	//
	// KNOWN TRADEOFF (confirmed via user-supplied debug log, 2026-07-18): applying the inflated
	// value to SpeedHack/CustomSpeedHack's OWN extra calls too produces the same
	// "curtime frozen, only outseq climbing" duplicate-tick pattern for THOSE buckets as well,
	// not just RapidFire's - so packet/pps counts look bigger but a portion of that traffic is
	// duplicate resends, not distinct new ticks. Restored at the user's explicit request (wants
	// the bigger raw numbers back over the more conservative real-tick-only version) - if this
	// needs revisiting, the fully-conservative version (unconditional 0 for every bucket) is in
	// this file's own edit history from earlier the same day.
	// SINGLE OWNER of the netchannel ack/choked spoof (2026-07-27).
	//
	// BUG CONFIRMED BY LIVE CAPTURE, then fixed here. Bleed(raw), Bleed(Improved) and Choke each
	// used to keep their OWN "last genuine ack" snapshot, each guarded by `ack != -1` and each
	// forcing ack=-1 right after. Whichever block ran FIRST poisoned every later one: they saw
	// the -1 their neighbour had just written, so their guard never passed and their snapshot
	// froze at whatever it happened to hold. The log showed it exactly - with Bleed(raw) on,
	// Choke's snapshot stuck at 1560 forever; the moment Bleed(raw) was toggled off, Improved's
	// snapshot unfroze and resumed climbing, and the poisoning simply moved to the next block in
	// order. The damage landed on toggle-off: Choke's restore wrote its stale 1560 back to the
	// netchannel while the genuine ack was ~1884, yanking the out-sequence number backwards by
	// ~324 packets.
	//
	// One capture, one spoof, one restore - taken BEFORE any feature writes, so the value is
	// always this frame's genuine one, and only released once EVERY spoofing feature is off (so
	// one feature toggling off can no longer restore while another is still forcing -1).
	static bool s_bSpoofWasActive = false;
	static int  s_nGenuineAck = 0, s_nGenuineChoked = 0;

	s_nBleedChokeWriteValue = 0;
	if (pGetClientState)
	{
		if (uintptr_t v4Bleed = pGetClientState())
		{
			auto pNetChanBleed = reinterpret_cast<CNetChannel *>(I::EngineClient->GetNetChannelInfo());

			// Capture the genuine ack/choked BEFORE any feature below forces them - this is the
			// step whose absence caused the cross-feature poisoning described above. Runs
			// unconditionally so the snapshot stays fresh even on frames where nothing is active.
			if (pNetChanBleed && pNetChanBleed->m_nOutSequenceNrAck != -1)
			{
				s_nGenuineAck    = pNetChanBleed->m_nOutSequenceNrAck;
				s_nGenuineChoked = pNetChanBleed->m_nChokedPackets;
			}

			// Bleed Exploit (raw): matches the reference script's own "tq" toggle. The netchannel
			// spoof it used to do inline is now handled once, below, for all four features.
			if (Features::Exploits::BleedExploit::IsActive())
			{
				s_nBleedChokeWriteValue = Features::Exploits::BleedExploit::nValue;
				*reinterpret_cast<int *>(v4Bleed + 19016) = s_nBleedChokeWriteValue;
			}

			// Bleed Exploit (Improved): identical writes, but tracks the last genuine (non-forced)
			// ack/choked values seen while active, and restores them the moment the feature is
			// toggled off - instead of leaving the netchannel stuck at -1/255 forever, which is
			// the exact bug this session found and fixed in RapidFire's own netchannel handling
			// (see the ROOT CAUSE / stuck-backlog comments below). No null/life-state checks
			// beyond what's unavoidable to not crash the process (per explicit user request -
			// this is deliberately as raw as the base variant otherwise).
			// Bleed Exploit (Improved): the "Improved" part was always the restore-on-toggle-off,
			// which is now the shared owner's job below - so what is left here is identical to
			// the raw variant. Kept as its own toggle because they carry independent sliders and
			// the user asked for both to stay separately combinable.
			if (Features::Exploits::BleedExploitImproved::IsActive())
			{
				s_nBleedChokeWriteValue = Features::Exploits::BleedExploitImproved::nValue;
				*reinterpret_cast<int *>(v4Bleed + 19016) = s_nBleedChokeWriteValue;
			}

			// Choke Exploit (1 + 2) - reversed from a real compiled reference implementation, see
			// Exploits.h for the full writeup (2026-07-18). Same ack=-1/choked=255 netchannel spoof
			// as Bleed, and (per the offset note above) the SAME clientstate field as Bleed too -
			// v4Bleed+19016, both resolving to the real client_state+19024 "Choked_Commands" field.
			// Toggle-style (per user request 2026-07-18, overriding the reference's own hold-key
			// default) - Choke 1 and Choke 2 are independent toggles that STACK when both are on
			// (confirmed via decompile: the reference sums Factor1+Factor2 into the same write).
			// Since this is now a sticky
			// toggle rather than a momentary hold, restore the real ack/choked once BOTH toggle
			// off - same "don't leave the netchannel stuck forever" fix already applied to Bleed
			// Exploit (Improved).
			{
				bool bChoke1 = Features::Exploits::ChokeExploit::IsActive();
				bool bChoke2 = Features::Exploits::ChokeExploit2::IsActive();

				if (bChoke1 || bChoke2)
				{
					s_nBleedChokeWriteValue = (bChoke1 ? Features::Exploits::ChokeExploit::nValue : 0)
						+ (bChoke2 ? Features::Exploits::ChokeExploit2::nValue : 0);
					*reinterpret_cast<int *>(v4Bleed + 19016) = s_nBleedChokeWriteValue;
				}
			}

			// The one place the netchannel spoof is applied and released, for all four features.
			// Releasing only once EVERY one of them is off is the second half of the fix: before,
			// one feature toggling off would restore while another was still forcing -1.
			{
				const bool bAnySpoof = Features::Exploits::BleedExploit::IsActive()
					|| Features::Exploits::BleedExploitImproved::IsActive()
					|| Features::Exploits::ChokeExploit::IsActive()
					|| Features::Exploits::ChokeExploit2::IsActive();

				if (pNetChanBleed)
				{
					if (bAnySpoof)
					{
						pNetChanBleed->m_nOutSequenceNrAck = -1;
						pNetChanBleed->m_nChokedPackets = 255;
					}
					else if (s_bSpoofWasActive)
					{
						pNetChanBleed->m_nOutSequenceNrAck = s_nGenuineAck;
						pNetChanBleed->m_nChokedPackets = s_nGenuineChoked;
					}
				}
				s_bSpoofWasActive = bAnySpoof;
			}
		}
	}

	// Bleed/Choke interaction trace (2026-07-27, see ExploitsDiag.h). Placed HERE - after every
	// block above has had its turn, before the real send - because that is the only point where
	// the question "what did each feature want vs what actually survived" has an answer.
	// One line per real frame, and only while at least one of the four is active.
	if (Features::Exploits::Diag::bLogBleedChoke)
	{
		const bool bB  = Features::Exploits::BleedExploit::IsActive();
		const bool bBI = Features::Exploits::BleedExploitImproved::IsActive();
		const bool bC1 = Features::Exploits::ChokeExploit::IsActive();
		const bool bC2 = Features::Exploits::ChokeExploit2::IsActive();

		// Logs for ONE frame past the last feature turning off. The release/restore fires on
		// exactly that frame, and gating purely on "something is active" skipped it - which is
		// why the first capture after the single-owner fix proved the snapshot no longer freezes
		// but could not show the restore writing a correct value (2026-07-27).
		static bool s_bDiagWasActive = false;
		const bool  bDiagAnyNow = bB || bBI || bC1 || bC2;

		if (bDiagAnyNow || s_bDiagWasActive)
		{
			s_bDiagWasActive = bDiagAnyNow;

			const int nWantB  = bB  ? Features::Exploits::BleedExploit::nValue         : 0;
			const int nWantBI = bBI ? Features::Exploits::BleedExploitImproved::nValue : 0;
			const int nWantC1 = bC1 ? Features::Exploits::ChokeExploit::nValue         : 0;
			const int nWantC2 = bC2 ? Features::Exploits::ChokeExploit2::nValue        : 0;

			// Read the field back rather than trusting s_nBleedChokeWriteValue - that variable is
			// what the LAST block intended, this is what the engine will actually consume.
			int nField = -1;
			if (pGetClientState)
				if (uintptr_t vDiag = pGetClientState())
					nField = *reinterpret_cast<int *>(vDiag + 19016);

			auto pNCDiag = reinterpret_cast<CNetChannel *>(I::EngineClient->GetNetChannelInfo());

			// Everything else on this line is OUR OWN process memory - it proves what we wrote,
			// not that the server did anything with it. These three are the closest thing to a
			// server-observable effect available client-side (same INetChannelInfo values the
			// Network Info overlay shows): outgoing packets/sec, average outgoing choke, and
			// outgoing bandwidth. If a feature genuinely changes how much the engine bundles and
			// sends, it has to show up here; if these are identical with one feature on and with
			// three, then combining them is doing nothing. (2026-07-27)
			auto pInfoDiag = I::EngineClient->GetNetChannelInfo();
			const float flPpsOut   = pInfoDiag ? pInfoDiag->GetAvgPackets(FLOW_OUTGOING) : -1.f;
			const float flChokeOut = pInfoDiag ? pInfoDiag->GetAvgChoke(FLOW_OUTGOING) * 100.f : -1.f;
			const float flDataOut  = pInfoDiag ? pInfoDiag->GetAvgData(FLOW_OUTGOING) / 1024.f : -1.f;

			Debug::Log(
				"[Byx][Exp] want b=%d bi=%d c1=%d c2=%d | sum=%d applied=%d field=%d | ack=%d choked=%d | snap=%d/%d | ppsOut=%.1f chokeOut=%.2f%% kbOut=%.1f\n",
				nWantB, nWantBI, nWantC1, nWantC2,
				nWantB + nWantBI + nWantC1 + nWantC2,   // what SUMMING would give
				s_nBleedChokeWriteValue,                  // what last-write-wins actually gives
				nField,
				pNCDiag ? pNCDiag->m_nOutSequenceNrAck : -999,
				pNCDiag ? pNCDiag->m_nChokedPackets     : -999,
				// Now a SINGLE shared snapshot. It must keep climbing with the real ack no
				// matter which combination of features is on - if it ever freezes again, the
				// one-owner fix has regressed.
				s_nGenuineAck, s_nGenuineChoked,
				flPpsOut, flChokeOut, flDataOut);
		}
	}

	// FAKELAG. Se decide aca, justo antes del envio real, y se ataca el gate `bSendPacket` del
	// propio CL_Move (ver Features/Exploits/Fakelag/Fakelag.h para el writeup y por que el intento
	// anterior dentro de CL_SendMove no podia funcionar).
	//
	// El CL_Move real hace `if (v4+112 > net_time || !CanPacket() || !bFinalTick) bSendPacket = 0;`
	// y con bSendPacket en 0 toma SOLO su rama de choke: `netchan->SetChoked(); ++chokedcommands;`
	// y retorna antes del SendDatagram. CanPacket() es `net_time > m_fClearTime`, asi que subir
	// m_fClearTime cierra el gate. Es exactamente lo inverso de lo que hace el loop de llamadas
	// extra mas abajo, que lo pone en 0 para FORZAR el envio.
	//
	// Se restaura el valor real apenas vuelve: el engine lo re-pacea solo en cada envio exitoso, y
	// dejarselo pisado romperia el envio de los frames siguientes.
	const bool bFakelagChoke = Features::Exploits::Fakelag::ShouldChoke();
	auto pNetChanFakelag = bFakelagChoke
		? reinterpret_cast<CNetChannel *>(I::EngineClient->GetNetChannelInfo())
		: nullptr;
	double dSavedClearTime = 0.0;

	if (pNetChanFakelag)
	{
		dSavedClearTime = pNetChanFakelag->m_fClearTime;
		pNetChanFakelag->m_fClearTime = 1.0e9;
	}

	// The one call the engine actually expects this frame. Now runs AFTER the Bleed/Choke block
	// above (see the TIMING FIX note) so this real packet also gets the inflated backup-command
	// bundling when either is active - previously ran first and never saw it.
	CALL_ORIGINAL(flAccumulatedExtraSamples, bFinalTick);

	if (pNetChanFakelag)
		pNetChanFakelag->m_fClearTime = dSavedClearTime;

	// Each extra call generates and sends another movement command for this frame. Split so the
	// loop below knows which iterations are plain-SpeedHack-attributed (movement only - attack
	// buttons get suppressed entirely, see Features::Exploits::SpeedHack::bExtraTick) vs Custom
	// SpeedHack/RapidFire-attributed (left alone - both behave like RapidFire, extra shots
	// included, per the user's explicit choice - see CustomSpeedHack.h).
	const int nSpeedHackExtra = Features::Exploits::SpeedHack::GetExtraCalls();

	// Verified 2026-07-18 by decompiling a real compiled reference SpeedHack (sub_100F6D20, matches
	// the fragment the user pasted byte-for-byte): our own SpeedHack extra-call mechanism already covers
	// everything that function does (max(0,factor) extra calls, "force send" is implicit in our
	// unconditional loop below) EXCEPT one thing - it also zeroes m_flSimulationTime, which "forces
	// the engine to catch-up with many ticks of prediction" per the original's own comment. Ported
	// here since it's now fully confirmed, not guessed.
	if (nSpeedHackExtra > 0)
	{
		if (auto pLocalSH = H::Entities->GetLocal())
			pLocalSH->m_flSimulationTime() = 0.f;
	}

	const int nCustomSpeedHackExtra = Features::Exploits::CustomSpeedHack::GetExtraCalls();
	// nRapidFireExtra now comes straight from CRapidFire's own algorithm (real accumulator, real
	// ack-progress cash-in - see RapidFire.h), computed during the real CALL_ORIGINAL above via
	// Copy_Command.cpp's RapidFire::OnCommand call. Replaces the old hand-rolled curtime-stuck
	// heuristic that used to live here.
	int nRapidFireExtra = Features::Exploits::RapidFire::GetExtraCalls();

	// Cuarto bucket: el par de referencia. UN solo contador para SpeedHackRef + RapidFireRef, no
	// dos - en esa implementacion los factores se combinan en una cifra en vez de sumarse como
	// buckets independientes (ver RefDriver.h). Va al final para que su bucket quede claramente
	// separado de los tres nuestros en el loop de abajo.
	const int nReferenceExtra = Features::Exploits::Reference::GetExtraCalls();

	// El SpeedHack de referencia pone sim_time en 0 por su cuenta, con el offset crudo +16, dentro
	// de su propio OnFirstTick (ver SpeedHackRef.h) - no se replica aca a proposito: si escribiera
	// ademas el netvar, el A/B contra nuestro SpeedHack dejaria de medir esa diferencia.

	// Si el Fakelag chokeo este frame, no se emiten llamadas extra: cada una volveria a intentar
	// mandar y anularia el choke que acabamos de pedir.
	const int nExtra = bFakelagChoke
		? 0
		: (nSpeedHackExtra + nCustomSpeedHackExtra + nRapidFireExtra + nReferenceExtra);

	// Publicar el cupo configurado para el overlay de Network Info (telemetria de "basura"). Se
	// publica SIEMPRE (aunque sea 0) para que el overlay distinga "inactivo" de "activo".
	Features::Exploits::Diag::nExtraThisFrame  = nExtra;
	Features::Exploits::Diag::nBurstBeforeWrap = 0;

	// Was a per-frame Debug::Log here (nExtra/bRapidFire/curtime/netchan snapshot) - removed
	// 2026-07-23, scaffolding for [[reference_clmove_ratelimit]]'s now-RESOLVED investigation. It
	// fired unconditionally every real frame any Exploit added extra calls, and was the dominant
	// contributor to a 1MB+ ByxStorm_debug.txt. flPrevCurTime itself is still real logic (feeds the
	// backlog-wrap detection below), not just diagnostic - kept.
	float flPrevCurTime = -1.f;
	if (nExtra > 0)
	{
		auto pLocal = H::Entities->GetLocal();
		flPrevCurTime = pLocal ? static_cast<float>(pLocal->m_nTickBase()) * I::GlobalVars->interval_per_tick : -1.f;
	}

	// ROOT CAUSE FOUND (2026-07-16): decompiled the real engine.dll CNetChannel::CanPacket() and
	// CNetChannel::SetChoked() in IDA. CanPacket() is `return net_time > this->m_fClearTime;`
	// (m_fClearTime, CNetChannel+0xB8, src/SDK/L4D2/inetchannel.h) - it does NOT read
	// m_nOutSequenceNrAck or m_nChokedPackets at all. SetChoked() is just
	// `++m_nOutSequenceNr; ++m_nChokedPackets;` - which is exactly the lockstep growth
	// ByxStorm_debug.txt showed for EVERY extra call, on SpeedHack as much as RapidFire (neither
	// spoofs anything netchannel-related on SpeedHack, yet both showed identical choking) - proof
	// this was never about ack/choked at all. m_fClearTime gets paced forward by SendDatagram()
	// after every real send (based on packet size / configured bandwidth rate) - correct behavior
	// for one command per real frame, but it means right after the frame's first real send,
	// m_fClearTime already sits in the future, so CanPacket() returns false and CL_Move takes the
	// SetChoked() no-op path for every subsequent extra call this frame - none of them were ever
	// reaching the server. Forcing m_fClearTime back before each extra call makes CanPacket() pass
	// so the real send path actually runs; SendDatagram naturally re-paces it forward again
	// afterward from real bandwidth math, so nothing is left broken - this only removes the
	// self-throttling that was silently killing every extra call before it could transmit.
	// Bucket boundaries for the loop below (SpeedHack, then CustomSpeedHack, then RapidFire,
	// matching the exact order nExtra was summed in). Only RapidFire's own bucket gets
	// the unconditional client_state+19016=0 reset (its own fragile per-command accumulator needs
	// a clean baseline, confirmed 2026-07-17) - every other bucket gets Bleed/Choke's inflated
	// value instead (see the TIMING FIX note above CALL_ORIGINAL), so their packets also bundle
	// backup commands instead of the field being wiped before they ever run. Restored 2026-07-18
	// at the user's explicit request after briefly reverting it - see the TIMING FIX note's
	// "KNOWN TRADEOFF" paragraph for the confirmed duplicate-tick side effect on non-RapidFire
	// buckets this reintroduces.
	const int nRapidFireBucketStart = nSpeedHackExtra + nCustomSpeedHackExtra;
	const int nRapidFireBucketEnd = nRapidFireBucketStart + nRapidFireExtra;

	// Bucket del par de referencia, al final. Recibe el mismo trato que el de RapidFire (reset
	// incondicional de clientstate+19016) SOLO cuando fue el RapidFire de referencia el que fijo el
	// contador: es su acumulador el que se rompe con un backlog inflado. Si el que manda es el
	// SpeedHack de referencia, el bucket recibe el valor de Bleed/Choke como los buckets de
	// SpeedHack/CustomSpeedHack.
	const int nReferenceBucketStart = nRapidFireBucketEnd;
	const int nReferenceBucketEnd = nReferenceBucketStart + nReferenceExtra;
	const bool bReferenceIsRapidFire = Features::Exploits::Reference::IsRapidFireDriving();

	// Rebobinado de SpeedHack (2026-08-13): si esta activo y el call site es rebobinable (E8), el
	// bucket de SpeedHack se genera por RE-ENTRADA del hook (ver el guard de arriba), asi que este
	// loop arranca DESPUES de esos indices y solo cubre CustomSpeedHack/RapidFire/Reference. Si no,
	// arranca en 0 y cubre todo (comportamiento de siempre).
	const bool bRewindSpeedHack = nSpeedHackExtra > 0
		&& Features::Exploits::SpeedHack::bRewind
		&& IsRewindableCallSite();
	const int nLoopStart = bRewindSpeedHack ? nSpeedHackExtra : 0;

	for (int i = nLoopStart; i < nExtra; ++i)
	{
		// First nSpeedHackExtra iterations are plain SpeedHack (movement only, zero shooting
		// effect - user confirmed in-game that without this, holding fire while SpeedHack is
		// active also multiplies shots exactly like RapidFire, since CL_Move resamples the whole
		// real input state on every extra call, attack button included). Everything after that
		// (Custom SpeedHack, then RapidFire) is left alone - both behave like RapidFire.
		Features::Exploits::SpeedHack::bExtraTick = i < nSpeedHackExtra;
		const bool bRapidFireBucket =
			(i >= nRapidFireBucketStart && i < nRapidFireBucketEnd)
			|| (bReferenceIsRapidFire && i >= nReferenceBucketStart && i < nReferenceBucketEnd);

		auto pNetChan = reinterpret_cast<CNetChannel *>(I::EngineClient->GetNetChannelInfo());
		int nRealAck = 0, nRealChoked = 0;
		if (pNetChan)
		{
			pNetChan->m_fClearTime = 0.0;

			nRealAck = pNetChan->m_nOutSequenceNrAck;
			nRealChoked = pNetChan->m_nChokedPackets;
			pNetChan->m_nOutSequenceNrAck = -1;
			pNetChan->m_nChokedPackets = 255;
		}

		// Force the "next allowed sample time" gate open too (see comment above pGetClientState) -
		// without this, CanPacket() alone just makes every extra call retransmit the SAME stale
		// command instead of sampling a genuinely new one.
		if (pGetClientState)
		{
			if (uintptr_t v4 = pGetClientState())
			{
				*reinterpret_cast<double *>(v4 + 112) = 0.0;

				// FOUND VIA IDA (2026-07-17): decompiled the real CL_Move (engine.dll+0x7D120) and
				// its packet-builder sub_1007CEC0. clientstate+19016 ("commands owed/backlog") feeds
				// directly into flAccumulatedExtraSamples (`v4+19012 + v4+19016 + 1`, passed to the
				// input sampler at dword_104EE780's vtable+80) - it's the SAME field the Bleed
				// Exploit(s) write to every real frame (see the Bleed block above, `v4Bleed + 19016`).
				// If Bleed inflates it (its slider values, e.g. 45), every one of THIS loop's extra
				// calls inherits that inflated backlog and the sampler treats each one as "catch up
				// on N owed commands" instead of "sample one genuinely fresh tick" - confirmed root
				// cause of RapidFire's curtime freezing solid (zero real ticks, only outseq moving)
				// whenever a Bleed Exploit was also held. Resetting it to 0 before each of OUR OWN
				// extra calls keeps their sampling a real fresh tick; Bleed's own one real-frame
				// packet (built by sub_1007CEC0 BEFORE this loop starts, using the inflated value
				// once) is unaffected - both features keep working together.
				//
				// SPLIT 2026-07-18 (see the TIMING FIX note above CALL_ORIGINAL): RapidFire's own
				// bucket keeps the unconditional reset-to-0 exactly as before - its accumulator is
				// specifically confirmed fragile against an inflated backlog. Every OTHER bucket
				// (SpeedHack/CustomSpeedHack) instead re-applies Bleed/Choke's inflated value here if
				// either is active, so sub_1007CEC0 bundles backup commands into EVERY one of their
				// packets too, not just whichever one happened to run before this field got zeroed.
				// When neither Bleed nor Choke is active, s_nBleedChokeWriteValue is 0 and this is
				// byte-for-byte the original unconditional-reset behavior. KNOWN TRADEOFF: this does
				// the same "duplicate tick" thing to SpeedHack/CustomSpeedHack's own sampling that it
				// does to RapidFire's - restored anyway at the user's explicit request (bigger raw
				// packet/pps numbers over real-tick-only counts).
				*reinterpret_cast<int *>(v4 + 19016) = bRapidFireBucket ? 0 : s_nBleedChokeWriteValue;
			}
		}

		// THIRD gate piece found via IDA (2026-07-17), explains why SOME entire bursts still froze
		// (curtime identical across all iterations) even after the two fixes above: CL_Move's v55
		// computation is `(!IsLoopback() || cvar) && (v4+112 > net_time || !CanPacket() || !a4)`
		// where `a4` is this exact `bFinalTick` parameter - on real frames where the engine's own
		// bFinalTick happened to be false, passing that SAME false value through to every one of
		// our extra calls made `!a4` true, forcing v55=0 for the WHOLE burst regardless of the
		// m_fClearTime/v4+112/v4+19016 forces above - v55==0 takes CL_Move's early-return path
		// (SetChoked(): increments outseq, samples nothing), matching "outseq climbs, curtime
		// frozen" exactly. The real per-frame call above keeps the engine's own bFinalTick (its
		// accounting, not ours to override); every one of OUR OWN synthetic extra calls always
		// wants to be treated as a real, finalized sample, so force true here specifically.
		CALL_ORIGINAL(flAccumulatedExtraSamples, 1);
		Features::Exploits::SpeedHack::bExtraTick = false;

		// BUG FOUND (2026-07-16, user report: RapidFire "breaks"/"stops recharging" the longer
		// it's held, specifically breaking the incap+RapidFire+revive god-mode trick):
		// unconditionally restoring here stomps a REAL ack update if one genuinely arrived while
		// CALL_ORIGINAL was processing incoming network data - m_nOutSequenceNrAck might have
		// legitimately advanced past -1 during the call (the engine actually caught up), and we
		// were reverting it back to the stale pre-call snapshot every single iteration regardless,
		// permanently preventing the netchannel's real ack from ever advancing while RapidFire is
		// held. This compounds the longer the key stays down, matching the reported symptom
		// exactly (breaks after firing "for a while", not immediately). A reference implementation
		// seen elsewhere only restores when the value is STILL what it force-wrote - only restore
		// if nothing genuine changed it during the call.
		if (pNetChan)
		{
			if (pNetChan->m_nOutSequenceNrAck == -1)
				pNetChan->m_nOutSequenceNrAck = nRealAck;
			if (pNetChan->m_nChokedPackets == 255)
				pNetChan->m_nChokedPackets = nRealChoked;
		}

		// Re-run client prediction for the command we just synthesized (see comment above - this
		// is what actually makes the extra shot/movement register, not just look like it happened).
		if (pCL_RunPrediction)
			pCL_RunPrediction();

		// [2026-08-13] tick_base-- en el comando sintetico recien predicho.
		// Correccion portada de la implementacion de referencia (su run_command_hook): el motor
		// avanza el tickbase predicho UNA vez por comando predicho, pero el servidor trata la
		// tanda entera de comandos sinteticos (todos con el MISMO tick_count) como UN solo tick.
		// Sin esta resta el tickbase local queda N ticks por delante del servidor por cada rafaga
		// (desincronizacion), y por eso la referencia hace `localplayer->tick_base()--` por cada
		// comando sintetico. Aca el equivalente es restar despues de CL_RunPrediction(), que es el
		// punto que simulo el comando extra y avanzo el tickbase.
		if (auto pLocalTick = H::Entities->GetLocal())
			pLocalTick->m_nTickBase()--;

		{
			// Was a per-iteration Debug::Log here (curtime/NextAttack/outseq/choked snapshot) -
			// removed 2026-07-23, same [[reference_clmove_ratelimit]] scaffolding as the per-frame
			// one above, except this one fired once per EXTRA CALL (not just once per frame) - by
			// far the single biggest contributor to log bloat. flCurTime2 itself is still real
			// logic (feeds the backlog-wrap detection right below), not just diagnostic - kept.
			auto pLocal2 = H::Entities->GetLocal();
			float flCurTime2 = pLocal2 ? static_cast<float>(pLocal2->m_nTickBase()) * I::GlobalVars->interval_per_tick : -1.f;

			// The client's own command/prediction history buffer has a fixed capacity. Once the
			// backlog of genuinely-transmitted-but-unacked commands wraps it, CL_RunPrediction
			// resyncs the local player's predicted tickbase sharply backward (observed: ~150
			// ticks / 5s) and every extra call past that point is resimulating from stale,
			// already-superseded state instead of producing a new command. Detect the wrap the
			// moment it happens (a hard backward jump in curtime, far more than one frame's
			// normal forward drift) and stop issuing further extra calls for the rest of THIS
			// frame only; every call up to that point already reached the server. Next real frame
			// starts over and self-adjusts to whatever the connection can currently sustain - no
			// fixed extra-call-count ceiling needed.
			if (flPrevCurTime >= 0.f && flCurTime2 >= 0.f && flPrevCurTime - flCurTime2 > 1.0f)
			{
				Debug::Log(
					"[Byx]   iter=%d backlog wrap detected (curtime %.4f -> %.4f), stopping burst early.\n",
					i, flPrevCurTime, flCurTime2);

				// Telemetria de "basura" para Network Info: este frame el buffer envolvio en la
				// iteracion `i`, asi que `nExtra - i` comandos habrian sido basura. Se anota el
				// timestamp para el rate de wraps/segundo.
				Features::Exploits::Diag::nBurstBeforeWrap = i;
				Features::Exploits::Diag::nWrapsTotal++;
				{
					const float flWrapNow = I::GlobalVars->curtime;
					s_aWrapTimes[s_nWrapHead] = flWrapNow;
					s_nWrapHead = (s_nWrapHead + 1) % 64;
					if (s_nWrapFilled < 64) ++s_nWrapFilled;
					float flRate = 0.f;
					for (int k = 0; k < s_nWrapFilled; k++)
						if (flWrapNow - s_aWrapTimes[k] <= 1.0f) flRate += 1.f;
					Features::Exploits::Diag::flWrapsPerSecond = flRate;
				}
				break;
			}
			flPrevCurTime = flCurTime2;
		}
	}

	// Setup del rebobinado de SpeedHack (2026-08-13): tras el loop (que ya cubrio los buckets
	// restantes), se fija el cupo de re-entradas y se rebobina el return address. Al retornar, el
	// CPU re-ejecuta el `call CL_Move` del motor y el guard de arriba genera un comando sintetico de
	// SpeedHack por cada re-entrada hasta agotar el cupo (la ultima re-entrada NO rebobina y deja
	// que el motor siga su flujo normal).
	if (bRewindSpeedHack)
	{
		s_nRewindLeft = nSpeedHackExtra;
		RewindReturnAddress();
	}
}
