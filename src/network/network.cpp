//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name network.cpp - The network. */
//
//      (c) Copyright 2000-2008 by Lutz Sammer, Andreas Arens, and Jimmy Salmon
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

//@{

//----------------------------------------------------------------------------
// Documentation
//----------------------------------------------------------------------------

/**
** @page NetworkModule Module - Network
**
** @section Basics How does it work.
**
** Stratagus uses an UDP peer to peer protocol (p2p). The default port
** is 6660.
**
** @subsection udp_vs_tcp UDP vs. TCP
**
** UDP is a connectionless protocol. This means it does not perform
** retransmission of data and therefore provides very few error recovery
** services. UDP instead offers a direct way to send and receive
** datagrams (packets) over the network; it is used primarily for
** broadcasting messages.
**
** TCP, on the other hand, provides a connection-based, reliable data
** stream. TCP guarantees delivery of data and also guarantees that
** packets will be delivered in the same order in which they were sent.
**
** TCP is a simple and effective way of transmitting data. For making sure
** that a client and server can talk to each other it is very good.
** However, it carries with it a lot of overhead and extra network lag.
**
** UDP needs less overhead and has a smaller lag. Which is very important
** for real time games. The disadvantages includes:
**
** @li You won't have an individual socket for each client.
** @li Given that clients don't need to open a unique socket in order to
** transmit data there is the very real possibility that a client
** who is not logged into the game will start sending all kinds of
** garbage to your server in some kind of attack. It becomes much
** more difficult to stop them at this point.
** @li Likewise, you won't have a clear disconnect/leave game message
** unless you write one yourself.
** @li Some data may not reach the other machine, so you may have to send
** important stuff many times.
** @li Some data may arrive in the wrong order. Imagine that you get
** package 3 before package 1. Even a package can come duplicate.
** @li UDP is connectionless and therefore has problems with firewalls.
**
** I have choosen UDP. Additional support for the TCP protocol is welcome.
**
** @subsection sc_vs_p2p server/client vs. peer to peer
**
** @li server to client
**
** The player input is send to the server. The server collects the input
** of all players and than send the commands to all clients.
**
** @li peer to peer (p2p)
**
** The player input is direct send to all others clients in game.
**
** p2p has the advantage of a smaller lag, but needs a higher bandwidth
** by the clients.
**
** p2p has been choosen for in-game.
** s/c for the preparing room.
**
** @subsection bandwidth bandwidth
**
** I wanted to support up to 8 players with 28.8kbit modems.
**
** Most modems have a bandwidth of 28.8K bits/second (both directions) to
** 56K bits/second (33.6K uplink) It takes actually 10 bits to send 1 byte.
** This makes calculating how many bytes you are sending easy however, as
** you just need to divide 28800 bits/second by 10 and end up with 2880
** bytes per second.
**
** We want to send many packets, more updated per second and big packets,
** less protocol overhead.
**
** If we do an update 6 times per second, leaving approximately 480 bytes
** per update in an ideal environment.
**
** For the TCP/IP protocol we need following:
** IP  Header 20 bytes
** UDP Header 8  bytes
**
** With 9 bytes per command and N(=9) commands there are 20+8+1+9*N(=120) bytes
** per packet. Sending it to 7 other players, gives 840 bytes per update.
** This means we could do 6 updates (each 166ms) per second (6*840=5040 bytes/s).
**
** @subsection a_packet Network packet
**
** @li [IP  Header - 20 bytes]
** @li [UDP Header -  8 bytes]
** @li [Header Data:Type - 1 byte]
** if Type is one of the InitConfigMessage
** @li [Header Data:SubType - 1 byte]
** @li [Data - depend of subtype (may be 0 byte)]
** if Type is Chat
** [..]
** if Type is Selection
** [..]
** else
** @li [Header Data:Types - N-1 bytes] (for N commands)
** @li [Header Data:Cycle - 1 byte]
** @li [Data:Commands - 8 bytes * N]
**
** @subsection internals Putting it together
**
** All computers in play must run absolute syncron. Only user commands
** are send over the network to the other computers. The command needs
** some time to reach the other clients (lag), so the command is not
** executed immediatly on the local computer, it is stored in a delay
** queue and send to all other clients. After a delay of ::NetworkLag
** game cycles the commands of the other players are received and executed
** together with the local command. Each ::NetworkUpdates game cycles there
** must a package send, to keep the clients in sync, if there is no user
** command, a dummy sync package is send.
** If there are missing packages, the game is paused and old commands
** are resend to all clients.
**
** @section missing What features are missing
**
** @li The recover from lost packets can be improved, if the server knows
** which packets the clients have received.
**
** @li The UDP protocol isn't good for firewalls, we need also support
** for the TCP protocol.
**
** @li Add a server / client protocol, which allows more players per game.
**
** @li Lag (latency) and bandwidth are set over the commandline. This
** should be automatic detected during game setup and later during
** game automatic adapted.
**
** @li Also it would be nice, if we support viewing clients. This means
** other people can view the game in progress.
**
** @li The current protocol only uses single cast, for local LAN we
** should also support broadcast and multicast.
**
** @li Proxy and relays should be supported, to improve the playable
** over the internet.
**
** @li We can sort the command by importants, currently all commands are
** send in order, only chat messages are send if there are free slots.
**
** @li password protection the login process (optional), to prevent that
** the wrong player join an open network game.
**
** @li add meta server support, i have planned to use bnetd and its protocol.
**
** @section api API How should it be used.
**
** ::InitNetwork1()
**
** ::InitNetwork2()
**
** ::ExitNetwork1()
**
** ::NetworkSendCommand()
**
** ::NetworkSendExtendedCommand()
**
** ::NetworkEvent()
**
** ::NetworkQuit()
**
** ::NetworkChatMessage()
**
** ::NetworkEvent()
**
** ::NetworkRecover()
**
** ::NetworkCommands()
**
** ::NetworkFildes
**
** ::NetworkInSync
**
** @todo FIXME: continue docu
*/

//----------------------------------------------------------------------------
//  Includes
//----------------------------------------------------------------------------

#include "stratagus.h"

#include <stddef.h>
#include <list>

#include "network.h"

#include "actions.h"
#include "commands.h"
#include "interface.h"
#include "map.h"
#include "net_lowlevel.h"
#include "net_message.h"
#include "netconnect.h"
#include "parameters.h"
#include "player.h"
#include "replay.h"
#include "sound.h"
#include "translate.h"
#include "unit.h"
#include "unit_manager.h"
#include "unittype.h"
#include "video.h"

//----------------------------------------------------------------------------
//  Declaration
//----------------------------------------------------------------------------

/**
**  Network command input/output queue.
*/
class CNetworkCommandQueue
{
public:
	CNetworkCommandQueue() : Time(0), Type(0) {}
	void Clear() { this->Time = this->Type = 0; Data.Clear(); }

	bool operator == (const CNetworkCommandQueue &rhs) const
	{
		return Time == rhs.Time && Type == rhs.Type && Data == rhs.Data;
	}
	bool operator != (const CNetworkCommandQueue &rhs) const { return !(*this == rhs); }
public:
	unsigned long Time;    /// time to execute
	unsigned char Type;    /// Command Type
	CNetworkCommand Data;  /// command content
};


/**
**  Network Selection Info
*/
struct NetworkSelectionHeader {
	unsigned char Type[MaxNetworkCommands];  /// Command
	unsigned NumberSent : 6;   /// New Number Selected
	unsigned Add : 1;          /// Adding to Selection
	unsigned Remove : 1;       /// Removing from Selection
};

//----------------------------------------------------------------------------
//  Variables
//----------------------------------------------------------------------------

char *NetworkAddr = NULL;                  /// Local network address to use
int NetworkPort = NetworkDefaultPort;      /// Local network port to use
CUDPSocket NetworkFildes;                  /// Network file descriptor
int NetworkInSync = 1;                     /// Network is in sync
int NetworkUpdates = 5;                    /// Network update each # game cycles
int NetworkLag = 10;                       /// Network lag in # game cycles
static unsigned long NetworkLastFrame[PlayerMax]; /// Last frame received packet
static const int NetworkTimeout = 45;             /// Number of seconds until player times out

static char NetMsgBuf[PlayerMax][128];     /// Chat message buffers
static int NetMsgBufLen[PlayerMax];        /// Stored chat message length
static unsigned long NetworkDelay;         /// Delay counter for recover.
static int NetworkSyncSeeds[256];          /// Network sync seeds.
static int NetworkSyncHashs[256];          /// Network sync hashs.
static CNetworkCommandQueue NetworkIn[256][PlayerMax][MaxNetworkCommands]; /// Per-player network packet input queue
static std::list<CNetworkCommandQueue *> CommandsIn;    /// Network command input queue
static std::list<CNetworkCommandQueue *> MsgCommandsIn; /// Network message input queue

#ifdef DEBUG
static int NetworkReceivedPackets;         /// Packets received packets
static int NetworkReceivedEarly;           /// Packets received too early
static int NetworkReceivedLate;            /// Packets received too late
static int NetworkReceivedDups;            /// Packets received as duplicates
static int NetworkReceivedLost;            /// Packets received packet lost

static int NetworkSendPackets;             /// Packets send packets
static int NetworkSendResend;              /// Packets send to resend
#endif

static int PlayerQuit[PlayerMax];          /// Player quit

#define MAX_NCQS 100
static CNetworkCommandQueue NCQs[MAX_NCQS]; /// CNetworkCommandQueues
static int NumNCQs;                         /// Number of NCQs in use


//----------------------------------------------------------------------------
//  Mid-Level api functions
//----------------------------------------------------------------------------

/**
**  Send message to all clients.
**
**  @param packet       Packet to send.
**  @param numcommands  Number of commands.
*/
static void NetworkBroadcast(const CNetworkPacket &packet, int numcommands)
{
	unsigned char *buf = new unsigned char[CNetworkPacket::Size(numcommands)];
	packet.Serialize(buf, numcommands);

	// Send to all clients.
	for (int i = 0; i < HostsCount; ++i) {
		const CHost host(Hosts[i].Host, Hosts[i].Port);
		NetworkFildes.Send(host, buf, CNetworkPacket::Size(numcommands));
	}
	delete[] buf;
}

/**
**  Network send packet. Build it from queue and broadcast.
**
**  @param ncq  Outgoing network queue start.
*/
static void NetworkSendPacket(const CNetworkCommandQueue (&ncq)[MaxNetworkCommands])
{
	CNetworkPacket packet;

#ifdef DEBUG
	++NetworkSendPackets;
#endif

	// Build packet of up to MaxNetworkCommands messages.
	int numcommands = 0;
	packet.Header.Cycle = ncq[0].Time & 0xFF;
	int i;
	for (i = 0; i < MaxNetworkCommands && ncq[i].Type != MessageNone; ++i) {
		packet.Header.Type[i] = ncq[i].Type;
		packet.Command[i] = ncq[i].Data;
		++numcommands;
	}
	for (; i < MaxNetworkCommands; ++i) {
		packet.Header.Type[i] = MessageNone;
	}
	NetworkBroadcast(packet, numcommands);
}

//----------------------------------------------------------------------------
//  API init..
//----------------------------------------------------------------------------

/**
**  Initialize network part 1.
*/
void InitNetwork1()
{
	CommandsIn.clear();
	MsgCommandsIn.clear();
	NumNCQs = 0;

	NetworkFildes.Close();
	NetworkInSync = 1;

	NetInit(); // machine dependent setup

	for (int i = 0; i < PlayerMax; ++i) {
		NetMsgBufLen[i] = 0;
	}
	NetworkUpdates = std::max(NetworkUpdates, 1);
	// Lag must be multiple of updates
	NetworkLag = (NetworkLag / NetworkUpdates) * NetworkUpdates;

	// Our communication port
	int port = NetworkPort;
	for (int i = 0; i < 10; ++i) {
		NetworkFildes.Open(CHost(NetworkAddr, port + i));
		if (NetworkFildes.IsValid()) {
			port = port + i;
			break;
		}
	}
	if (NetworkFildes.IsValid() == false) {
		fprintf(stderr, "NETWORK: No free ports %d-%d available, aborting\n", port, port + 10);
		NetExit(); // machine dependent network exit
		return;
	}
#ifdef DEBUG
	{
		char buf[128];

		gethostname(buf, sizeof(buf));
		DebugPrint("%s\n" _C_ buf);
		const std::string hostStr = CHost(buf, port).toString();
		DebugPrint("My host:port %s\n" _C_ hostStr.c_str());
	}
#endif

#if 0 // FIXME: need a working interface check
	unsigned long ips[10];
	int networkNumInterfaces = NetSocketAddr(NetworkFildes, ips, 10);
	if (networkNumInterfaces) {
		DebugPrint("Num IP: %d\n" _C_ networkNumInterfaces);
		for (int i = 0; i < networkNumInterfaces; ++i) {
			DebugPrint("IP: %d.%d.%d.%d\n" _C_ NIPQUAD(ntohl(ips[i])));
		}
	} else {
		fprintf(stderr, "NETWORK: Not connected to any external IPV4-network, aborting\n");
		ExitNetwork1();
		return;
	}
#endif
}

/**
**  Cleanup network part 1. (to be called _AFTER_ part 2 :)
*/
void ExitNetwork1()
{
	if (!IsNetworkGame()) { // No network running
		return;
	}

#ifdef DEBUG
	DebugPrint("Received: %d packets, %d early, %d late, %d dups, %d lost.\n" _C_
			   NetworkReceivedPackets _C_ NetworkReceivedEarly _C_ NetworkReceivedLate _C_
			   NetworkReceivedDups _C_ NetworkReceivedLost);
	DebugPrint("Send: %d packets, %d resend\n" _C_ NetworkSendPackets _C_ NetworkSendResend);
#endif

	NetworkFildes.Close();
	NetExit(); // machine dependent setup

	NetworkInSync = 1;
	NetPlayers = 0;
	HostsCount = 0;
}

/**
**  Initialize network part 2.
*/
void InitNetwork2()
{
	ThisPlayer->SetName(Parameters::Instance.LocalPlayerName);
	for (int i = 0; i < HostsCount; ++i) {
		Players[Hosts[i].PlyNr].SetName(Hosts[i].PlyName);
	}
	DebugPrint("Lag %d, Updates %d, Hosts %d\n" _C_ NetworkLag _C_ NetworkUpdates _C_ HostsCount);
	// Prepare first time without syncs.
	memset(NetworkIn, 0, sizeof(NetworkIn));
	for (int i = 0; i <= NetworkLag; i += NetworkUpdates) {
		for (int n = 0; n < HostsCount; ++n) {
			for (int c = 0; c < MaxNetworkCommands; ++c) {
				NetworkIn[i][Hosts[n].PlyNr][c].Time = i;
				NetworkIn[i][Hosts[n].PlyNr][c].Type = MessageSync;
			}
		}
	}
	memset(NetworkSyncSeeds, 0, sizeof(NetworkSyncSeeds));
	memset(NetworkSyncHashs, 0, sizeof(NetworkSyncHashs));
	memset(PlayerQuit, 0, sizeof(PlayerQuit));
	memset(NetworkLastFrame, 0, sizeof(NetworkLastFrame));
}

//----------------------------------------------------------------------------
//  Memory management for CNetworkCommandQueues
//----------------------------------------------------------------------------

/**
**  Allocate a CNetworkCommandQueue
**
**  @return  CNetworkCommandQueue
*/
static CNetworkCommandQueue *AllocNCQ()
{
	Assert(NumNCQs != MAX_NCQS);
	CNetworkCommandQueue *ncq = &NCQs[NumNCQs++];
	ncq->Clear();
	return ncq;
}

/**
**  Free a CNetworkCommandQueue
**
**  @param ncq  CNetworkCommandQueue to free
*/
static void FreeNCQ(CNetworkCommandQueue *ncq)
{
	NCQs[ncq - NCQs] = NCQs[--NumNCQs];
}

//----------------------------------------------------------------------------
//  Commands input
//----------------------------------------------------------------------------

/**
**  Prepare send of command message.
**
**  Convert arguments into network format and place it into output queue.
**
**  @param command  Command (Move,Attack,...).
**  @param unit     Unit that receive the command.
**  @param x        optional X map position.
**  @param y        optional y map position.
**  @param dest     optional destination unit.
**  @param type     optional unit-type argument.
**  @param status   Append command or flush old commands.
**
**  @warning  Destination and unit-type shares the same network slot.
*/
void NetworkSendCommand(int command, const CUnit &unit, int x, int y,
						const CUnit *dest, const CUnitType *type, int status)
{
	CNetworkCommandQueue ncq;

	ncq.Time = GameCycle;
	ncq.Type = command;
	if (status) {
		ncq.Type |= 0x80;
	}
	ncq.Data.Unit = htons(UnitNumber(unit));
	ncq.Data.X = htons(x);
	ncq.Data.Y = htons(y);
	Assert(!dest || !type); // Both together isn't allowed
	if (dest) {
		ncq.Data.Dest = htons(UnitNumber(*dest));
	} else if (type) {
		ncq.Data.Dest = htons(type->Slot);
	} else {
		ncq.Data.Dest = htons(0xFFFF); // -1
	}
	// Check for duplicate command in queue
	std::list<CNetworkCommandQueue *>::iterator it;
	for (it = CommandsIn.begin(); it != CommandsIn.end(); ++it) {
		if (**it == ncq) {
			return;
		}
	}
	CNetworkCommandQueue *pncq = AllocNCQ();
	*pncq = ncq;
	CommandsIn.push_back(pncq);
}

/**
**  Prepare send of extended command message.
**
**  Convert arguments into network format and place it into output queue.
**
**  @param command  Command (Move,Attack,...).
**  @param arg1     optional argument #1
**  @param arg2     optional argument #2
**  @param arg3     optional argument #3
**  @param arg4     optional argument #4
**  @param status   Append command or flush old commands.
*/
void NetworkSendExtendedCommand(int command, int arg1, int arg2, int arg3,
								int arg4, int status)
{
	CNetworkCommandQueue *ncq = AllocNCQ();
	CommandsIn.push_back(ncq);

	ncq->Time = GameCycle;
	CNetworkExtendedCommand *nec = (CNetworkExtendedCommand *)&ncq->Data;

	ncq->Type = MessageExtendedCommand;
	if (status) {
		ncq->Type |= 0x80;
	}
	nec->ExtendedType = command;
	nec->Arg1 = arg1;
	nec->Arg2 = htons(arg2);
	nec->Arg3 = htons(arg3);
	nec->Arg4 = htons(arg4);
}

/**
**  Sends my selections to teammates
**
**  @param units  Units to send
**  @param count  Number of units to send
*/
void NetworkSendSelection(CUnit **units, int count)
{
	int teammates[PlayerMax];

	// Check if we have any teammates to send to
	int numteammates = 0;
	for (int i = 0; i < HostsCount; ++i) {
		if (Players[Hosts[i].PlyNr].Team == ThisPlayer->Team) {
			teammates[numteammates++] = i;
		}
	}
	if (!numteammates) {
		return;
	}
	// Build and send packets to cover all units.
	CNetworkPacket packet;
	int unitcount = 0;
	while (unitcount < count) {
		NetworkSelectionHeader &header = (NetworkSelectionHeader &) packet.Header;
		if (unitcount == 0) {
			header.Add = 0;
		} else {
			header.Add = 1;
		}
		header.Remove = 0;

		int nosent = 0;
		int i;
		for (i = 0; i < MaxNetworkCommands && unitcount < count; ++i) {
			header.Type[i] = MessageSelection;
			CNetworkSelection &selection = (CNetworkSelection &)packet.Command[i];
			for (int ref = 0; ref < 4 && unitcount < count; ++ref, ++unitcount) {
				selection.Unit[ref] = htons(UnitNumber(*units[unitcount]));
				++nosent;
			}
		}
		if (unitcount >= count) {
			// This is the last command
			header.NumberSent = nosent;
		} else {
			header.NumberSent = MaxNetworkCommands * 4;
		}
		for (; i < MaxNetworkCommands; ++i) {
			packet.Header.Type[i] = MessageNone;
		}
		// Send the Constructed packet to team members
		const int numcommands = (nosent + 3) / 4;
		unsigned char *buf = new unsigned char [CNetworkPacket::Size(numcommands)];
		packet.Serialize(buf, numcommands);
		const int len = CNetworkPacketHeader::Size() + CNetworkSelection::Size() * numcommands;

		for (int i = 0; i < numteammates; ++i) {
			const CHost host(Hosts[teammates[i]].Host, Hosts[teammates[i]].Port);
			NetworkFildes.Send(host, buf, len);
		}
		delete [] buf;
	}
}

/**
**  Process Received Unit Selection
**
**  @param packet  Network Packet to Process
**  @param player  Player number
*/
static void NetworkProcessSelection(const CNetworkPacket &packet, int player)
{
	const NetworkSelectionHeader &header = reinterpret_cast<const NetworkSelectionHeader &>(packet.Header);
	const size_t count = header.NumberSent;
	const int adjust = (header.Add << 1) | header.Remove;
	std::vector<CUnit *> units;

	for (int i = 0; header.Type[i] == MessageSelection; ++i) {
		const CNetworkSelection &selection = reinterpret_cast<const CNetworkSelection &>(packet.Command[i]);

		for (int j = 0; j < 4 && units.size() < count; ++j) {
			units.push_back(&UnitManager.GetSlotUnit(ntohs(selection.Unit[j])));
		}
	}
	Assert(count == units.size());

	ChangeTeamSelectedUnits(Players[player], units, adjust);
}

/**
**  Remove a player from the game.
**
**  @param player  Player number
*/
static void NetworkRemovePlayer(int player)
{
	// Remove player from Hosts and clear NetworkIn
	for (int i = 0; i < HostsCount; ++i) {
		if (Hosts[i].PlyNr == player) {
			Hosts[i] = Hosts[HostsCount - 1];
			--HostsCount;
			break;
		}
	}
	for (int i = 0; i < 256; ++i) {
		for (int c = 0; c < MaxNetworkCommands; ++c) {
			NetworkIn[i][player][c].Time = 0;
		}
	}
}

static bool IsNetworkCommandReady(unsigned long gameCycle)
{
	// Check if all next messages are available.
	const CNetworkCommandQueue (&ncqs)[PlayerMax][MaxNetworkCommands] = NetworkIn[gameCycle & 0xFF];
	for (int i = 0; i < HostsCount; ++i) {
		const CNetworkCommandQueue *ncq = ncqs[Hosts[i].PlyNr];

		if (ncq[0].Time != gameCycle) {
			return false;
		}
	}
	return true;
}

static void NetworkParseInGameEvent(unsigned char *buf, int len, const CHost &host)
{
	CNetworkPacket packet;
	int commands = packet.Deserialize(buf, len);
	if (commands < 0) {
		DebugPrint("Bad packet read\n");
		return;
	}
	const int index = FindHostIndexBy(host);
	if (index == -1 || PlayerQuit[Hosts[index].PlyNr]) {
#ifdef DEBUG
		const std::string hostStr = host.toString();
		DebugPrint("Not a host in play: %s\n" _C_ hostStr.c_str());
#endif
		return;
	}
	const int player = Hosts[index].PlyNr;

	// In a normal packet there is a least sync, selection may not have that
	if (packet.Header.Type[0] == MessageSelection || commands == 0) {
		NetworkProcessSelection(packet, player);
		return;
	}
	// Parse the packet commands.
	for (int i = 0; i != commands; ++i) {
		const CNetworkCommand &nc = packet.Command[i];
		bool validCommand = false;

		// Handle some messages.
		if (packet.Header.Type[i] == MessageQuit) {
			const int playerNum = ntohs(nc.X);

			if (playerNum >= 0 && playerNum < NumPlayers) {
				PlayerQuit[playerNum] = 1;
				validCommand = true;
			}
		}
		if (packet.Header.Type[i] == MessageResend) {
			// Destination cycle (time to execute).
			unsigned long n = ((GameCycle + 128) & ~0xFF) | packet.Header.Cycle;
			if (n > GameCycle + 128) {
				n -= 0x100;
			}
			// FIXME: not necessary to send this packet multiple times!!!!
			// other side sends re-send until it gets an answer.
			if (n != NetworkIn[n & 0xFF][ThisPlayer->Index][0].Time) {
				// Asking for a cycle we haven't gotten to yet, ignore for now
				return;
			}
			NetworkSendPacket(NetworkIn[n & 0xFF][ThisPlayer->Index]);
			// Check if a player quit this cycle
			for (int j = 0; j < HostsCount; ++j) {
				for (int c = 0; c < MaxNetworkCommands; ++c) {
					const CNetworkCommandQueue *ncq;
					ncq = &NetworkIn[n & 0xFF][Hosts[j].PlyNr][c];
					if (ncq->Time && ncq->Type == MessageQuit) {
						CNetworkPacket np;
						np.Header.Cycle = ncq->Time & 0xFF;
						np.Header.Type[0] = ncq->Type;
						np.Command[0] = ncq->Data;
						for (int k = 1; k < MaxNetworkCommands; ++k) {
							np.Header.Type[k] = MessageNone;
						}
						NetworkBroadcast(np, 1);
					}
				}
			}
			return;
		}
		// Destination cycle (time to execute).
		unsigned long n = ((GameCycle + 128) & ~0xFF) | packet.Header.Cycle;
		if (n > GameCycle + 128) {
			n -= 0x100;
		}
		// Receive statistic
		NetworkLastFrame[player] = FrameCounter;

		// Place in network in
		switch (packet.Header.Type[i] & 0x7F) {
			case MessageExtendedCommand:
				// FIXME: ensure the sender is part of the command
				validCommand = true;
				break;
			case MessageSync:
				// Sync does not matter
				validCommand = true;
				break;
			case MessageQuit:
			case MessageQuitAck:
			case MessageResend:
			case MessageChat:
			case MessageChatTerm:
				// FIXME: ensure it's from the right player
				validCommand = true;
				break;
			case MessageCommandDismiss: // Fall through!
			default: {
				const unsigned int slot = ntohs(nc.Unit);
				const CUnit *unit = slot < UnitManager.GetUsedSlotCount() ? &UnitManager.GetSlotUnit(slot) : NULL;

				if (unit && (unit->Player->Index == player
							 || Players[player].IsTeamed(*unit))) {
					validCommand = true;
				} else {
					validCommand = false;
				}
			}
		}
		// FIXME: not all values in nc have been validated
		if (validCommand) {
			NetworkIn[packet.Header.Cycle][player][i].Time = n;
			NetworkIn[packet.Header.Cycle][player][i].Type = packet.Header.Type[i];
			NetworkIn[packet.Header.Cycle][player][i].Data = nc;
		} else {
			SetMessage(_("%s sent bad command"), Players[player].Name.c_str());
			DebugPrint("%s sent bad command: 0x%x\n" _C_ Players[player].Name.c_str()
					   _C_ packet.Header.Type[i] & 0x7F);
		}
	}
	for (int i = commands; i != MaxNetworkCommands; ++i) {
		NetworkIn[packet.Header.Cycle][player][i].Time = 0;
	}
	// Waiting for this time slot
	if (!NetworkInSync) {
		unsigned long n = (GameCycle / NetworkUpdates) * NetworkUpdates + NetworkUpdates;
		if (IsNetworkCommandReady(n) == true) {
			NetworkInSync = 1;
		}
	}
}

/**
**  Called if message for the network is ready.
**  (by WaitEventsOneFrame)
**
**  @todo
**  NetworkReceivedEarly NetworkReceivedLate NetworkReceivedDups
**  Must be calculated.
*/
void NetworkEvent()
{
	if (!IsNetworkGame()) {
		NetworkInSync = 1;
		return;
	}
	// Read the packet.
	unsigned char buf[1024];
	CHost host;
	int len = NetworkFildes.Recv(&buf, sizeof(buf), &host);
	if (len < 0) {
		DebugPrint("Server/Client gone?\n");
		// just hope for an automatic recover right now..
		NetworkInSync = 0;
		return;
	}
#ifdef DEBUG
	++NetworkReceivedPackets;
#endif
	// Setup messages
	if (NetConnectRunning) {
		if (NetworkParseSetupEvent(buf, len, host)) {
			return;
		}
	}
	NetworkParseInGameEvent(buf, len, host);
}

/**
**  Quit the game.
*/
void NetworkQuit()
{
	if (!ThisPlayer) {
		return;
	}
	const int n = (GameCycle + NetworkUpdates) / NetworkUpdates * NetworkUpdates + NetworkLag;
	CNetworkCommandQueue (&ncqs)[MaxNetworkCommands] = NetworkIn[n & 0xFF][ThisPlayer->Index];
	ncqs[0].Type = MessageQuit;
	ncqs[0].Time = n;
	ncqs[0].Data.X = ThisPlayer->Index;
	for (int i = 1; i < MaxNetworkCommands; ++i) {
		ncqs[i].Type = MessageNone;
	}
	NetworkSendPacket(ncqs);
}

/**
**  Send chat message. (Message is sent with low priority)
**
**  @param msg  Text message to send.
*/
void NetworkChatMessage(const std::string &msg)
{
	if (!IsNetworkGame()) {
		return;
	}
	const char *cp = msg.c_str();
	size_t n = msg.size();
	CNetworkChat *ncm = NULL;
	while (n >= sizeof(ncm->Text)) {
		CNetworkCommandQueue *ncq = AllocNCQ();
		MsgCommandsIn.push_back(ncq);
		ncq->Type = MessageChat;
		ncm = (CNetworkChat *)(&ncq->Data);
		ncm->Player = ThisPlayer->Index;
		memcpy(ncm->Text, cp, sizeof(ncm->Text));
		cp += sizeof(ncm->Text);
		n -= sizeof(ncm->Text);
	}
	CNetworkCommandQueue *ncq = AllocNCQ();
	MsgCommandsIn.push_back(ncq);
	ncq->Type = MessageChatTerm;
	ncm = (CNetworkChat *)(&ncq->Data);
	ncm->Player = ThisPlayer->Index;
	memcpy(ncm->Text, cp, n + 1); // see >= above :)
}

static void ParseNetworkCommand_Sync(const CNetworkCommandQueue &ncq)
{
	Assert((ncq.Type & 0x7F) == MessageSync);

	int ply = ntohs(ncq.Data.X) << 16;
	ply |= ntohs(ncq.Data.Y);
	if (ply != NetworkSyncSeeds[GameCycle & 0xFF]
		|| ntohs(ncq.Data.Unit) != NetworkSyncHashs[GameCycle & 0xFF]) {
		SetMessage("%s", _("Network out of sync"));
		DebugPrint("\nNetwork out of sync %x!=%x! %d!=%d! Cycle %lu\n\n" _C_
				   ply _C_ NetworkSyncSeeds[GameCycle & 0xFF] _C_
				   ntohs(ncq.Data.Unit) _C_ NetworkSyncHashs[GameCycle & 0xFF] _C_ GameCycle);
	}
}

static void ParseNetworkCommand_Chat(const CNetworkCommandQueue &ncq)
{
	const CNetworkChat &ncm = reinterpret_cast<const CNetworkChat &>(ncq.Data);
	const int ply = ncm.Player;

	if (NetMsgBufLen[ply] + sizeof(ncm.Text) < 128) {
		memcpy(NetMsgBuf[ply] + NetMsgBufLen[ply], ncm.Text, sizeof(ncm.Text));
	}
	NetMsgBufLen[ply] += sizeof(ncm.Text);
	if (ncq.Type == MessageChatTerm) {
		NetMsgBuf[ply][127] = '\0';
		SetMessage("%s", NetMsgBuf[ply]);
		PlayGameSound(GameSounds.ChatMessage.Sound, MaxSampleVolume);
		CommandLog("chat", NoUnitP, FlushCommands, -1, -1, NoUnitP, NetMsgBuf[ply], -1);
		NetMsgBufLen[ply] = 0;
	}
}

/**
**  Parse a network command.
**
**  @param ncq  Network command from queue
*/
static void ParseNetworkCommand(const CNetworkCommandQueue &ncq)
{
	switch (ncq.Type & 0x7F) {
		case MessageSync: ParseNetworkCommand_Sync(ncq); break;

		case MessageChat: // Follow
		case MessageChatTerm: ParseNetworkCommand_Chat(ncq); break;

		case MessageQuit:
			NetworkRemovePlayer(ncq.Data.X);
			CommandLog("quit", NoUnitP, FlushCommands, ncq.Data.X, -1, NoUnitP, NULL, -1);
			CommandQuit(ncq.Data.X);
			break;
		case MessageExtendedCommand: {
			const CNetworkExtendedCommand *nec = (CNetworkExtendedCommand *)(&ncq.Data);
			ParseExtendedCommand(nec->ExtendedType, (ncq.Type & 0x80) >> 7,
								 nec->Arg1, ntohs(nec->Arg2), ntohs(nec->Arg3), ntohs(nec->Arg4));
		}
		break;
		case MessageNone:
			// Nothing to Do, This Message Should Never be Executed
			Assert(0);
			break;
		default:
			ParseCommand(ncq.Type, ntohs(ncq.Data.Unit),
						 ntohs(ncq.Data.X), ntohs(ncq.Data.Y), ntohs(ncq.Data.Dest));
			break;
	}
}

/**
**  Network send commands.
*/
static void NetworkSendCommands(unsigned long gameCycle)
{
	// No command available, send sync.
	int numcommands = 0;
	CNetworkCommandQueue (&ncq)[MaxNetworkCommands] = NetworkIn[gameCycle & 0xFF][ThisPlayer->Index];
	ncq->Clear();
	if (CommandsIn.empty() && MsgCommandsIn.empty()) {
		ncq[0].Type = MessageSync;
		ncq[0].Data.Unit = htons(SyncHash & 0xFFFF);
		ncq[0].Data.X = htons(SyncRandSeed >> 16);
		ncq[0].Data.Y = htons(SyncRandSeed & 0xFFFF);
		ncq[0].Time = gameCycle;
		numcommands = 1;
	} else {
		while (!CommandsIn.empty() && numcommands < MaxNetworkCommands) {
			CNetworkCommandQueue *incommand = CommandsIn.front();
#ifdef DEBUG
			if (incommand->Type != MessageExtendedCommand) {
				CUnit &unit = UnitManager.GetSlotUnit(ntohs(ncq->Data.Unit));
				// FIXME: we can send destoyed units over network :(
				if (unit.Destroyed) {
					DebugPrint("Sending destroyed unit %d over network!!!!!!\n" _C_
							   ntohs(incommand->Data.Unit));
				}
			}
#endif
			ncq[numcommands] = *incommand;
			ncq[numcommands].Time = gameCycle;
			++numcommands;
			CommandsIn.pop_front();
			FreeNCQ(incommand);
		}
		while (!MsgCommandsIn.empty() && numcommands < MaxNetworkCommands) {
			CNetworkCommandQueue *incommand = MsgCommandsIn.front();
			MsgCommandsIn.pop_front();
			ncq[numcommands] = *incommand;
			ncq[numcommands].Time = gameCycle;
			++numcommands;
			FreeNCQ(incommand);
		}
	}
	if (numcommands != MaxNetworkCommands) {
		ncq[numcommands].Type = MessageNone;
	}
	NetworkSendPacket(ncq);

	NetworkSyncSeeds[gameCycle & 0xFF] = SyncRandSeed;
	NetworkSyncHashs[gameCycle & 0xFF] = SyncHash & 0xFFFF; // FIXME: 32bit later
}

/**
**  Network excecute commands.
*/
static void NetworkExecCommands(unsigned long gameCycle)
{
	// Must execute commands on all computers in the same order.
	for (int i = 0; i < NumPlayers; ++i) {
		// Remove commands.
		const CNetworkCommandQueue *ncqs = NetworkIn[gameCycle & 0xFF][i];
		for (int c = 0; c < MaxNetworkCommands; ++c) {
			const CNetworkCommandQueue &ncq = ncqs[c];
			if (ncq.Type == MessageNone) {
				break;
			}
			if (ncq.Time) {
#ifdef DEBUG
				if (ncq.Time != gameCycle) {
					DebugPrint("cycle %lu idx %lu time %lu\n" _C_
							   gameCycle _C_ gameCycle & 0xFF _C_ ncq.Time);
					Assert(ncq.Time == gameCycle);
				}
#endif
				ParseNetworkCommand(ncq);
			}
		}
	}
}

/**
**  Handle network commands.
*/
void NetworkCommands()
{
	if (!IsNetworkGame()) {
		return;
	}
	if ((GameCycle % NetworkUpdates) != 0) {
		return;
	}
	// Send messages to all clients (other players)
	NetworkSendCommands(GameCycle + NetworkLag);
	NetworkExecCommands(GameCycle);
	if (IsNetworkCommandReady(GameCycle + NetworkUpdates) == false) {
		NetworkInSync = 0;
		NetworkDelay = FrameCounter + NetworkUpdates;
		// FIXME: should send a resend request.
	}
}

/**
**  Network resend commands, we have a missing packet send to all clients
**  what packet we are missing.
**
**  @todo
**  We need only send to the clients, that have not delivered the packet.
*/
static void NetworkResendCommands()
{
#ifdef DEBUG
	++NetworkSendResend;
#endif

	// Build packet
	CNetworkPacket packet;
	packet.Header.Type[0] = MessageResend;
	packet.Header.Type[1] = MessageNone;
	packet.Header.Cycle =
		(uint8_t)((GameCycle / NetworkUpdates) * NetworkUpdates + NetworkUpdates);

	NetworkBroadcast(packet, 1);
}

/**
**  Recover network.
*/
void NetworkRecover()
{
	if (HostsCount == 0) {
		NetworkInSync = 1;
		return;
	}
	if (FrameCounter <= NetworkDelay) {
		return;
	}
	NetworkDelay += NetworkUpdates;

	// Check for players that timed out
	for (int i = 0; i < HostsCount; ++i) {
		if (!NetworkLastFrame[Hosts[i].PlyNr]) {
			continue;
		}
		int secs = (FrameCounter - NetworkLastFrame[Hosts[i].PlyNr]) /
				   (FRAMES_PER_SECOND * VideoSyncSpeed / 100);
		// FIXME: display a menu while we wait
		if (secs >= 3 && secs < NetworkTimeout) {
			if (FrameCounter % FRAMES_PER_SECOND < (unsigned long)NetworkUpdates) {
				SetMessage(_("Waiting for player \"%s\": %d:%02d"), Hosts[i].PlyName,
						   (NetworkTimeout - secs) / 60, (NetworkTimeout - secs) % 60);
			}
		}
		if (secs >= NetworkTimeout) {
			CNetworkCommand nc;
			CNetworkPacket np;

			unsigned long n = GameCycle + NetworkUpdates;
			nc.X = Hosts[i].PlyNr;
			NetworkIn[n & 0xFF][Hosts[i].PlyNr][0].Time = n;
			NetworkIn[n & 0xFF][Hosts[i].PlyNr][0].Type = MessageQuit;
			NetworkIn[n & 0xFF][Hosts[i].PlyNr][0].Data = nc;
			PlayerQuit[Hosts[i].PlyNr] = 1;
			SetMessage("%s", _("Timed out"));

			const CNetworkCommandQueue *ncq = &NetworkIn[n & 0xFF][Hosts[i].PlyNr][0];
			np.Header.Cycle = ncq->Time & 0xFF;
			np.Header.Type[0] = ncq->Type;
			np.Header.Type[1] = MessageNone;

			NetworkBroadcast(np, 1);

			if (IsNetworkCommandReady(GameCycle + NetworkUpdates) == false) {
				NetworkInSync = 0;
				NetworkDelay = FrameCounter + NetworkUpdates;
				// FIXME: should send a resend request.
			}
		}
	}
	// Resend old commands
	NetworkResendCommands();
}

//@}
