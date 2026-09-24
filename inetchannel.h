#pragma once
#include "netchannel.h"
#include "inetchannelinfo.h"
#include "inetmessage.h"

#define	MAX_QPATH 96
#define	MAX_OSPATH 260

#define NET_FRAMES_BACKUP	128		// must be power of 2
#define NET_FRAMES_MASK		(NET_FRAMES_BACKUP-1)
#define MAX_SUBCHANNELS		8		// we have 8 alternative send&wait bits

#define SUBCHANNEL_FREE		0	// subchannel is free to use
#define SUBCHANNEL_TOSEND	1	// subchannel has data, but not send yet
#define SUBCHANNEL_WAITING	2   // sbuchannel sent data, waiting for ACK
#define SUBCHANNEL_DIRTY	3	// subchannel is marked as dirty during changelevel

class IDemoRecorder;
class INetChannelHandler;

typedef void* FileHandle_t;

class INetChannel : public INetChannelInfo
{
public:
	virtual	~INetChannel(void) {};
	virtual void SetDataRate(float rate) = 0;
	virtual bool RegisterMessage(INetMessage *msg) = 0;
	virtual bool StartStreaming(unsigned int challengeNr) = 0;
	virtual void ResetStreaming(void) = 0;
	virtual void SetTimeout(float seconds, bool bForceExact = false) = 0;
	virtual void SetDemoRecorder(IDemoRecorder *recorder) = 0;
	virtual void SetChallengeNr(unsigned int chnr) = 0;
	virtual void Reset(void) = 0;
	virtual void Clear(void) = 0;
	virtual void Shutdown(const char *reason) = 0;
	virtual void ProcessPlayback(void) = 0;
	virtual bool ProcessStream(void) = 0;
	virtual void ProcessPacket(struct netpacket_s *packet, bool bHasHeader) = 0;
	virtual bool SendNetMsg(INetMessage &msg, bool bForceReliable = false, bool bVoice = false) = 0;
	virtual bool SendData(bf_write &msg, bool bReliable = true) = 0;
	virtual bool SendFile(const char *filename, unsigned int transferID, bool bIsReplayDemoFile) = 0;
	virtual void DenyFile(const char *filename, unsigned int transferID, bool bIsReplayDemoFile) = 0;
	virtual void RequestFile_OLD(const char *filename, unsigned int transferID) = 0;
	virtual void SetChoked(void) = 0;
	virtual int SendDatagram(bf_write *data) = 0;
	virtual bool Transmit(bool onlyReliable = false) = 0;
	virtual const netadr_t &GetRemoteAddress(void) const = 0;
	virtual INetChannelHandler *GetMsgHandler(void) const = 0;
	virtual int GetDropNumber(void) const = 0;
	virtual int GetSocket(void) const = 0;
	virtual unsigned int GetChallengeNr(void) const = 0;
	virtual void GetSequenceData(int &nOutSequenceNr, int &nInSequenceNr, int &nOutSequenceNrAck) = 0;
	virtual void SetSequenceData(int nOutSequenceNr, int nInSequenceNr, int nOutSequenceNrAck) = 0;
	virtual void UpdateMessageStats(int msggroup, int bits) = 0;
	virtual bool CanPacket(void) const = 0;
	virtual bool IsOverflowed(void) const = 0;
	virtual bool IsTimedOut(void) const = 0;
	virtual bool HasPendingReliableData(void) = 0;
	virtual void SetFileTransmissionMode(bool bBackgroundMode) = 0;
	virtual void SetCompressionMode(bool bUseCompression) = 0;
	virtual unsigned int RequestFile(const char *filename, bool bIsReplayDemoFile) = 0;
	virtual void SetMaxBufferSize(bool bReliable, int nBytes, bool bVoice = false) = 0;
	virtual bool IsNull() const = 0;
	virtual int GetNumBitsWritten(bool bReliable) = 0;
	virtual void SetInterpolationAmount(float flInterpolationAmount) = 0;
	virtual void SetRemoteFramerate(float flFrameTime, float flFrameTimeStdDeviation) = 0;
	virtual void SetMaxRoutablePayloadSize(int nSplitSize) = 0;
	virtual int GetMaxRoutablePayloadSize() = 0;
	virtual bool SetActiveChannel(INetChannel *pNewChannel) = 0;
	virtual void AttachSplitPlayer(int nSplitPlayerSlot, INetChannel *pChannel) = 0;
	virtual void DetachSplitPlayer(int nSplitPlayerSlot) = 0;
	virtual bool IsRemoteDisconnected() const = 0;
};

class CNetChannel : public INetChannel
{
public:
	typedef struct dataFragments_s
	{
		FileHandle_t file; //0x00E8 // open file handle
		char filename[MAX_OSPATH]; //0x00EC // filename
		char *buffer; //0x01F0 // if NULL it's a file
		unsigned int bytes; //0x01F4 // size in bytes
		unsigned int bits; //0x01F8 // size in bits
		unsigned int transferID; //0x01FC // only for files
		bool isCompressed; //0x0200 // true if data is bzip compressed
		unsigned int nUncompressedSize; //0x0204 // full size in bytes
		bool asTCP; //0x0208 // send as TCP stream
		bool isReplayDemo; //0x0209 // if it's a file, is it a replay .dem file?
		int numFragments; //0x020C // number of total fragments
		int ackedFragments; //0x0210 // number of fragments send & acknowledged
		int pendingFragments; //0x0214 // number of fragments send, but not acknowledged yet
	} dataFragments_t;

	struct subChannel_s
	{
		int	startFraggment[MAX_STREAMS]; //0x0348
		int	numFragments[MAX_STREAMS]; //0x0350
		int	sendSeqNr; //0x0358
		int	state; //0x035C // 0 = free, 1 = scheduled to send, 2 = send & waiting, 3 = dirty
		int	index; //0x0360 // index in m_SubChannels[]

		void Free()
		{
			state = SUBCHANNEL_FREE;
			sendSeqNr = -1;
			for (int i = 0; i < MAX_STREAMS; i++)
			{
				numFragments[i] = 0;
				startFraggment[i] = -1;
			}
		}
	};

	typedef struct netframe_s
	{
		float time;	//0x0578 // net_time received/send
		int	size;	//0x057C // total size in bytes
		short choked; //0x0580 // number of previously chocked packets
		int	dropped; //0x0584
		float latency; //0x0588 // raw ping for this packet, not cleaned. set when acknowledged otherwise -1.
		float avg_latency;	//0x058C // averaged ping for this packet
		float m_flInterpolationAmount;  //0x0590
		unsigned short msggroups[TOTAL]; //0x0594 // received bytes for each message group
		bool valid; //0x05B0 // false if dropped, lost, flushed
	} netframe_t;

	typedef struct
	{
		float nextcompute;	//0x0550 // Time when we should recompute k/sec data
		float avgbytespersec;	//0x0554 // average bytes/sec
		float avgpacketspersec; //0x0558 // average packets/sec
		float avgloss;		//0x055C // average packet loss [0..1]
		float avgchoke;		//0x0560 // average packet choke [0..1]
		float avglatency;	//0x0564 // average ping, not cleaned
		float latency;		//0x0568 // current ping, more accurate also more jittering
		int	totalpackets;	//0x056C // total processed packets
		int	totalbytes;		//0x0570 // total processed bytes
		int	currentindex;	//0x0574 // current frame index
		netframe_t	frames[NET_FRAMES_BACKUP]; // frame history
		netframe_t* currentframe; //0x2378 // current frame
	} netflow_t;

public:
	bool m_bProcessingMessages; //0x0004
	bool m_bClearedDuringProcessing; //0x0005
	bool m_bShouldDelete; //0x0006
	int32_t m_nOutSequenceNr; //0x0008
	int32_t m_nInSequenceNr; //0x000C
	int32_t m_nOutSequenceNrAck; //0x0010
	int32_t m_nOutReliableState; //0x0014
	int32_t m_nInReliableState; //0x0018
	int32_t m_nChokedPackets; //0x001C
	bf_write m_StreamReliable; //0x0020
	CUtlMemory<byte> m_ReliableDataBuffer; //0x0038
	bf_write m_StreamUnreliable;
	CUtlMemory<byte> m_UnreliableDataBuffer; //0x005C
	bf_write m_StreamVoice; //0x0044
	CUtlMemory<byte> m_VoiceDataBuffer; //0x0080
	int32_t m_Socket; //0x008C
	int32_t m_StreamSocket; //0x0090
	uint32_t m_MaxReliablePayloadSize; //0x0094
	netadr_t remote_address;
	float last_received; //0x00A4
	double connect_time; //0x00A8
	int32_t m_Rate; //0x00B0
	char pad_00B4[4]; //0x00B4
	double m_fClearTime; //0x00B8
	CUtlVector<dataFragments_t*> m_WaitingList[MAX_STREAMS]; //0x00C0
	dataFragments_t	m_ReceiveList[MAX_STREAMS]; //0x00E8
	subChannel_s m_SubChannels[MAX_SUBCHANNELS]; //0x0348
	unsigned int m_FileRequestCounter; //0x0428
	bool m_bFileBackgroundTranmission; //0x042C
	bool m_bUseCompression; //0x042D
	bool m_StreamActive; //0x042E
	int32_t m_SteamType; //0x0430
	int32_t m_StreamSeqNr; //0x0434
	int32_t m_StreamLength; //0x0438
	int32_t m_StreamReceived; //0x043C
	char m_SteamFile[MAX_OSPATH]; //0x0440
	CUtlMemory<byte> m_StreamData;
	netflow_t m_DataFlow[MAX_FLOWS];
	int32_t	m_MsgStats[TOTAL];
	int32_t m_PacketDrop; //0x41E0
	char m_Name[32]; //0x41E4
	unsigned int m_ChallengeNr; //0x4204
	float m_Timeout; //0x4208
	INetChannelHandler* m_MessageHandler; //0x420C
	CUtlVector< CUtlVector< INetMessageBinder* > >	m_NetMessages; //0x4210
	IDemoRecorder* m_DemoRecorder; //0x4224
	int32_t m_nQueuedPackets; //0x4228
	float m_flInterpolationAmount; //0x422C
	float m_flRemoteFrameTime; //0x4230
	float m_flRemoteFrameTimeStdDeviation; //0x4234
	int32_t m_nMaxRoutablePayloadSize; //0x4238
	int32_t m_nSplitPacketSequence; //0x423C
	INetChannel *m_pActiveChannel; //0x4240
};