#ifndef _table
#define _table


#include <iostream>
#include <vector>
#include <deque>
#include "link.h"
#include "node.h"
#include <map>
using namespace std;


//===============================================================================
//=====================================Generic===================================


#if defined(GENERIC)
class Table {
  // Students should write this class

 public:
  ostream & Print(ostream &os) const;
};
#endif


//==============================================================================
//==================================Link State==================================

#if defined(LINKSTATE)
class Table {
 private:
  map<unsigned, pair<unsigned, double> > lstable;
 
  // Students should write this class
 public:
  Table(map<unsigned, pair<unsigned, double> > table);
  ostream & Print(ostream &os) const;
};
#endif

//===============================================================================
//===================================Distance Vector=============================

#if defined(DISTANCEVECTOR)


class Node;
class Link;

class Table {
	private:
		vector< vector<double> >* table;

	public:
		Table();
		Table(Node* node);
    Table(const Table &rhs);
    Table & operator=(const Table &rhs);
  	virtual void SetEntry(unsigned src, unsigned dest, double lat); 
    virtual double GetEntry(unsigned src, unsigned dest);
    virtual vector<double>* GetRow(unsigned src);
    virtual void SetRow(unsigned rownum, vector<double> row);
    virtual unsigned GetTableSize();
    virtual unsigned GetTableColSize();
    virtual void PrintTable();
    virtual void PrintTable(Node* node) const;
  	ostream & Print(ostream &os) const;

};
#endif

inline ostream & operator<<(ostream &os, const Table &t) { return t.Print(os);}

#endif
