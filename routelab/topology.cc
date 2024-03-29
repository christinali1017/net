#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include "topology.h"

/* event of routesim */
Topology::Topology()
{}


Topology::~Topology()
{
  for (deque<Node*>::iterator i=nodes.begin();
	   i!=nodes.end(); ++i) { 
    delete *i;
  }
  nodes.clear();
  for (deque<Link*>::iterator i=links.begin();
	   i!=links.end(); ++i) { 
    delete *i;
  }
  links.clear();
}

deque<Node*>::iterator Topology::FindMatchingNodeIt(const Node *n)
{
  for (deque<Node*>::iterator i=nodes.begin();
	   i!=nodes.end(); ++i) { 
    if ((**i).Matches(*n)) {
      return i;
    }
  }
  return nodes.end();
}

Node * Topology::FindMatchingNode(const Node *n)
{
  deque<Node*>::iterator i=FindMatchingNodeIt(n);
  if (i!=nodes.end()) { 
    return *i;
  } else { 
    return 0;
  }
}


deque<Link*> * Topology::GetOutgoingLinks(const Node *src)
{
  deque<Link*> *out = new deque<Link*>;

  for (deque<Link*>::iterator i=links.begin();
	   i!=links.end(); ++i) { 
    if ((*i)->GetSrc()==src->GetNumber()) { 
      out->push_back(*i);
    }
  }
  return out;
}

deque<Node*> *Topology::GetNeighbors(const Node *n)
{
  deque<Link*> *temp = GetOutgoingLinks(n);
  deque<Node*> *nodes = new deque<Node*>;

  for (deque<Link*>::iterator i=temp->begin();
	   i!=temp->end(); ++i) { 
    nodes->push_back(FindMatchingNode(&Node((*i)->GetDest(),0,0,0)));
  }
  return nodes;
}


deque<Link*>::iterator Topology::FindMatchingLinkIt(const Link *l)
{
  for (deque<Link*>::iterator i=links.begin();
	   i!=links.end(); ++i) { 
    if ((**i).Matches(*l)) {
      return i;
    }
  }
  return links.end();
}

Link *Topology::FindMatchingLink(const Link *l)
{
  deque<Link*>::iterator i=FindMatchingLinkIt(l);
  if (i!=links.end()) { 
    return *i;
  } else { 
    return 0;
  }
}

void Topology::AddNode(Node *n)
{
  if (FindMatchingNode(n)) {
    ChangeNode(n);
  }  else {
    nodes.push_back(n);
  }
}
  
void Topology::DeleteNode(const Node *n)
{
  deque<Node*>::iterator i=FindMatchingNodeIt(n);
  if (i!=nodes.end()) { 
    delete *i;
    nodes.erase(i);
  }
}

void Topology::ChangeNode(const Node *n)
{
  deque<Node*>::iterator i=FindMatchingNodeIt(n);
  if (i!=nodes.end()) { 
    **i=*n;
  }
}

void Topology::AddLink(Link *l) 
{
  if (FindMatchingLink(l)) {
    ChangeLink(l);
  }  else {
    links.push_back(l);
  }
}

void Topology::DeleteLink(const Link *l)
{
  deque<Link*>::iterator i=FindMatchingLinkIt(l);
  if (i!=links.end()) { 
    delete *i;
    links.erase(i);
  }
}


void Topology::ChangeLink(const Link *l)
{
  deque<Link*>::iterator i=FindMatchingLinkIt(l);
  if (i!=links.end()) { 
    **i=*l;
  }
  Node *n=FindMatchingNode(&Node(l->GetSrc(),0,0,0));
  n->LinkHasBeenUpdated(l);
}

void Topology::WriteDot(const string &n) const 
{
  FILE *out = fopen(n.c_str(),"w");
  if (out==0) { 
    return;
  } 
  fprintf(out,"digraph topo {\n");
  for (deque<Node*>::const_iterator i=nodes.begin(); i!=nodes.end();++i) {
    fprintf(out,"%u\n",(*i)->GetNumber());
  }
  for (deque<Link*>::const_iterator i=links.begin(); i!=links.end();++i) {
    fprintf(out,"%u -> %u [ label=\"%5.1lf\" ];\n",(*i)->GetSrc(),(*i)->GetDest(), (*i)->GetLatency());
  }
  fprintf(out,"}\n");
  fclose(out);
}



void Topology::DrawTopology() const
{
  WriteDot(string("_topo.in"));
  system("dot _topo.in > _topo.out");
  system("dotty _topo.out");
}


ostream &Topology::Print(ostream &os) const
{
  os << "Topology(nodes={";
  for (deque<Node*>::const_iterator i=nodes.begin();
       i!=nodes.end();
       ++i) { 
    os << **i <<",";
  }
  os <<"}, links={";
  for (deque<Link*>::const_iterator i=links.begin();
       i!=links.end();
       ++i) { 
    os << **i <<",";
  }
  os <<"})";
  return os;
}



//
// This is totally disgusting
//
void Topology::CollectShortestPathTreeLinks(const Node &src, deque<Link> &links) 
{
  vector<double> distance(nodes.size());
  vector<unsigned>    pred(nodes.size());
  deque<unsigned> visited;
  deque<unsigned> unvisited;
 

  for (deque<Node*>::const_iterator i=nodes.begin();i!=nodes.end();++i) {
    unvisited.push_back((**i).GetNumber());
    pred[(**i).GetNumber()]=nodes.size()+1;
    if ((**i).GetNumber()!=src.GetNumber()) {
      distance[(**i).GetNumber()]=99e99;
    } else {
      distance[(**i).GetNumber()]=0;
    }
  }
  
  while (unvisited.size()>0) {
    double curmin=100e99;
    deque<unsigned>::iterator c;
    unsigned closest;
    for (deque<unsigned>::iterator i=unvisited.begin(); i!=unvisited.end();++i) { 
      if (distance[*i]<curmin) { 
	curmin=distance[*i];
	c=i;
      }
    }
    closest=*c;
    unvisited.erase(c);
    visited.push_back(closest);
    if (closest!=src.GetNumber()) { 
      links.push_back(Link(pred[closest],closest,0,0,0));
    }
    deque<Link*> *adj= GetOutgoingLinks(FindMatchingNode(&Node(closest,0,0,0)));
    for (deque<Link*>::const_iterator i=adj->begin();i!=adj->end();++i) {
      unsigned dest=(**i).GetDest();
      double dist=(**i).GetLatency();
      if (dist<distance[dest]) { 
	distance[dest]=dist;
	pred[dest]=closest;
      }
    }
    delete adj;
  }
}

void Topology::CollectShortestPathLinks(const Node &src, const Node &dest, deque<Link> &links) 
{
  CollectShortestPathTreeLinks(src,links);
}



