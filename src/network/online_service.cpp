#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

#ifdef USE_WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "game.h"
#include "parameters.h"
#include "assert.h"
#include "network.h"
#include "network/netsockets.h"
#include "util.h"
#include "version.h"

#include "./xsha1.h"


class BNCSInputStream {
public:
    BNCSInputStream(CTCPSocket *socket) {
        this->sock = socket;
        this->bufsize = 1024;
        this->buffer = (char*)calloc(sizeof(char), bufsize);
        this->avail = 0;
        this->pos = 0;
    };
    ~BNCSInputStream() {};

    std::string readString() {
        if (avail == 0) {
            return NULL;
        }
        std::stringstream strstr;
        int i = pos;
        char c;
        while ((c = buffer[i]) != '\0' && i < avail) {
            strstr.put(c);
            i += 1;
        }
        consumeData(i);
        return strstr.str();
    };

    std::vector<std::string> readStringlist() {
        std::vector<std::string> stringlist;
        while (true) {
            std::string nxt = readString();
            if (nxt.empty()) {
                break;
            } else {
                stringlist.push_back(nxt);
            }
        }
        return stringlist;
    };

    uint8_t read8() {
        uint8_t byte = buffer[pos];
        consumeData(1);
        return byte;
    }

    uint16_t read16() {
        uint16_t byte = ntohs(reinterpret_cast<uint16_t *>(buffer + pos)[0]);
        consumeData(2);
        return byte;
    }

    uint32_t read32() {
        uint32_t byte = ntohs(reinterpret_cast<uint32_t *>(buffer + pos)[0]);
        consumeData(4);
        return byte;
    }

    uint64_t read64() {
        uint64_t byte = ntohs(reinterpret_cast<uint64_t *>(buffer + pos)[0]);
        consumeData(8);
        return byte;
    }

    bool readBool8() {
        return read8() != 0;
    }

    bool readBool32() {
        return read32() != 0;
    }

    uint64_t readFiletime() {
        return read64();
    }

    std::string string32() {
        // uint32 encoded (4-byte) string
        uint32_t data = read32();
        char dt[5];
        strncpy(dt, (const char*)&data, 4);
        dt[4] = '\0';
        return std::string(dt);
    };

    /**
     * To be called at the start of a message, gets the entire data into memory or returns -1.
     */
    uint8_t readMessageId() {
        // Every BNCS message has the same header:
        //  (UINT8) Always 0xFF
        //  (UINT8) Message ID
        // (UINT16) Message length, including this header
        //   (VOID) Message data
        avail += this->sock->Recv(buffer + avail, 4 - avail);
        if (avail < 4) {
            return -1;
        }
        assert(read8() == 0xff);
        uint8_t msgId = read8();
        uint16_t len = read16();
        avail += this->sock->Recv(buffer + avail, len - 4);
        if (avail < len) {
            // Didn't receive full message on the socket, yet. Reset position so
            // this method can be used to try again
            pos = 0;
            return -1;
        } else {
            return 0;
        }
    };

private:
    void consumeData(int bytes) {
        pos += bytes;
    }

    CTCPSocket *sock;
    char *buffer;
    int avail;
    int pos;
    int bufsize;
};

class BNCSOutputStream {
public:
    BNCSOutputStream(uint8_t id) {
        // Every BNCS message has the same header:
        //  (UINT8) Always 0xFF
        //  (UINT8) Message ID
        // (UINT16) Message length, including this header
        //   (VOID) Message data
        this->sz = 16;
        this->buf = (uint8_t*) calloc(sizeof(uint8_t), this->sz);
        this->pos = 0;
        serialize8(0xff);
        serialize8(id);
        this->length_pos = pos;
        serialize16((uint16_t)0);
    };
    ~BNCSOutputStream() {
        free(buf);
    };

    void serialize32(uint32_t data) {
        ensureSpace(sizeof(data));
        uint32_t *view = reinterpret_cast<uint32_t *>(buf + pos);
        *view = htonl(data);
        pos += sizeof(data);
    };
    void serialize32(int32_t data) {
        ensureSpace(sizeof(data));
        int32_t *view = reinterpret_cast<int32_t *>(buf + pos);
        *view = htonl(data);
        pos += sizeof(data);
    };
    void serialize16(uint16_t data) {
        ensureSpace(sizeof(data));
        uint16_t *view = reinterpret_cast<uint16_t *>(buf + pos);
        *view = htons(data);
        pos += sizeof(data);
    };
    void serialize16(int16_t data) {
        ensureSpace(sizeof(data));
        uint16_t *view = reinterpret_cast<uint16_t *>(buf + pos);
        *view = htons(data);
        pos += sizeof(data);
    };
    void serialize8(uint8_t data) {
        ensureSpace(sizeof(data));
        *(buf + pos) = data;
        pos++;
    };
    void serialize(const char* str, int len) {
        ensureSpace(len);
        memcpy(buf + pos, str, len);
        pos += len;
    };
    void serialize(const char* str) {
        int len = strlen(str);
        ensureSpace(len);
        memcpy(buf + pos, str, len);
        pos += len;
    };

    int flush(CTCPSocket *sock) {
        return sock->Send(getBuffer(), pos);
    };

    void flush(CUDPSocket *sock, CHost *host) {
        sock->Send(*host, getBuffer(), pos);
    };

private:
    uint8_t *getBuffer() {
        // insert length to make it a valid buffer
        uint16_t *view = reinterpret_cast<uint16_t *>(buf + length_pos);
        *view = htons(pos);
        return buf;
    };

    void ensureSpace(size_t required) {
        if (pos + required < sz) {
            sz = sz * 2;
            buf = (uint8_t*) realloc(buf, sz);
            assert(buf != NULL);
        }
    }

    uint8_t *buf;
    int sz;
    int pos;
    int length_pos;
};

class Context;
class NetworkState {
public:
    virtual ~NetworkState() {};
    virtual void doOneStep(Context *ctx) = 0;

protected:
    int send(Context *ctx, BNCSOutputStream *buf);
};

class Context {
public:
    Context() {
        this->udpSocket = new CUDPSocket();
        this->tcpSocket = new CTCPSocket();
        this->istream = new BNCSInputStream(tcpSocket);
        this->state = NULL;
        this->host = new CHost("127.0.0.1", 6112); // TODO: parameterize
        this->clientToken = MyRand();
        this->username = "";
        this->info = "";
    }

    ~Context() {
        if (state != NULL) {
            delete state;
        }
        delete udpSocket;
        delete tcpSocket;
        delete host;
    }

    std::string getInfo() { return info; }

    void setInfo(std::string arg) { info = arg; }

    std::string getUsername() { return username; }

    void setUsername(std::string arg) { username = arg; }

    uint32_t* getPassword2() {
        // we assume that any valid password has at least 1 non-null word hash
        for (int i = 0; i < 5; i++) {
            if (password[i] != 0) {
                return password2;
            }
        }
        return NULL;
    }

    uint32_t* getPassword1() {
        // we assume that any valid password has at least 1 non-null word hash
        for (int i = 0; i < 5; i++) {
            if (password[i] != 0) {
                return password;
            }
        }
        return NULL;
    }

    void setPassword(std::string pw) {
        xsha1_calcHashBuf(pw.c_str(), pw.length(), password);
        xsha1_calcHashDat(password, password2);
    }

    CHost *getHost() { return host; }

    void setHost(CHost *arg) {
        if (host != NULL) {
            delete host;
        }
        host = arg;
    }

    CUDPSocket *getUDPSocket() { return udpSocket; }

    CTCPSocket *getTCPSocket() { return tcpSocket; }

    BNCSInputStream *getMsgIStream() { return istream; }

    void doOneStep() { this->state->doOneStep(this); }

    void setState(NetworkState* newState) {
        assert (newState != this->state);
        if (this->state != NULL) {
            delete this->state;
        }
        this->state = newState;
    }

    uint32_t clientToken;
    uint32_t serverToken;

private:
    NetworkState *state;
    CHost *host;
    CUDPSocket *udpSocket;
    CTCPSocket *tcpSocket;
    BNCSInputStream *istream;

    std::string info;
    std::string username;
    uint32_t password[5]; // xsha1 hash of password
    uint32_t password2[5]; // xsha1 hash of password hash
};

int NetworkState::send(Context *ctx, BNCSOutputStream *buf) {
    return buf->flush(ctx->getTCPSocket());
}

class DisconnectedState : public NetworkState {
public:
    DisconnectedState(std::string message) {
        this->message = message;
        this->hasPrinted = false;
    };

    virtual void doOneStep(Context *ctx) {
        if (!hasPrinted) {
            std::cout << message << std::endl;
            hasPrinted = true;
        }
        // the end
    }

private:
    bool hasPrinted;
    std::string message;
};

/* needed
C>S 0x02 SID_STOPADV

C>S 0x09 SID_GETADVLISTEX
S>C 0x09 SID_GETADVLISTEX

C>S 0x0E SID_CHATCOMMAND

S>C 0x0F SID_CHATEVENT

C>S 0x1C SID_STARTADVEX3
S>C 0x1C SID_STARTADVEX3

C>S 0x22 SID_NOTIFYJOIN

C>S 0x3D SID_CREATEACCOUNT2
S>C 0x3D SID_CREATEACCOUNT2

C>S 0x2C SID_GAMERESULT

C>S 0x2F SID_FINDLADDERUSER
S>C 0x2F SID_FINDLADDERUSER

C>S 0x26 SID_READUSERDATA
S>C 0x26 SID_READUSERDATA

C>S 0x31 SID_CHANGEPASSWORD
S>C 0x31 SID_CHANGEPASSWORD

C>S 0x5A SID_RESETPASSWORD

C>S 0x65 SID_FRIENDSLIST
S>C 0x65 SID_FRIENDSLIST

*/

class Game {
public:
    Game(uint32_t settings, uint16_t port, uint32_t host, uint32_t status, uint32_t time, std::string name, std::string pw, std::string stats) {
        this->gameSettings = settings;
        this->host = CHost(host, port);
        this->gameStatus = status;
        this->elapsedTime = time;
        this->gameName = name;
        this->gamePassword = pw;
        this->gameStatstring = stats;
    }

    CHost getHost() { return host; }

    

    std::string getGameStatus() {
        switch (gameStatus) {
        case 0x0:
            return "OK";
        case 0x1:
            return "No such game";
        case 0x2:
            return "Wrong password";
        case 0x3:
            return "Game full";
        case 0x4:
            return "Game already started";
        case 0x5:
            return "Spawned CD-Key not allowed";
        case 0x6:
            return "Too many server requests";
        }
        return "Unknown status";
    }

    std::string getGameType() {
        switch (gameSettings & 0xff) {
        case 0x02:
            return "Melee";
        case 0x03:
            return "Free 4 All";
        case 0x04:
            return "One vs One";
        case 0x05:
            return "CTF";
        case 0x06:
            switch (gameSettings & 0xffff0000) {
            case 0x00010000:
                return "Greed 2500 resources";
            case 0x00020000:
                return "Greed 5000 resources";
            case 0x00030000:
                return "Greed 7500 resources";
            case 0x00040000:
                return "Greed 10000 resources";
            }
            return "Greed";
        case 0x07:
            switch (gameSettings & 0xffff0000) {
            case 0x00010000:
                return "Slaughter 15 minutes";
            case 0x00020000:
                return "Slaughter 30 minutes";
            case 0x00030000:
                return "Slaughter 45 minutes";
            case 0x00040000:
                return "Slaughter 60 minutes";
            }
            return "Slaughter";
        case 0x08:
            return "Sudden Death";
        case 0x09:
            switch (gameSettings & 0xffff0000) {
            case 0x00000000:
                return "Ladder (Disconnect is not loss)";
            case 0x00010000:
                return "Ladder (Loss on Disconnect)";
            }
            return "Ladder";
        case 0x0A:
            return "Use Map Settings";
        case 0x0B:
        case 0x0C:
        case 0x0D:
            std::string sub("");
            switch (gameSettings & 0xffff0000) {
            case 0x00010000:
                sub += " (2 teams)";
                break;
            case 0x00020000:
                sub += " (3 teams)";
                break;
            case 0x00030000:
                sub += " (4 teams)";
                break;                
            }
            switch (gameSettings & 0xff) {
            case 0x0B:
                return "Team Melee" + sub;
            case 0x0C:
                return "Team Free 4 All" + sub;
            case 0x0D:
                return "Team CTF" + sub;
            }
        case 0x0F:
            switch (gameSettings & 0xffff0000) {
            case 0x00010000:
                return "Top vs Bottom (1v7)";
            case 0x00020000:
                return "Top vs Bottom (2v6)";
            case 0x00030000:
                return "Top vs Bottom (3v5)";
            case 0x00040000:
                return "Top vs Bottom (4v4)";
            case 0x00050000:
                return "Top vs Bottom (5v3)";
            case 0x00060000:
                return "Top vs Bottom (6v2)";
            case 0x00070000:
                return "Top vs Bottom (7v1)";
            }
            return "Top vs Bottom";
        case 0x10:
            return "Iron Man Ladder";
        default:
            return "Unknown";
        }
    }

private:
    uint32_t gameSettings;
    uint32_t languageId;
    CHost host;
    uint32_t gameStatus;
    uint32_t elapsedTime;
    std::string gameName;
    std::string gamePassword;
    std::string gameStatstring;
}

class S2C_CHATEVENT : public NetworkState {
public:
    S2C_CHATEVENT() {
        this->ticks = 0;
    }

    virtual void doOneStep(Context *ctx) {
        ticks++;
        if ((ticks % 5000) == 0) {
            // C>S 0x07 PKT_KEEPALIVE
            // ~5000 frames @ ~50fps == 100 seconds
            BNCSOutputStream keepalive(0x07);
            keepalive.serialize32(ticks);
            keepalive.flush(ctx->getUDPSocket(), ctx->getHost());
        }

        if (ctx->getTCPSocket()->HasDataToRead(0)) {
            uint8_t msg = ctx->getMsgIStream()->readMessageId();
            if (msg == 0xff) {
                // try again next time
                return;
            }

            switch (msg) {
            case 0x00: // SID_NULL
                break;
            case 0x0f: // CHATEVENT
                handleChatevent(ctx);
                break;
            case 0x09:
                // S>C 0x09 SID_GETADVLISTEX
                handleGamelist(ctx);
            default:
                // S>C 0x68 SID_FRIENDSREMOVE
                // S>C 0x67 SID_FRIENDSADD
                std::cout << "Unhandled message ID: " << std::hex << msg << std::endl;
            }
        }
    }

private:
    void handleGamelist(Context *ctx) {
        uint32_t cnt = ctx->getMsgIStream()->read32();
        std::vector<std::tuple<CHost, std::string, uint32_t, std::string, std::string, std::string>> games();
        if (cnt == 0) {
            ctx->setGamelist(games);
            return;
        }
    }

    void handleChatevent(Context *ctx) {
        uint32_t eventId = ctx->getMsgIStream()->read32();
        uint32_t userFlags = ctx->getMsgIStream()->read32();
        uint32_t ping = ctx->getMsgIStream()->read32();
        uint32_t ip = ctx->getMsgIStream()->read32();
        uint32_t acn = ctx->getMsgIStream()->read32();
        uint32_t reg = ctx->getMsgIStream()->read32();
        std::string username = ctx->getMsgIStream()->readString();
        std::string text = ctx->getMsgIStream()->readString();
        switch (eventId) {
        case 0x01: // sent for user that is already in channel
            ctx->addUser(username);
            break;
        case 0x02: // user joined channel
            ctx->addUser(username);
            ctx->showInfo(username " joined");
            break;
        case 0x03: // user left channel
            ctx->rmUser(username);
            ctx->showInfo(username " left");
        case 0x04: // recv whisper
            ctx->showChat(username + " whispers: " + text);
            break;
        case 0x05: // recv chat
            ctx->showChat(username + ": " + text);
            break;
        case 0x06: // recv broadcast
            ctx->showChat("[BROADCAST]: " + text);
            break;
        case 0x07: // channel info
            break;
        case 0x09: // user flags update
            break;
        case 0x0a: // sent whisper
            break;
        case 0x0d: // channel full
            ctx->showInfo("Channel full");
            break;
        case 0x0e: // channel does not exist
            ctx->showInfo("Channel does not exist");
            break;
        case 0x0f: // channel is restricted
            ctx->showInfo("Channel restricted");
            break;
        case 0x12: // general info text
            ctx->showInfo("[INFO]: " + text);
            break;
        case 0x13: // error message
            ctx->showError("[ERROR]: " + text);
            break;
        case 0x17: // emote
            break;
        }
    }

    uint32_t ticks;
};

class S2C_GETCHANNELLIST : public NetworkState {
    virtual void doOneStep(Context *ctx) {
        if (ctx->getTCPSocket()->HasDataToRead(0)) {
            uint8_t msg = ctx->getMsgIStream()->readMessageId();
            if (msg == 0xff) {
                // try again next time
                return;
            }
            if (msg != 0x0b) {
                std::string error = std::string("Expected SID_GETCHANNELLIST, got msg id ");
                error += std::to_string(msg);
                ctx->setState(new DisconnectedState(error));
            }
            
            std::vector<std::string> channels = ctx->getMsgIStream()->readStringlist();
            ctx->setChannels(channels);

            BNCSOutputStream getadvlistex(0x09);
            getadvlistex.serialize16(0x00); // all games
            getadvlistex.serialize16(0x01); // no sub game type
            getadvlistex.serialize32(0xff80); // show all games
            getadvlistex.serialize32(0x00); // reserved field
            getadvlistex.serialize32(0xff); // return all games
            getadvlistex.serialize(""); // no game name
            getadvlistex.serialize(""); // no game pw
            getadvlistex.serialize(""); // no game statstring
            getadvlistex.flush(ctx->getTCPSocket());

            ctx->setState(new S2C_CHATEVENT());
        }
    }
};

class S2C_ENTERCHAT : public NetworkState {
    virtual void doOneStep(Context *ctx) {
        if (ctx->getTCPSocket()->HasDataToRead(0)) {
            uint8_t msg = ctx->getMsgIStream()->readMessageId();
            if (msg == 0xff) {
                // try again next time
                return;
            }
            if (msg != 0x0a) {
                std::string error = std::string("Expected SID_ENTERCHAT, got msg id ");
                error += std::to_string(msg);
                ctx->setState(new DisconnectedState(error));
            }

            std::string uniqueName = ctx->getMsgIStream()->readString();
            std::string statString = ctx->getMsgIStream()->readString();
            std::string accountName = ctx->getMsgIStream()->readString();

            ctx->setUsername(uniqueName);
            if (!statString.empty()) {
                ctx->setInfo("Statstring after logon: " + statString);
            }

            ctx->setState(new S2C_GETCHANNELLIST());
        }
    }
};

class C2S_ENTERCHAT : public NetworkState {
    virtual void doOneStep(Context *ctx) {
        // does all of enterchar, getchannellist, and first-join joinchannel
        BNCSOutputStream enterchat(0x0a);
        enterchat.serialize(ctx->getUsername().c_str());
        enterchat.serialize("");
        enterchat.flush(ctx->getTCPSocket());

        BNCSOutputStream getlist(0x0b);
        getlist.serialize32(0x00);
        getlist.flush(ctx->getTCPSocket());

        BNCSOutputStream join(0x0c);
        join.serialize32(0x01); // first-join
        join.serialize("ignored");
        join.flush(ctx->getTCPSocket());

        ctx->setState(new S2C_ENTERCHAT());
    }
};

class S2C_PKT_SERVERPING : public NetworkState {
public:
    S2C_PKT_SERVERPING() {
        this->retries = 0;
    };

    virtual void doOneStep(Context *ctx) {
        if (ctx->getUDPSocket()->HasDataToRead(1)) {
            // PKT_SERVERPING
            //  (UINT8) 0xFF
            //  (UINT8) 0x05
            // (UINT16) 8
            // (UINT32) UDP Code
            char buf[8];
            int received = ctx->getUDPSocket()->Recv(buf, 8, ctx->getHost());
            if (received == 8) {
                uint32_t udpCode = reinterpret_cast<uint32_t*>(buf)[1];
                BNCSOutputStream udppingresponse(0x14);
                udppingresponse.serialize32(udpCode);
                udppingresponse.flush(ctx->getTCPSocket());
            }
        } else {
            retries++;
            if (retries < 5000) {
                return;
            }
            // we're using a timeout of 1ms, so now we've been waiting at
            // the very least for 5 seconds... let's skip UDP then
        }
        ctx->setState(new C2S_ENTERCHAT());
    }

private:
    int retries;
};

class C2S_LOGONRESPONSE2 : public NetworkState {
    virtual void doOneStep(Context *ctx);
};

class S2C_LOGONRESPONSE2 : public NetworkState {
    virtual void doOneStep(Context *ctx) {
        if (ctx->getTCPSocket()->HasDataToRead(0)) {
            uint8_t msg = ctx->getMsgIStream()->readMessageId();
            if (msg == 0xff) {
                // try again next time
                return;
            }
            if (msg != 0x3a) {
                std::string error = std::string("Expected SID_LOGONRESPONSE2, got msg id ");
                error += std::to_string(msg);
                ctx->setState(new DisconnectedState(error));
            }

            uint32_t status = ctx->getMsgIStream()->read32();

            switch (status) {
            case 0x00:
                // success. we need to send SID_UDPPINGRESPONSE before entering chat
                ctx->setState(new S2C_PKT_SERVERPING());
                return;
            case 0x01:
            case 0x02:
                ctx->setInfo("Account does not exist or incorrect password");
                ctx->setPassword("");
                ctx->setState(new C2S_LOGONRESPONSE2());
                return;
            case 0x06:
                ctx->setInfo("Account closed: " + ctx->getMsgIStream()->readString());
                ctx->setState(new C2S_LOGONRESPONSE2());
                return;
            default:
                ctx->setState(new DisconnectedState("unknown logon response"));
            }
        }
    }
};

void C2S_LOGONRESPONSE2::doOneStep(Context *ctx) {
    std::string user = ctx->getUsername();
    uint32_t *pw = ctx->getPassword1(); // single-hashed for SID_LOGONRESPONSE2
    if (!user.empty() && pw) {
        BNCSOutputStream logon(0x3a);
        logon.serialize32(ctx->clientToken);
        logon.serialize32(ctx->serverToken);

        // Battle.net password hashes are hashed twice using XSHA-1. First, the
        // password is hashed by itself, then the following data is hashed again
        // and sent to Battle.net:
        // (UINT32) Client Token
        // (UINT32) Server Token
        // (UINT32)[5] First password hash
        uint32_t data[7];
        data[0] = ctx->clientToken;
        data[1] = ctx->serverToken;
        data[2] = pw[0];
        data[3] = pw[1];
        data[4] = pw[2];
        data[5] = pw[3];
        data[6] = pw[4];
        uint32_t sendHash[5];
        xsha1_calcHashDat(data, sendHash);
        for (int i = 0; i < 20; i++) {
            logon.serialize8(reinterpret_cast<uint8_t*>(sendHash)[i]);
        }
        logon.serialize(user.c_str());
        logon.flush(ctx->getTCPSocket());

        ctx->setState(new S2C_LOGONRESPONSE2());
    }
};

class S2C_SID_AUTH_CHECK : public NetworkState {
    virtual void doOneStep(Context *ctx) {
        if (ctx->getTCPSocket()->HasDataToRead(0)) {
            uint8_t msg = ctx->getMsgIStream()->readMessageId();
            if (msg == 0xff) {
                // try again next time
                return;
            }
            if (msg != 0x51) {
                std::string error = std::string("Expected SID_AUTH_CHECK, got msg id ");
                error += std::to_string(msg);
                ctx->setState(new DisconnectedState(error));
            }

            uint32_t result = ctx->getMsgIStream()->read32();
            std::string info = ctx->getMsgIStream()->readString();

            switch (result) {
            case 0x000:
                // Passed challenge
                ctx->setState(new C2S_LOGONRESPONSE2());
                return;
            case 0x100:
                // Old game version
                info = "Old game version: " + info;
                break;
            case 0x101:
                // Invalid version
                info = "Invalid version: " + info;
                break;
            case 0x102:
                // Game version must be downgraded
                info = "Game version must be downgraded: " + info;
                break;
            case 0x200:
                // Invalid CD key
                info = "Invalid CD Key: " + info;
                break;
            case 0x201:
                // CD Key in use
                info = "CD Key in use: " + info;
                break;
            case 0x202:
                // Banned key
                info = "Banned key: " + info;
                break;
            case 0x203:
                // Wrong product
                info = "Wrong product: " + info;
                break;
            default:
                // Invalid version code
                info = "Invalid version code: " + info;
            }
            ctx->setState(new DisconnectedState(info));
        }
    }
};

class S2C_SID_AUTH_INFO : public NetworkState {
    virtual void doOneStep(Context *ctx) {
        if (ctx->getTCPSocket()->HasDataToRead(0)) {
            uint8_t msg = ctx->getMsgIStream()->readMessageId();
            if (msg == 0xff) {
                // try again next time
                return;
            }
            if (msg != 0x50) {
                std::string error = std::string("Expected SID_AUTH_INFO, got msg id ");
                error += std::to_string(msg);
                ctx->setState(new DisconnectedState(error));
            }

            uint32_t logonType = ctx->getMsgIStream()->read32();
            assert(logonType == 0x00); // only support Broken SHA-1 logon for now
            uint32_t serverToken = ctx->getMsgIStream()->read32();
            ctx->serverToken = serverToken;
            uint32_t udpValue = ctx->getMsgIStream()->read32();
            uint64_t mpqFiletime = ctx->getMsgIStream()->readFiletime();
            std::string mpqFilename = ctx->getMsgIStream()->readString();
            std::string formula = ctx->getMsgIStream()->readString();

            // immediately respond with pkt_conntest2 udp msg
            BNCSOutputStream conntest(0x09);
            conntest.serialize32(serverToken);
            conntest.serialize32(udpValue);
            conntest.flush(ctx->getUDPSocket(), ctx->getHost());

            // immediately respond with SID_AUTH_CHECK
            BNCSOutputStream check(0x51);
            check.serialize32(ctx->clientToken);
            // EXE version (one UINT32 value, serialized in network byte order here)
            check.serialize8(StratagusPatchLevel2);
            check.serialize8(StratagusPatchLevel);
            check.serialize8(StratagusMinorVersion);
            check.serialize8(StratagusMajorVersion);
            // EXE hash - we use lua file checksums
            check.serialize32(FileChecksums);
            // no CD key, not a Spawn key
            check.serialize32(0);
            check.serialize32(0);
            // EXE information
            std::string exeInfo("");
            if (!FullGameName.empty()) {
                exeInfo += FullGameName;
            } else {
                if (!GameName.empty()) {
                    exeInfo += GameName;
                } else {
                    exeInfo += Parameters::Instance.applicationName;
                }
            }
            exeInfo += StratagusLastModifiedDate;
            exeInfo += " ";
            exeInfo += StratagusLastModifiedTime;
            exeInfo += " ";
            exeInfo += std::to_string(StratagusVersion);
            check.serialize(exeInfo.c_str());
            // Key owner name
            check.serialize(DESCRIPTION);
            check.flush(ctx->getTCPSocket());

            ctx->setState(new S2C_SID_AUTH_CHECK());
        }
    }
};

class S2C_SID_PING : public NetworkState {
    virtual void doOneStep(Context *ctx) {
        if (ctx->getTCPSocket()->HasDataToRead(0)) {
            uint8_t msg = ctx->getMsgIStream()->readMessageId();
            if (msg == 0xff) {
                // try again next time
                return;
            }
            if (msg != 0x25) {
                // not a ping
                std::string error = std::string("Expected SID_PING, got msg id ");
                error += std::to_string(msg);
                ctx->setState(new DisconnectedState(error));
            }
            uint32_t pingValue = ctx->getMsgIStream()->read32();

            // immediately respond with C2S_SID_PING
            BNCSOutputStream buffer(0x25);
            buffer.serialize32(pingValue);
            send(ctx, &buffer);

            ctx->setState(new S2C_SID_AUTH_INFO());
        }
    }
};

class ConnectState : public NetworkState {
    virtual void doOneStep(Context *ctx) {
        // Connect
        ctx->getTCPSocket()->Open(*ctx->getHost());
        if (ctx->getTCPSocket()->IsValid() == false) {
            ctx->setState(new DisconnectedState("TCP open failed"));
            return;
        }
        if (ctx->getTCPSocket()->Connect(*ctx->getHost()) == false) {
            ctx->setState(new DisconnectedState("TCP connect failed"));
            return;
        }
        // Send proto byte
        ctx->getTCPSocket()->Send("\x1", 1);

        // C>S SID_AUTH_INFO
        BNCSOutputStream buffer(0x50);
        // (UINT32) Protocol ID
        buffer.serialize32(0x00);
        // (UINT32) Platform code
        buffer.serialize("IX86", 4);
        // (UINT32) Product code - we'll just use W2BN and encode what we are in
        // the version
        buffer.serialize("W2BN", 4);
        // (UINT32) Version byte - just use the last from W2BN
        buffer.serialize32(0x4f);
        // (UINT32) Language code
        buffer.serialize32(0x00);
        // (UINT32) Local IP
        if (CNetworkParameter::Instance.localHost.compare("127.0.0.1")) {
            // user set a custom local ip, use that
            struct in_addr addr;
            inet_aton(CNetworkParameter::Instance.localHost.c_str(), &addr);
            buffer.serialize32(addr.s_addr);
        } else {
            unsigned long ips[20];
            int networkNumInterfaces = NetworkFildes.GetSocketAddresses(ips, 20);
            bool found_one = false;
            if (networkNumInterfaces) {
                // we can only advertise one, so take the first that fits in 32-bit and is thus presumably ipv4
                for (int i = 0; i < networkNumInterfaces; i++) {
                    uint32_t ip = ips[i];
                    if (ip == ips[i]) {
                        found_one = true;
                        buffer.serialize32(ip);
                        break;
                    }
                }
            }
            if (!found_one) {
                // use default
                buffer.serialize32(0x00);
            }
        }
        // (UINT32) Time zone bias
        uint32_t bias = 0;
        std::time_t systemtime = std::time(NULL);
        struct std::tm *utc = std::gmtime(&systemtime);
        if (utc != NULL) {
            std::time_t utctime_since_epoch = std::mktime(utc);
            if (utctime_since_epoch != -1) {
                struct std::tm *localtime = std::localtime(&systemtime);
                if (localtime != 0) {
                    std::time_t localtime_since_epoch = std::mktime(localtime);
                    if (localtime_since_epoch != -1) {
                        bias = std::difftime(utctime_since_epoch, localtime_since_epoch) / 60;
                    }
                }
            }
        }
        buffer.serialize32(bias);
        // (UINT32) MPQ locale ID
        buffer.serialize32(0x00);
        // (UINT32) User language ID
        buffer.serialize32(0x00);
        // (STRING) Country abbreviation
        buffer.serialize("USA");
        // (STRING) Country
        buffer.serialize("United States");

        send(ctx, &buffer);
        ctx->setState(new S2C_SID_PING());
    }
};

static void goOnline() {
    Context *ctx = new Context();
    ctx->setState(new ConnectState());
    while (true) {
        ctx->doOneStep();
        sleep(1);
    }
}
