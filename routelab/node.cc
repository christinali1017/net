#include "node.h"
#include "context.h"
#include "error.h"
#include <limits>


Node::Node(const unsigned n, SimulationContext *c, double b, double l) : 
    number(n), context(c), bw(b), lat(l) 
{
  #if defined(DISTANCEVECTOR)
  if(b==0 && l==0){}
  else table = new Table(this);
  #endif
}

Node::Node() 
{ throw GeneralException(); }

#if defined(DISTANCEVECTOR)
Node::Node(const Node &rhs) : 
  number(rhs.number), context(rhs.context), bw(rhs.bw), lat(rhs.lat), table(rhs.table)
  {
  }
#endif

#if defined(GENERIC)
Node::Node(const Node &rhs) : 
  number(rhs.number), context(rhs.context), bw(rhs.bw), lat(rhs.lat)
  {
  }
#endif

#if defined(LINKSTATE)
Node::Node(const Node &rhs) : 
  number(rhs.number), context(rhs.context), bw(rhs.bw), lat(rhs.lat)
  {
  }
#endif

Node & Node::operator=(const Node &rhs) 
{
  return *(new(this)Node(rhs));
}

void Node::SetNumber(const unsigned n) 
{ number=n;}

unsigned Node::GetNumber() const 
{ return number;}

void Node::SetLatency(const double l)
{ lat=l;}

double Node::GetLatency() const 
{ return lat;}

void Node::SetBW(const double b)
{ bw=b;}

double Node::GetBW() const 
{ return bw;}

Node::~Node()
{}

// Implement these functions  to post an event to the event queue in the event simulator
// so that the corresponding node can recieve the ROUTING_MESSAGE_ARRIVAL event at the proper time
void Node::SendToNeighbors(const RoutingMessage *m)
{
  context->SendToNeighbors(this, m);
}

void Node::SendToNeighbor(const Node *n, const RoutingMessage *m)
{

}

deque<Node*> *Node::GetNeighbors() const
{
  return context->GetNeighbors(this);
}

deque<Link*> *Node::GetOutgoingLinks() const
{
  return context->GetOutgoingLinks(this);
}

void Node::SetTimeOut(const double timefromnow)
{
  context->TimeOut(this,timefromnow);
}


bool Node::Matches(const Node &rhs) const
{
  return number==rhs.number;
}

//============================================================================
//=============================Generic========================================


#if defined(GENERIC)
void Node::LinkHasBeenUpdated(const Link *l)
{
  cerr << *this << " got a link update: "<<*l<<endl;
  //Do Something generic:
  SendToNeighbors(new RoutingMessage);
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  cerr << *this << " got a routing messagee: "<<*m<<" Ignored "<<endl;
}


void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored"<<endl;
}

Node *Node::GetNextHop(const Node *destination) const
{
  return 0;
}

Table *Node::GetRoutingTable() const
{
  return new Table;
}


ostream & Node::Print(ostream &os) const
{
  os << "Node(number="<<number<<", lat="<<lat<<", bw="<<bw<<")";
  return os;
}

#endif

//============================================================================
//==================================Link State================================


#if defined(LINKSTATE)

void Node::Dijkstra()
{
  // re-calculate the lstable;
  map<unsigned, pair<unsigned, double> > tmplstable;
  map<unsigned, map<unsigned, double> >::iterator it;
  map<unsigned, pair<unsigned, double> >::iterator tmpls_it;
  map<unsigned, double> outgoings;
  unsigned current = number;
  double min = std::numeric_limits<double>::infinity();

  // cerr << "Dijkstra Start." << endl;
  // initialize
  lstable.erase(lstable.begin(), lstable.end());
  for (it = costtable.begin(); it != costtable.end(); ++it) {
    tmplstable[it->first] = make_pair(0, std::numeric_limits<double>::infinity());
    map<unsigned, double> it_outgoing = it->second;
    map<unsigned, double>::iterator it_outgoing_it = it_outgoing.begin();
    for (;it_outgoing_it != it_outgoing.end(); ++it_outgoing_it) {
      tmplstable[it_outgoing_it->first] = make_pair(0, std::numeric_limits<double>::infinity());
    }
    if (current==it->first)
    {
      min = 0;
      outgoings = it_outgoing;
    }
  }
  tmplstable[current] = make_pair(current, 0);
  // save to lstable
  tmplstable.erase(current);
  // update rest nodes
  for (map<unsigned, double>::iterator out_it = outgoings.begin(); out_it != outgoings.end(); ++out_it)
  {
    map<unsigned, pair<unsigned, double> >::iterator ls_it = tmplstable.find(out_it->first);
    if ((ls_it != tmplstable.end()) && ((ls_it->second).second > min + out_it->second)) {
      // update like state
      ls_it->second = make_pair(current, min + out_it->second);
    }
  }
  // cerr << "Dijkstra initialized." << endl;

  // loop until all added
  while (!tmplstable.empty())
  {
    min = std::numeric_limits<double>::infinity();
    // find the minimum
    for (tmpls_it = tmplstable.begin(); tmpls_it != tmplstable.end(); ++tmpls_it) {
      if ((tmpls_it->second).second < min) {
        min = (tmpls_it->second).second;
        current = tmpls_it->first;
      }
    }
    // save to lstable
    tmpls_it = tmplstable.find(current);
    if (tmpls_it == tmplstable.end())
      break;
    it = costtable.find(current);
    if (it == costtable.end())
    {
      lstable.insert(make_pair(tmpls_it->first,tmpls_it->second));
      tmplstable.erase(current);
      continue;
    }
    outgoings = it->second;
    lstable.insert(make_pair(tmpls_it->first,tmpls_it->second));
    tmplstable.erase(current);
    // update the rest nodes
    for (map<unsigned, double>::iterator out_it = outgoings.begin(); out_it != outgoings.end(); ++out_it)
    {
      map<unsigned, pair<unsigned, double> >::iterator ls_it = tmplstable.find(out_it->first);
      if ((ls_it != lstable.end()) && ((ls_it->second).second > min + out_it->second)) {
        // update like state
        ls_it->second = make_pair(current, min + out_it->second);
      }
    }
  }

}


bool Node::UpdateLocalCostTable(unsigned src, unsigned dest, unsigned cost) 
{
  bool updated = false;
  map<unsigned, map<unsigned, double> >::iterator it = costtable.find(src);
  if (it == costtable.end()) {
    // new node
    map<unsigned, double> nl;
    nl.insert(make_pair(dest, cost));
    costtable.insert(make_pair(src, nl));
    updated = true;
  } else {
    map<unsigned, double> it_outgoing = it->second;
    map<unsigned, double>::iterator it_outgoing_it = it_outgoing.find(dest);
    if (it_outgoing_it == it_outgoing.end()) {
      // new link
      it_outgoing.insert(make_pair(dest, cost));
      it->second = it_outgoing;
      updated = true;
    } else {
      if (it_outgoing_it->second != cost) {
        it_outgoing.erase(it_outgoing_it);
        it_outgoing.insert(make_pair(dest, cost));
        it->second = it_outgoing;
        updated = true;
      }
    }
  }

  return updated;
}

void Node::SendMessageThroughShortestPath(unsigned from){
  const RoutingMessage *m = new RoutingMessage(number, &costtable);
  const Node *n = this;
  // cerr << "New Message Need to Be Propergated" << endl;
  map<unsigned, bool> sent;
  sent.insert(make_pair(from, true));
  for (map<unsigned, pair<unsigned, double> >::iterator ls_it = lstable.begin(); ls_it != lstable.end(); ++ls_it) {
    unsigned mdest = ls_it->first;
    const Node *next = this->GetNextHop(&Node(mdest, 0, 0, 0));
    map<unsigned, bool>::iterator it = sent.find(next->GetNumber());
    if (it == sent.end()) {
      context->SendToNeighbor(n, next, m);
      sent.insert(make_pair(next->GetNumber(), true));
    }
  }
  // cerr << "Sent Message Through Shortest Path" << endl;
}

void Node::LinkHasBeenUpdated(const Link *l)
{
  // cerr << *this<<": Link Update: "<<*l<<endl;
  cerr << ": Link Update: "<<*l<<endl;
  // update costtable, if the information changed propergate the information
  unsigned src = l->GetSrc();
  unsigned dest = l->GetDest();
  unsigned cost = l->GetLatency();
  bool updated = this->UpdateLocalCostTable(src, dest, cost);
  // re-calculate the lstable
  this->Dijkstra();
  if (updated) {
    this->SendMessageThroughShortestPath(number);
  }
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  // cerr << *this << "Routing Message: "<<*m << endl;
  cerr << "Routing Message: "<<*m << endl;
  // update costtable, if the information changed propergate the information
  unsigned from = m->from;
  bool updated = false;
  map<unsigned, map<unsigned, double> > *incomingCosttable = m->costtable;
  map<unsigned, map<unsigned, double> >::iterator it_cost;
  for (it_cost = incomingCosttable->begin(); it_cost != incomingCosttable->end(); ++it_cost)
  {
    map<unsigned, double> outgoings = it_cost->second;
    map<unsigned, double>::iterator outgoings_it;
    for (outgoings_it = outgoings.begin(); outgoings_it!= outgoings.end(); ++outgoings_it)
    {
      unsigned src = it_cost->first;
      unsigned dest = outgoings_it->first;
      double cost = outgoings_it->second;
      updated = this->UpdateLocalCostTable(src, dest, cost) || updated;
    }
  }
  
  // re-calculate the lstable
  this->Dijkstra();
  if (updated) {
    this->SendMessageThroughShortestPath(from);
  }
}

void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored"<<endl;
}

Node *Node::GetNextHop(const Node *destination) const
{
  // return the next node based on the lstable to destination
  if (lstable.find(destination->GetNumber()) != lstable.end())
  {
    unsigned n, next;
    next = destination->GetNumber();
    n = (lstable.find(next)->second).first;
    while (n!=number) {
      next = n;
      n = (lstable.find(next)->second).first;
    }

    return new Node(next, 0, 0, 0);
  }
  
  return 0;
}

Table *Node::GetRoutingTable() const
{
  map<unsigned, pair<unsigned, double> > rttable;
  map<unsigned, pair<unsigned, double> > nowlstable = lstable;
  for (map<unsigned, pair<unsigned, double> >::iterator it = nowlstable.begin(); it != nowlstable.end(); ++it)
  {
    unsigned dest = it->first;
    Node *next = this->GetNextHop(&Node(dest, 0, 0, 0));
    unsigned nexthop = next->GetNumber();
    double cost = (it->second).second;
    rttable.insert(make_pair(dest, make_pair(nexthop, cost)));
  }
  Table * tab = new Table(rttable);
  return tab;
}


ostream & Node::Print(ostream &os) const
{
  os << "Node(number="<<number<<", lat="<<lat<<", bw="<<bw<<")" << endl;
  os << "Routing Table" << endl;
  os << "next hop -> destination = total latency" << endl;
  os << (*(this->GetRoutingTable())) << endl;
  return os;
}
#endif

//============================================================================
//=============================Distance Vector================================

#if defined(DISTANCEVECTOR)

void Node::LinkHasBeenUpdated(const Link *l)
{
  // update our table 
  unsigned src = l->GetSrc();
  unsigned dest = l->GetDest();
  double lat = l->GetLatency(); 
  bool isupdated = false;
  unsigned cursize = table->GetTableSize();


  if(dest >= cursize)
  { 
    //this is a new link
    table->SetEntry(src, dest, lat);
    isupdated = true;
  }
  else
  { 
    //change the old link

    double current = table->GetEntry(src, dest);
    table->SetEntry(src, dest, lat);
    deque<Link*>* outLinks = this->GetOutgoingLinks();

    // one link change: the other spots in my line may also change
    for( dest = 0; dest < cursize; dest++){
      for(deque<Link*>::iterator it = outLinks->begin(); it != outLinks->end(); ++it)
      {
        unsigned internode = (*it)->GetDest();
        if(internode+1 > cursize)
          continue;
        double newlat = (*it)->GetLatency() + table->GetEntry(internode, dest);
        if( table->GetEntry(src, dest) > newlat)
        {
          table->SetEntry(src, dest, newlat);
        }
      }
      if (current!= table->GetEntry(src, dest)) {
        isupdated = true;
      }
    }


  }
  

  //if latency is updated
  if(isupdated == true){
    vector<double>* newline = table->GetRow(number);
    RoutingMessage* newchange = new RoutingMessage(number,newline);
    SendToNeighbors(newchange);
  }
   //cerr <<"Node LinkHasBeenUpdated End"<<endl;
}



void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  //update line from changing table
  unsigned index = m->src;
  vector<double>* row = m->updateLine;
  unsigned rowsize = row->size();
  unsigned cursize = table->GetTableSize();

  //cerr<<"src dest "<<index<<" "<<number<<endl;
  for(unsigned i = 0; i < rowsize; i++){
    // cerr<<"set entry  "<<index<<" " <<i<<endl;
    table->SetEntry(index, i, (*row)[i]);
   // cerr<<"end set entry  "<<index<<" " <<i<<endl;
  }
  
  //check whether the line of myself should be changed
  bool isupdated = false;
  deque<Link*>* outLinks = this->GetOutgoingLinks();
  for(unsigned dest = 0; dest < table->GetTableSize(); dest++){
    if(dest == number)
      continue;
    double current = table->GetEntry(number, dest);
    table->SetEntry(number, dest, std::numeric_limits<double>::infinity());
    for(deque<Link*>::iterator it = outLinks->begin(); it != outLinks->end(); ++it)
    {
      unsigned internode = (*it)->GetDest();
      if(internode+1 > cursize)
        continue;

      double newlat = (*it)->GetLatency() + table->GetEntry(internode, dest);
      if( table->GetEntry(number, dest) > newlat)
      {
        
        table->SetEntry(number, dest, newlat);
      }
    }
    if(current != table->GetEntry(number, dest))
      isupdated = true;
  }


  if(isupdated == true){
    vector<double>* newline = table->GetRow(number);
    RoutingMessage* newchange = new RoutingMessage(number,newline);
    SendToNeighbors(newchange);
  }

  //cerr<<"end ProcessIncomingRoutingMessage  "<<endl;

}

void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored"<<endl;
}


Node *Node::GetNextHop(const Node *destination) const
{
  
  #if defined(DISTANCEVECTOR)

  ////print my neibs
  //deque<Node*>* neibs = this->GetNeighbors();
  //for(deque<Node*>::iterator it = neibs->begin(); it != neibs->end(); ++it){
  //  cerr<<"node "<<(*it)->GetNumber();
  //  (*it)->table->PrintTable();
  //}
  ////print myself
  //cerr<<"node number "<<number<<endl;
 // table->PrintTable();


  unsigned dest = destination->GetNumber();
  double shortest = table->GetEntry(number,dest);
  if(shortest == std::numeric_limits<double>::infinity()) return new Node(number, 0, 0, 0);

  Node* next;
  deque<Link*>* outLinks = GetOutgoingLinks();
  bool neibexist = false;
  for(deque<Link*>::iterator it = outLinks->begin(); it != outLinks->end(); ++it)
  {
    // cerr<<"GetNextHop"<<endl;
    unsigned neib = (*it)->GetDest();
    if(neib+1 > table->GetTableSize())
        continue;
    double srcToNeib = (*it)->GetLatency();
    double neibToDest = table->GetEntry(neib, dest);

    if(srcToNeib + neibToDest == shortest)
    {
      next = GetNeighbor(neib);
      break; 
    }
  }
  return next;

  #endif
  
}

Table *Node::GetRoutingTable() const
{
	
  Node* node = new Node(number, 0, 0, 0);
  Table* routetable = new Table(node);
  for(unsigned i = 0; i < table->GetTableSize(); i++)
  {
    if (i == number) { 
      routetable->SetEntry(0, i, i);
      routetable->SetEntry(1, i, 0);
      continue;
    }
    Node * tmpNode = GetNextHop(&Node(i, 0, 0, 0));
    routetable->SetEntry(0, i, (double)tmpNode->GetNumber());
    routetable->SetEntry(1, i, (double)table->GetEntry(number, i));
  }
  return routetable;

}


Node *Node::GetNeighbor(unsigned neibnum) const
{

  Node* neibWithNum;
  deque<Node*>* neibs = GetNeighbors();
  for(deque<Node*>::iterator it = neibs->begin(); it != neibs->end(); ++it)
  {
    if((*it)->GetNumber() == neibnum)
      neibWithNum = new Node(**it);
  }

  return neibWithNum;

}


ostream & Node::Print(ostream &os) const
{
  os << "Node(number="<<number<<", lat="<<lat<<", bw="<<bw<<")" << endl;
  //os<<"Cost table"<<endl;
  //os << *table;
  os << "Routing Table" << endl;
  os << "next hop -> destination = total latency" << endl;
  
  Table* routetable = GetRoutingTable();
  for(unsigned i = 0; i < routetable->GetTableSize(); i++)
  {
    if (i == number) {
      continue;
    }
    os<<routetable->GetEntry(0, i)<<" -> "<<i<<" = "<<routetable->GetEntry(1, i)<<endl;
  }
  return os;
}
#endif
