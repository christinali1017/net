#include "messages.h"

//============================================================================
//===============================Generic======================================

#if defined(GENERIC)
ostream &RoutingMessage::Print(ostream &os) const
{
  os << "RoutingMessage()";
  return os;
}
#endif


//============================================================================
//===============================Link State ==================================



#if defined(LINKSTATE)

ostream &RoutingMessage::Print(ostream &os) const
{
  map<unsigned, map<unsigned, double> > *mcosttable = costtable;
  map<unsigned, map<unsigned, double> >::iterator it;
  os << "FROM: " << from << endl;
  os << "Message includes following cost table:" << endl;
  for (it = mcosttable->begin(); it != mcosttable->end(); ++it) {
    os << it->first << " : ";
    map<unsigned, double> it_outgoing = it->second;
    map<unsigned, double>::iterator it_outgoing_it = it_outgoing.begin();
    for (;it_outgoing_it != it_outgoing.end(); ++it_outgoing_it) {
      os << it_outgoing_it->first << "(" << it_outgoing_it->second << ") ";
    }
    os << endl;
  }
  return os;
}

RoutingMessage::RoutingMessage(const unsigned mfrom, map<unsigned, map<unsigned, double> > *mcosttable){
	from = mfrom;
	costtable = mcosttable;
}

RoutingMessage::RoutingMessage()
{}


RoutingMessage::RoutingMessage(const RoutingMessage &rhs)
{}

#endif


//============================================================================
//=============================Distance Vector================================



#if defined(DISTANCEVECTOR)

ostream &RoutingMessage::Print(ostream &os) const
{
  vector<double> *mrow = updateLine;
  vector<double>::iterator it;
  os << "FROM: " << src << endl;
  for (it = mrow->begin(); it != mrow->end(); ++it) {
    os << (*it) << " ";
  }
  os << endl;
  return os;
}

RoutingMessage::RoutingMessage()
{}

RoutingMessage::RoutingMessage(unsigned src1, vector<double>* row)
{
	src = src1;
	updateLine = row;
}


RoutingMessage::RoutingMessage(const RoutingMessage &rhs)
{}

#endif

