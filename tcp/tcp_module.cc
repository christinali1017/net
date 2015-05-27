#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include <iostream>
#include "tcpstate.h"
#include "Minet.h"


using std::cout;
using std::endl;
using std::cerr;
using std::string;

// Send Data Functions
enum SendPacketType {
  SYN,
  FIN,
  SYNACK,
  ACK,
  RST,
  DATA,
  CONNECTFAIL
};
static void SendPacket(MinetHandle mnth, ConnectionList<TCPState>::iterator &cs, SendPacketType ptyp);
static Packet CreatePacket(ConnectionList<TCPState>::iterator &cs, unsigned char flags, char *sendBuf, unsigned int dataSize);
static Buffer FetchPayload(Packet p, unsigned int &datalen);
static bool PassPayloadToSock(MinetHandle mnth, ConnectionList<TCPState>::iterator &cs, Packet p);

// Set Timeout Event Triggers
static double PickupNextTimeout();
static int EstimateNewTimerInterval(Time newRTT);

// Response Timer Event
static void ResTimerLISTEN(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);
static void ResTimerSYNSENT(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);
static void ResTimerSYNRCVD(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);
static void ResTimerESTABLISHED(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);
static void ResTimerCLOSEWAIT(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);
static void ResTimerLASTACK(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);
static void ResTimerFINWAIT1(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);
static void ResTimerFINWAIT2(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);
static void ResTimerCLOSING(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);
static void ResTimerTIMEWAIT(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs);

// Response IP Event
static void ResIPStateLISTEN(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);
static void ResIPStateSYNSENT(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);
static void ResIPStateSYNRCVD(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);
static void ResIPStateESTABLISHED(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);
static void ResIPStateCLOSEWAIT(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);
static void ResIPStateLASTACK(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);
static void ResIPStateFINWAIT1(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);
static void ResIPStateFINWAIT2(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);
static void ResIPStateCLOSING(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);
static void ResIPStateTIMEWAIT(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist);

// Response Socket Event
static void ResSockConnect(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist);
static void ResSockAccept(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist);
static void ResSockStatus(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist);
static void ResSockWrite(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist);
static void ResSockForward(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist);
static void ResSockClose(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist);


int main(int argc, char *argv[])
{
  MinetHandle mux, sock;

  MinetInit(MINET_TCP_MODULE);

  mux=MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
  sock=MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

  if (MinetIsModuleInConfig(MINET_IP_MUX) && mux==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to mux"));
    return -1;
  }

  if (MinetIsModuleInConfig(MINET_SOCK_MODULE) && sock==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't accept from sock module"));
    return -1;
  }

  MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

  MinetEvent event;
  ConnectionList<TCPState> clist;
  

  while (MinetGetNextEvent(event, PickupNextTimeout())==0) {
    // if we received an unexpected type of event, print error
    if (event.eventtype!=MinetEvent::Dataflow 
      || event.direction!=MinetEvent::IN
      || event.eventtype!=MinetEvent::Timeout) {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    // if we received a valid event from Minet, do processing
    } if (event.eventtype == MinetEvent::Timeout) {
      ConnectionList<TCPState>::iterator cs = clist.FindEarliest();
      // each timeout just handle one Event
      if (cs != clist.end()) {
        switch ((*cs).state.GetState()) {
          case LISTEN :
            ResTimerLISTEN(mux, sock, cs);
            break;
          case SYN_SENT :
            ResTimerSYNSENT(mux, sock, cs);
            break;
          case SYN_RCVD :
            ResTimerSYNRCVD(mux, sock, cs);
            break;
          case ESTABLISHED :
            ResTimerESTABLISHED(mux, sock, cs);
            break;
          case CLOSE_WAIT :
            ResTimerCLOSEWAIT(mux, sock, cs);
          case LAST_ACK :
            ResTimerLASTACK(mux, sock, cs);
            break;
          case FIN_WAIT1 :
            ResTimerFINWAIT1(mux, sock, cs);
            break;
          case FIN_WAIT2 :
            ResTimerFINWAIT2(mux, sock, cs);
            break;
          case CLOSING :
            ResTimerCLOSING(mux, sock, cs);
            break;
          case TIME_WAIT :
            ResTimerTIMEWAIT(mux, sock, cs);
            break;
          default :
            cerr << "Sorry, I cannot handle right now. BYE!" << endl;
        }
      }
    } else {
      //  Data from the IP layer below  //
      if (event.handle==mux) {
        Packet p;
        MinetReceive(mux,p);
      	unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
      	//cerr << "estimated header len="<<tcphlen<<"\n";
      	p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
      	IPHeader iph=p.FindHeader(Headers::IPHeader);
      	TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
        // cerr << "TCP Packet: IP Header is "<<iph<<" and ";
        // cerr << "TCP Header is "<<tcph << " and ";
        // cerr << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID");

        Connection c;
        iph.GetDestIP(c.src);
        iph.GetSourceIP(c.dest);
        iph.GetProtocol(c.protocol);
        tcph.GetDestPort(c.srcport);
        tcph.GetSourcePort(c.destport);
        ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
        if (cs !=clist.end() ) {
          // cerr << "SOME CONNECTION TOUCHED" << endl;
          if (PassPayloadToSock(sock, cs, p)) {
            // Payload has been pass to socket. (checksum is also check in PassPayloadToSock)
            switch ((*cs).state.GetState()) {
              case LISTEN :
                ResIPStateLISTEN(mux, sock, p, c, clist);
                break;
              case SYN_SENT :
                ResIPStateSYNSENT(mux, sock, p, c, clist);
                break;
              case SYN_RCVD :
                ResIPStateSYNRCVD(mux, sock, p, c, clist);
                break;
              case ESTABLISHED :
                ResIPStateESTABLISHED(mux, sock, p, c, clist);
                break;
              case CLOSE_WAIT :
                ResIPStateCLOSEWAIT(mux, sock, p, c, clist);
              case LAST_ACK :
                ResIPStateLASTACK(mux, sock, p, c, clist);
                break;
              case FIN_WAIT1 :
                ResIPStateFINWAIT1(mux, sock, p, c, clist);
                break;
              case FIN_WAIT2 :
                ResIPStateFINWAIT2(mux, sock, p, c, clist);
                break;
              case CLOSING :
                ResIPStateCLOSING(mux, sock, p, c, clist);
                break;
              case TIME_WAIT :
                ResIPStateTIMEWAIT(mux, sock, p, c, clist);
                break;
              default :
                cerr << "Sorry, I cannot handle right now. BYE!" << endl;
            }
          }
        } else {
          cerr << "No matched port. Discard." << endl;
        }
      }
      //  Data from the Sockets layer above  //
      if (event.handle==sock) {
      	SockRequestResponse s;
      	MinetReceive(sock,s);
        switch (s.type) {
          case CONNECT :
            ResSockConnect(mux, sock, s, clist);
          case ACCEPT :
            ResSockAccept(mux, sock, s, clist);
            break;
          case WRITE :
            ResSockWrite(mux, sock, s, clist);
            break;
          case FORWARD :
            ResSockForward(mux, sock, s, clist);
            break;
          case CLOSE :
            ResSockClose(mux, sock, s, clist);
            break;
          case STATUS :
            ResSockStatus(mux, sock, s, clist);
            break;
          default :
            cerr << "[CANNOT HANDLE RIGHT NOW]Received Socket Request:" << s << endl;
        }
      }
    }
  }
  return 0;
}

// Response Timer Event
static void ResTimerLISTEN(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {

}

static void ResTimerSYNSENT(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {
  if ((*cs).state.ExpireTimerTries()) {
    // Connection Failed, Close Connection
    SockRequestResponse eRes;
    eRes.type=WRITE;
    eRes.connection=(*ptr).connection;
    eRes.bytes=0;
    eRes.error=ECONN_FAILED;
    MinetSend(sock,eRes);
  } else {
    SendPacket(mux, cs, (SendPacketType) SYN);
    cerr << "Resend SYN" << endl;
  }
}

static void ResTimerSYNRCVD(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {
  if ((*cs).state.ExpireTimerTries()) {
    // Send RST, Connection Failed, Close Connection
    SockRequestResponse eRes;
    eRes.type=WRITE;
    eRes.connection=(*ptr).connection;
    eRes.bytes=0;
    eRes.error=ECONN_FAILED;
    MinetSend(sock,eRes);
    SendPacket(mux, cs, (SendPacketType) RST);
  } else {
    SendPacket(mux, cs, (SendPacketType) SYNACK);
    cerr << "Resend SYNACK" << endl;
  }
}

static void ResTimerESTABLISHED(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {
  if ((*cs).state.SendBuffer.GetSize()) {
    // if there is not ACKed data, then resend and restart Timer
    (*cs).state.SetLastSent((*cs).state.GetLastAcked());
    SendPacket(mux, cs, (SendPacketType) DATA);
    cerr << "Resend DATA" << endl;
  } else {
    // stop timer
    (*cs).bTmrActive = false;
  }
}

static void ResTimerCLOSEWAIT(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {

}

static void ResTimerLASTACK(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {
  SendPacket(mux, cs, (SendPacketType) FIN);
  cerr << "Resend FIN" << endl;
}

static void ResTimerFINWAIT1(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {
  SendPacket(mux, cs, (SendPacketType) FIN);
  cerr << "Resend FIN" << endl;
}

static void ResTimerFINWAIT2(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {

}

static void ResTimerCLOSING(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {
  // resend FIN
  SendPacket(mux, cs, (SendPacketType) FIN);
  cerr << "Resend FIN" << endl;
}

static void ResTimerTIMEWAIT(MinetHandle mux, MinetHandle sock, ConnectionList<TCPState>::iterator &cs) {
  // Erase connection
  cerr << "CLOSED and Erase connection" << endl;
  (*cs).state.SetState(CLOSED);
  clist.erase(cs);
}


// Response IP Event
static void ResIPStateLISTEN(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist){
  // get the received packet flags
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
  unsigned char flags = 0;
  tcph.GetFlags(flags);
  // if is_SYN then send SYNACK
  if (IS_SYN(flags)) {
    cerr << "TCP Header flags: " << (int) flags << endl;
    cerr << "SYN packet detected." <<endl;
    unsigned int initSeqNum;
    unsigned int recvdSeqNum;
    unsigned short rwnd;
    // create a new connection and initialize it
    Time newtimeout(0,0);
    initSeqNum = (rand() % SEQ_LENGTH_MASK);
    cerr << "Generate new initSeqNum:" << initSeqNum << endl;
    TCPState newtcp(initSeqNum, SYN_RCVD, 8);
    tcph.GetSeqNum(recvdSeqNum);
    newtcp.SetLastRecvd(recvdSeqNum);
    tcph.GetWinSize(rwnd);
    newtcp.SetSendRwnd(rwnd);
    ConnectionToStateMapping<TCPState> ctsm(c, newtimeout, newtcp, false);
    clist.push_front(ctsm);
    ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);

    if (cs != clist.end()) {
      //cerr << "New connection added:" << endl << *cs << endl;
      // send Packet to IP
      SendPacket(mux, cs, (SendPacketType) SYNACK);
      (*cs).state.SetState(SYN_RCVD);
      cerr << "SYN RCVD" << endl;
    } 
    cerr << "receivedPacket: " << recvdSeqNum << endl;
  } else {
    // don't do anything right now, just discard
  }
}

static void ResIPStateSYNSENT(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist){
  cerr << "SYN_SENT connection got a packet." << endl;
  // check if the packet is correct SYNACK packet.
  SockRequestResponse sRes;
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  unsigned char flags = 0;
  unsigned int ack;
  unsigned int recvdSeqNum;
  tcph.GetFlags(flags);
  tcph.GetAckNum(ack);
  tcph.GetSeqNum(recvdSeqNum);
  if (IS_ACK(flags) && IS_SYN(flags)) {
    if ((*cs).state.SetLastAcked(ack)) {
      // correctly ACKed, ACK received paket, move to ESTABLISHED
      SendPacket(mux, cs, (SendPacketType) ACK);
      (*cs).state.SetState(ESTABLISHED);
      cerr << "ESTABLISHED" << endl;
      // tell socket it has successfully connected
      sRes.type = WRITE;
      sRes.connection = (*cs).connection;
      sRes.bytes = 0;
      sRes.error = EOK;
      MinetSend(sock, sRes);
    }
  } else if (IS_SYN(flags)) {
    // send SYNACK, move to SYN_RCVD
    SendPacket(mux, cs, (SendPacketType) SYNACK);
    (*cs).state.SetState(SYN_RCVD);
    // TODO: do we need to send response to sock?
  }

}

static void ResIPStateSYNRCVD(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist){
  cerr << "SYN_RCVD connection get a packet.(above)" << endl;
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
  // TODO: should chech if payload is added.
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  unsigned char flags = 0;
  unsigned int ack;
  unsigned int recvdSeqNum;
  tcph.GetFlags(flags);
  tcph.GetAckNum(ack);
  tcph.GetSeqNum(recvdSeqNum);
  if (IS_ACK(flags)) {
    if ((*cs).state.SetLastAcked(ack)) {
      // correctly ACKed, move to ESTABLISHED
      (*cs).state.SetState(ESTABLISHED);
      (*cs).bTmrActive = false;
      cerr << "ESTABLISHED" << endl;
      // send response to Socket to tell socket new connection is built.
      SockRequestResponse sRes;
      sRes.type = WRITE;
      sRes.connection = (*cs).connection;
      sRes.bytes = 0;
      sRes.error = EOK;
      MinetSend(sock, sRes);
    }
  } else if (IS_SYN(flags) && (*cs).state.GetLastRecvd() == recvdSeqNum) {
    // previous SYNACK not received, resend SYNACK
    SendPacket(mux, cs, (SendPacketType) SYNACK);
    cerr << "SYNACK RESENT" << endl;
  }
}

static void ResIPStateESTABLISHED(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist){
  cerr << "ESTABLISHED connection get a packet.(above)" << endl;
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  IPHeader iph=p.FindHeader(Headers::IPHeader);
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
  unsigned char flags = 0;
  unsigned int ack;
  unsigned int recvdSeqNum;
  tcph.GetFlags(flags);
  tcph.GetAckNum(ack);
  tcph.GetSeqNum(recvdSeqNum);

  if (IS_SYN(flags) && IS_ACK(flags) && (*cs).state.GetLastRecvd() == recvdSeqNum) {
    // resend ACK
    SendPacket(mux, cs, (SendPacketType) ACK);
  } else {
    // get the packet data length
    unsigned int datalen;
    Buffer recvdData = FetchPayload(p, datalen);
                
    if (IS_ACK(flags)) {
      if ((*cs).state.SetLastAcked(ack)) {
        if (datalen > 0) {
          // for testing
          //cerr << "Get a packet with data size: " << datalen << endl;
          //(*cs).state.SendBuffer.AddBack(recvdData);
          //SendPacket(mux, cs, (SendPacketType) DATA);
          // for real impliment
          SendPacket(mux, cs, (SendPacketType) ACK);
        }
      }
    }
    if (IS_FIN(flags)) {
      // send ACK
      (*cs).state.SetState(CLOSE_WAIT);
      // SendPacket(mux, cs, (SendPacketType) ACK);
      cerr << "CLOSE_WAIT" << endl;

      // send a zero byte WRITE to sock as CLOSE
      SockRequestResponse sRes;
      sRes.type = WRITE;
      sRes.bytes = 0;
      sRes.error = EOK;
      MinetSend(sock, sRes);

      // TOTEST: should be done by timeout or socket close.
      // int sleeploop = 0;
      // while (sleeploop < 99999){
      //   sleeploop += 1;
      // }
      // SendPacket(mux, cs, (SendPacketType) FIN);
      // (*cs).state.SetState(LAST_ACK);
      // cerr << "LAST_ACK" << endl;
      // cerr << "Sent packet with SeqNum: " << (*cs).state.GetLastSent() << endl;  
    }
  }
}

static void ResIPStateCLOSEWAIT(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist) {

}

static void ResIPStateLASTACK(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist){
  cerr << "LAST_ACK connection get a packet.(above)" << endl;
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
  // receive last ACK, then close the connection
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  unsigned char flags = 0;
  unsigned int ack;
  unsigned int recvdSeqNum;
  tcph.GetFlags(flags);
  tcph.GetAckNum(ack);
  tcph.GetSeqNum(recvdSeqNum);
  if (IS_ACK(flags)) {
    if ((*cs).state.SetLastAcked(ack-1)) {
      (*cs).state.SetState(CLOSED);
      clist.erase(cs);
      cerr << "CLOSED and erase Connection. Last Ack Num: " << ack << endl;
    }
  }
}

static void ResIPStateFINWAIT1(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist) {
  cerr << "FIN_WAIT1 connection get a packet" << endl;
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  unsigned char flags = 0;
  unsigned int ack;
  unsigned int recvdSeqNum;
  tcph.GetFlags(flags);
  tcph.GetAckNum(ack);
  tcph.GetSeqNum(recvdSeqNum);
  if (IS_FIN(flags) && !IS_ACK(flags)) {
    // receive FIN, send ACK, move to CLOSING
    SendPacket(mux, cs, (SendPacketType) ACK);
    (*cs).state.SetState(CLOSING);
    cerr << "CLOSING" << endl;
  } else if (IS_FIN(flags) && IS_ACK(flags)) {
    if ((*cs).state.SetLastAcked(ack) && ((*cs).state.GetLastSent() == (ack-1))) {
      // receive FINACK (all Sent are Acked), send ACK, move to TIME_WAIT
      SendPacket(mux, cs, (SendPacketType) ACK);
      (*cs).state.SetState(TIME_WAIT);
      (*cs).timeout = new Time();
      (*cs).timeout.tv_sec += 2*MSL_TIME_SECS;
      (*cs).bTmrActive = true;
      cerr << "TIME_WAIT" << endl;

      // TODO should be called by timeout
      int sleeploop = 0;
      while (sleeploop < 99999){
        sleeploop += 1;
      }
      (*cs).state.SetState(CLOSED);
      clist.erase(cs);
      cerr << "CLOSED and erase Connection." << endl;

    }
  } else if (IS_ACK(flags)) {
    if ((*cs).state.SetLastAcked(ack) && ((*cs).state.GetLastSent() == (ack-1))) {
      // receive ACK (all Sent are Acked), move to FIN_WAIT2
      (*cs).state.SetState(FIN_WAIT2);
      (*cs).bTmrActive = false;
      cerr << "FIN_WAIT2" << endl;
    }
  }
}

static void ResIPStateFINWAIT2(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist) {
  cerr << "FIN_WAIT2 connection get a packet" << endl;
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  unsigned char flags = 0;
  unsigned int ack;
  unsigned int recvdSeqNum;
  tcph.GetFlags(flags);
  tcph.GetAckNum(ack);
  tcph.GetSeqNum(recvdSeqNum);
  if (IS_FIN(flags)) {
    // receive FIN, send ACK, move to TIME_WAIT
    SendPacket(mux, cs, (SendPacketType) ACK);
    (*cs).state.SetState(TIME_WAIT);
    (*cs).timeout = new Time();
    (*cs).timeout.tv_sec += 2*MSL_TIME_SECS;
    (*cs).bTmrActive = true;
    cerr << "TIME_WAIT" << endl;

    // TODO should be called by timeout
    int sleeploop = 0;
    while (sleeploop < 99999){
      sleeploop += 1;
    }
    (*cs).state.SetState(CLOSED);
    clist.erase(cs);
    cerr << "CLOSED and erase Connection." << endl;

  } else if (IS_ACK(flags)) {
    if ((*cs).state.SetLastAcked(ack)) {
      // receive ACK, do nothing
    }
  }
}

static void ResIPStateCLOSING(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist) {
  cerr << "CLOSING connection get a packet" << endl;
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  unsigned char flags = 0;
  unsigned int ack;
  unsigned int recvdSeqNum;
  tcph.GetFlags(flags);
  tcph.GetAckNum(ack);
  tcph.GetSeqNum(recvdSeqNum);
  if (IS_ACK(flags)) {
    if ((*cs).state.SetLastAcked(ack) && ((*cs).state.GetLastSent() == ack-1)) {
      // receive ACK, (all are ACKed), move to TIME_WAIT
      (*cs).state.SetState(TIME_WAIT);
      (*cs).timeout = new Time();
      (*cs).timeout.tv_sec += 2*MSL_TIME_SECS;
      (*cs).bTmrActive = true;
      cerr << "TIME_WAIT" << endl;

      // TODO should be called by timeout
      int sleeploop = 0;
      while (sleeploop < 99999){
        sleeploop += 1;
      }
      (*cs).state.SetState(CLOSED);
      clist.erase(cs);
      cerr << "CLOSED and erase Connection." << endl;


    }
  }
}

static void ResIPStateTIMEWAIT(MinetHandle mux, MinetHandle sock, Packet p, Connection c, ConnectionList<TCPState> &clist) {

}


// Send Data Functions
static void SendPacket(MinetHandle mnth, ConnectionList<TCPState>:: iterator &cs, SendPacketType ptyp) {
  unsigned char flags=0;
  size_t dataSize=0;
  unsigned int dataOffset=0;
  char sendBuf[TCP_MAXIMUM_SEGMENT_SIZE];
  // set flags
  switch (ptyp) {
    case SYN :
      SET_SYN(flags);
      break;
    case SYNACK :
      SET_SYN(flags);
      SET_ACK(flags);
      break;
    case FIN :
      SET_ACK(flags);
      SET_FIN(flags);
      break;
    case ACK :
      SET_ACK(flags);
      break;
    case DATA :
      SET_ACK(flags);
      SET_PSH(flags);
      break;
    case RST :
      SET_RST(flags);
    default: 
      return;
  }
  // decide the payload 
  // TODO: check if it is GO-BACK-N
  (*cs).state.SendPacketPayload(dataOffset, dataSize, (unsigned int) ((*cs).state.SendBuffer.GetSize()));
  (*cs).state.SendBuffer.GetData(sendBuf, dataSize, dataOffset);
  cerr << "Packet include payload length: " << dataSize << endl;
  // create packet
  Packet p = CreatePacket(cs, flags, sendBuf, (unsigned int) dataSize);

  MinetSend(mnth, p);

  // set Timer
  if (ptyp != ACK && ptyp != RST) {
    (*cs).timeout = new Time();
    (*cs).timeout.tv_sec = EstimateNewTimerInterval(new Time(0));
    (*cs).bTmrActive = true;
  }
}

// create packet
static Packet CreatePacket(ConnectionList<TCPState>::iterator &cs, 
                          unsigned char flags, 
                          char *sendBuf,
                          unsigned int dataSize) {
  // payload include!!!!!!!!!!
  Packet p(sendBuf, (size_t) dataSize);


  // setup ip header
  IPHeader iph;
  iph.SetProtocol(IP_PROTO_TCP);
  iph.SetSourceIP((*cs).connection.src);
  iph.SetDestIP((*cs).connection.dest);

  if (IS_SYN(flags))
    iph.SetTotalLength(TCP_HEADER_BASE_LENGTH + 4 + IP_HEADER_BASE_LENGTH);
  else
    iph.SetTotalLength(TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH + dataSize);

  // push IPHeader info at the front of packet
  p.PushFrontHeader(iph);

  // setup tcp header
  TCPHeader tcph;
  tcph.SetSourcePort((*cs).connection.srcport, p);
  tcph.SetDestPort((*cs).connection.destport, p);
  if (IS_SYN(flags)) {
    tcph.SetSeqNum((*cs).state.GetLastAcked(), p);
  } else {
    tcph.SetSeqNum((*cs).state.GetLastSent()+1, p);
  }
  if (IS_SYN(flags)) {
    // set MSS
    TCPOptions opts;
    opts.len = TCP_HEADER_OPTION_KIND_MSS_LEN;
    opts.data[0] = (char) TCP_HEADER_OPTION_KIND_MSS;
    opts.data[1] = (char) TCP_HEADER_OPTION_KIND_MSS_LEN;
    opts.data[2] = (char) ((TCP_MAXIMUM_SEGMENT_SIZE & 0xFF00) >> 8);
    opts.data[3] = (char) (TCP_MAXIMUM_SEGMENT_SIZE & 0x00FF);
    tcph.SetOptions(opts);
  }
  // set last sent
  (*cs).state.SetLastSent((*cs).state.GetLastSent() + dataSize);

  // set flags
  tcph.SetFlags(flags, p);
  if (IS_ACK(flags))
    tcph.SetAckNum((*cs).state.GetLastRecvd()+1, p);
  // set window size
  tcph.SetWinSize((unsigned short)((*cs).state.TCP_BUFFER_SIZE) - (unsigned short)((*cs).state.RecvBuffer.GetSize()), p);

  if (IS_SYN(flags)) {
    tcph.SetHeaderLen((TCP_HEADER_BASE_LENGTH+4)/4,p);
  } else {
    tcph.SetHeaderLen((TCP_HEADER_BASE_LENGTH/4), p);
  }

  // push TCPHeader info at the back of packet
  p.PushBackHeader(tcph);

  return p;
}

static Buffer FetchPayload(Packet p, unsigned int &datalen) {
  // cumputer payload length
  // Buffer.GetSize() function is unreliable for incoming packet but perfect for created Buffer by ourselives.
  unsigned short totlen;
  unsigned char iphlen;
  unsigned char tcphlen;
  IPHeader iph=p.FindHeader(Headers::IPHeader);
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
  iph.GetTotalLength(totlen);
  iph.GetHeaderLength(iphlen);
  tcph.GetHeaderLen(tcphlen);
  datalen = (unsigned int) totlen - (unsigned int) (iphlen*sizeof(int)) - (unsigned int) (tcphlen*sizeof(int));
  // build Buffer payload
  Buffer recvdData(p.GetPayload());
  char * localbuffer;
  cerr << "Get a packet with data size: " << datalen << endl;
  localbuffer = (char*) malloc((size_t) (datalen+1));
  recvdData.GetData(localbuffer, (size_t) datalen, 0);
  localbuffer[datalen] = 0;
  cerr << "Data: " << localbuffer << endl;
  Buffer payload(localbuffer,(size_t) datalen);

  return payload;
}

static bool PassPayloadToSock(MinetHandle mnth, ConnectionList<TCPState>::iterator &cs, Packet p){
  TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
  if (tcph.IsCorrectChecksum(p)) {
    unsigned char flags = 0;
    unsigned int ack;
    unsigned int recvdSeqNum;
    unsigned short rwnd;
    tcph.GetFlags(flags);
    tcph.GetAckNum(ack);
    tcph.GetSeqNum(recvdSeqNum);
    tcph.GetWinSize(rwnd);
    cerr << "receivedPacket: " << recvdSeqNum << endl;
    // get the packet data length
    unsigned int datalen;
    Buffer recvdData = FetchPayload(p, datalen);
    if ((*cs).state.GetLastRecvd() <= recvdSeqNum+datalen-1) {
      // if haven't received before
      if (datalen > 0) {
        // if it has payload
        cerr << "Get a packet with data size: " << datalen << endl;
        (*cs).state.SetLastRecvd(recvdSeqNum+datalen-1);
        (*cs).state.SetSendRwnd(rwnd);
        (*cs).state.RecvBuffer.AddBack(recvdData);
        // pass packet payload to socket
        SockRequestResponse sRes(WRITE, (*cs).connection, (*cs).state.RecvBuffer, (unsigned) ((*cs).state.RecvBuffer.GetSize()), EOK);
        MinetSend(mnth, sRes);
          
      } else {
        (*cs).state.SetLastRecvd(recvdSeqNum);
        (*cs).state.SetSendRwnd(rwnd);
      }
    } else {
      // if has received, pass to IP Event handlers.
    }
    return true;
  } else {
    cerr << "received a packet with invalid checksum. Discard." << endl;
    return false;
  }
}

// Set Timeout Event Trigger
static double PickupNextTimeout(){
  Time now;
  double nextTimeout = -1;
  ConnectionList<TCPState>::iterator cs = clist.FindEarliest();
  if (gettimeofday(&now, 0) < 0) {
    cerr << "Fail to active timer." << endl;
    exit(-1);
  }
  if (cs != clist.end()) {
    // if there is an active timer
    if ((*cs).timeout < now) {
      nextTimeout = 0;
    } else {
      nextTimeout = (double) (*cs).timeout - (double) now;
    }
  }
  return nextTimeout;
}

static unsigned int EstimateNewTimerInterval(Time newRTT) {
    static unsigned double  estimated = 0;
    static unsigned double devRTT = 0;
    static unsigned double timeoutInterval = 5;
    if((double)newRTT == 0){
        return timeoutInterval;
    }
    if(estimated == 0){
        estimated = newRTT.tv_sec;
        devRTT = 0;
    }else{
        estimated = 0.875 * estimated + 0.125 * newRTT.tv_sec;
        devRTT = 0.75 * devRTT + 0.25 * (newRTT.tv_sec > estimated ? (newRTT.tv_sec - estimated) : (estimated - newRTT.tv_sec));
    }
    timeoutInterval = estimated + 4 * devRTT;
    
    return timeoutInterval < 1 ? 1 : (unsigned int)timeoutInterval;
}

static Time EstimateNewRTT(ConnectionList<TCPState>::iterator &cs, unsigned int rcvd){
    static unsigned int oldseq = 0;
    static unsigned int sendTime = 0;
    static unsigned int rcvdTime = 0;
    if(oldseq == 0){
        (*cs).state.GetLastSent();
    }
    if(oldseq == rcvd){
        Time now;
        rcvdTime = now.tv_sec;
        return new Time(rcvdTime - sendTime);
    }else if(oldseq < rcvd){
        Time now;
        sendTime = now.tv_sec;
        oldseq = rcvd;
        return now;
    }
}

// Response Socket Event
static void ResSockConnect(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist){
  // send a SYN to build a connection
  SockRequestResponse sRes;
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(sReq.connection);
  if (cs == clist.end()) {
    // no one reserved this port
    Time newtimeout(0,0);
    TCPState newtcp((unsigned int)(rand()% SEQ_LENGTH_MASK), SYN_SENT, 3);
    Connection c = sReq.connection;
    ConnectionToStateMapping<TCPState> newCTSM(c, newtimeout, newtcp, false);
    clist.push_front(newCTSM);
    // send SYN
    ConnectionList<TCPState>::iterator cs = clist.FindMatching(sReq.connection);
    if (cs != clist.end()) {
      int sleeploop = 0;
      SendPacket(mux, cs, (SendPacketType) SYN);
      cerr << "SENT SYN 1" << endl;
      while (sleeploop < 99999){
        sleeploop += 1;
      }
      sleeploop = 0;
      SendPacket(mux, cs, (SendPacketType) SYN);
      cerr << "SENT SYN 2" << endl;
      while (sleeploop < 99999){
        sleeploop += 1;
      }
      sleeploop = 0;
      SendPacket(mux, cs, (SendPacketType) SYN);
      cerr << "SENT SYN 3" << endl;
      while (sleeploop < 99999){
        sleeploop += 1;
      }
      sleeploop = 0;
      SendPacket(mux, cs, (SendPacketType) SYN);
      cerr << "SENT SYN 4" << endl;
      while (sleeploop < 99999){
        sleeploop += 1;
      }
      sleeploop = 0;
      SendPacket(mux, cs, (SendPacketType) SYN);
      cerr << "SENT SYN 5" << endl;
      while (sleeploop < 99999){
        sleeploop += 1;
      }
      sleeploop = 0;
      SendPacket(mux, cs, (SendPacketType) SYN);
      cerr << "SENT SYN 6" << endl;
      while (sleeploop < 99999){
        sleeploop += 1;
      }
      (*cs).state.SetState(SYN_SENT);
      cerr << "Add New Connection: " << (*cs).connection << endl;
    }
    // build response
    sRes.type = STATUS;
    sRes.connection = sReq.connection;
    sRes.bytes = 0;
    sRes.error = EOK;
  } else {
    // someone has reserved this port
    sRes.type = CLOSE;
    sRes.connection = sReq.connection;
    sRes.bytes = 0;
    sRes.error = ERESOURCE_UNAVAIL;
  }

  MinetSend(sock, sRes);
}

static void ResSockAccept(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist){
  // add a connection to connection list
  // initialize it
  // set it's status as LISTEN
  // send back request result to socket
  SockRequestResponse sRes;
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(sReq.connection);
  if (cs == clist.end()) {
    // no one reserved this port
    Time newtimeout(0,0);
    TCPState newtcp(0, LISTEN, 0);
    Connection c = sReq.connection;
    ConnectionToStateMapping<TCPState> newCTSM(c, newtimeout, newtcp, false);
    clist.push_front(newCTSM);
    // build response
    sRes.type = STATUS;
    sRes.connection = sReq.connection;
    sRes.bytes = 0;
    sRes.error = EOK;
  } else {
    // someone has reserved this port for listening
    sRes.type = CLOSE;
    sRes.connection = sReq.connection;
    sRes.bytes = 0;
    sRes.error = ERESOURCE_UNAVAIL;
  }

  MinetSend(sock, sRes);
}

static void ResSockStatus(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist) {
  // Get the response of passed received data
  SockRequestResponse sRes;
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(sReq.connection);
  if (cs == clist.end()) {
    // no one connected in this connection
    sRes.type = STATUS;
    sRes.error = ENOMATCH;
    MinetSend(sock, sRes);
  } else if ((*cs).state.RecvBuffer.GetSize() > 0){
    // matched connection, erace successfully read data
    (*cs).state.RecvBuffer.Erase(0, sReq.bytes);
    cerr << "RecvBuffer Data Length after ERASED: " << (*cs).state.RecvBuffer.GetSize() << endl;
    // if there is some data not be read.
    if ((*cs).state.RecvBuffer.GetSize() > 0){
      sRes.type = WRITE;
      sRes.connection = (*cs).connection;
      sRes.data = (*cs).state.RecvBuffer;
      sRes.bytes = (unsigned) ((*cs).state.RecvBuffer.GetSize());
      sRes.error = EOK;
      MinetSend(sock, sRes);
    }
  }
}

static void ResSockWrite(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist){
  cerr << "GET A WRITE REQUEST" << endl;
  cerr << "request Buffer data size:" << sReq.data.GetSize() << endl;
  SockRequestResponse sRes;
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(sReq.connection);
  if (cs == clist.end()) {
    // no connection matched 
    sRes.type = STATUS;
    sRes.error = ENOMATCH;
    MinetSend(sock, sRes);
  } else {
    // matched connection, add data to end of sendBuffer and send
    unsigned int datalen = (unsigned int)sReq.data.GetSize();
    datalen = MIN_MACRO(datalen, (*cs).state.TCP_BUFFER_SIZE - (unsigned int)((*cs).state.SendBuffer.GetSize()));
    cerr << "add Data to SendBuffer, datalen:" << datalen << endl;
    // sReq.data.ExtractFront((size_t) datalen);
    (*cs).state.SendBuffer.AddBack(sReq.data.ExtractFront((size_t) datalen));
    // TODO: for testing
    SendPacket(mux, cs, (SendPacketType) FIN);
    (*cs).state.SetState(FIN_WAIT1);
    // TOFO: for real impliment 
    //SendPacket(mux, cs, (SendPacketType) DATA);
    sRes.type = STATUS;
    sRes.bytes = datalen;
    sRes.error = EOK;
    MinetSend(sock, sRes);
  }
}

static void ResSockForward(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist){
  // TCP module ignores this message
  // A zero error STATUS will be returned
  SockRequestResponse sRes;
  sRes.type = STATUS;
  sRes.error = EOK;
  MinetSend(sock, sRes);
}

static void ResSockClose(MinetHandle mux, MinetHandle sock, SockRequestResponse sReq, ConnectionList<TCPState> &clist) {
  // close a connection
  SockRequestResponse sRes;
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(sReq.connection);
  if (cs == clist.end()) {
    // no connection matched
    sRes.type = STATUS;
    sRes.error = ENOMATCH;
    MinetSend(sock, sRes);
  } else {
    // matched connection, send FIN
    SendPacket(mux, cs, (SendPacketType) FIN);
    // response to socket module
    sRes.type = STATUS;
    sRes.bytes = 0;
    sRes.error = EOK;
    MinetSend(sock, sRes);
    // change TCP State
    if ((*cs).state.GetState() == CLOSE_WAIT) {
      (*cs).state.SetState(LAST_ACK);
    } else {
      (*cs).state.SetState(FIN_WAIT1);
    }
  }
}