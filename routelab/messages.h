#ifndef _messages
#define _messages

#include <iostream>

#include "node.h"
#include "link.h"


//============================================================================
//===============================Generic======================================


#if defined(GENERIC)
struct RoutingMessage {
 public:
  ostream & Print(ostream &os) const;
};
#endif

//============================================================================
//===============================Link State ==================================

#if defined(LINKSTATE)
struct RoutingMessage {
  unsigned from;
  map<unsigned, map<unsigned, double> > *costtable;

  RoutingMessage(const unsigned mfrom, map<unsigned, map<unsigned, double> > *mcosttable);
  RoutingMessage();
  RoutingMessage(const RoutingMessage &rhs);
  RoutingMessage &operator=(const RoutingMessage &rhs);

  ostream & Print(ostream &os) const;
};
#endif

//============================================================================
//=============================Distance Vector================================


#if defined(DISTANCEVECTOR)
struct RoutingMessage {

 	vector<double>* updateLine;
 	unsigned src;

	RoutingMessage();
	RoutingMessage(unsigned src, vector<double>* row);
	RoutingMessage(const RoutingMessage &rhs);
	RoutingMessage &operator=(const RoutingMessage &rhs);

	ostream & Print(ostream &os) const;
};
#endif


inline ostream & operator<<(ostream &os, const RoutingMessage &m) { return m.Print(os);}

#endif
