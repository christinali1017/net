#include "table.h"
#include <limits>

//===============================================================================
//=====================================Generic===================================

#if defined(GENERIC)
ostream & Table::Print(ostream &os) const
{
  // WRITE THIS
  os << "Table()";
  return os;
}
#endif

//==============================================================================
//==================================Link State==================================


#if defined(LINKSTATE)

Table::Table(map<unsigned, pair<unsigned, double> > table){
	lstable = table;
}

ostream & Table::Print(ostream &os) const
{
  map<unsigned, pair<unsigned, double> > ls;
  ls.insert(lstable.begin(), lstable.end());
  for (map<unsigned, pair<unsigned, double> >::iterator ls_it = ls.begin(); ls_it != ls.end(); ++ls_it) {
    os << (ls_it->second).first << " -> " << ls_it->first << " = " << (ls_it->second).second << endl;
  }
  return os;
}


#endif

//===============================================================================
//===================================Distance Vector=============================

#if defined(DISTANCEVECTOR)
		Table::Table(){}

		// contruct a n*n matrix, n is the index of the largest number among the node and its neighbors
		Table::Table(Node *node)
		{
			
			unsigned src = node->GetNumber(); // initial size of table
			table = new vector<vector<double> >(src+1, vector<double>(src+1,std::numeric_limits<double>::infinity()));
			for(unsigned i = 0; i<GetTableSize(); i++)
			{
				(*table)[i][i] = 0;
			}
		}

		Table::Table(const Table &rhs): table(rhs.table){

		}

		Table & Table::operator=(const Table &rhs)
		{
			return *(new(this)Table(rhs));
		}

		void Table::SetEntry(unsigned src, unsigned dest, double lat)
		{
			unsigned curtablesize = GetTableSize();
			if(dest+1 > curtablesize)
			{
				for(unsigned i = 0; i<curtablesize; i++)
				{	
					for(unsigned j = curtablesize; j<dest+1;j++)
					{
						(*table)[i].push_back(std::numeric_limits<double>::infinity());
					}
					
				}
				for(unsigned i = curtablesize; i<dest+1; i++)
				{
					table->push_back(vector<double>(dest+1, std::numeric_limits<double>::infinity()));
				}
				for(unsigned i = 0; i<GetTableSize(); i++)
				{
					(*table)[i][i] = 0;
				}

			}


			if(src+1 > curtablesize)
			{
				for(unsigned i = 0; i<curtablesize; i++)
				{	
					for(unsigned j = curtablesize; j<src+1;j++)
					{
						(*table)[i].push_back(std::numeric_limits<double>::infinity());
					}
					
				}
				for(unsigned i = curtablesize; i<src+1; i++)
				{
					table->push_back(vector<double>(src+1, std::numeric_limits<double>::infinity()));
				}
				for(unsigned i = 0; i<GetTableSize(); i++)
				{
					(*table)[i][i] = 0;
				}

			}

			(*table)[src][dest] = lat;
		}

		double Table::GetEntry(unsigned src, unsigned dest)
		{
			//cerr<<"GetEntry src:"<<src<<" dest:"<<dest<<endl;
			return (*table)[src][dest];
		}

		//get the row by row index
		vector<double>* Table::GetRow(unsigned src)
		{
			vector<double>* row = new vector<double>(table->size(),  std::numeric_limits<double>::infinity());
			for(unsigned i = 0; i < table->size(); i++){

				(*row)[i] = (*table)[src][i];
			}

			return row;
		}

		//reset the row by index and row
		void Table::SetRow(unsigned rownum, vector<double> row)
		{
			(*table)[rownum] = row;
		}

		unsigned Table::GetTableSize()
		{
			return table->size();
		}

		unsigned Table::GetTableColSize()
		{
			return (*table)[0].size();
		}

		ostream & Table::Print(ostream &os) const
		{
		  // WRITE THIS
			for(unsigned i = 0; i<table->size(); i++)
				{	
					for(unsigned j = 0; j<table->size();j++)
					{
						cerr << (*table)[i][j]<<"  ";
					}
					cerr<<"\n";
				}
				
			os << "Table()";
			return os;
		}

		void Table::PrintTable(){
			cerr<<"=============print table===============\n";
			for(unsigned i = 0; i<table->size(); i++)
			{	
				for(unsigned j = 0; j<table->size();j++)
				{
					cerr << (*table)[i][j]<<"  ";
				}
				cerr<<"\n";
			}
		}


		void Table::PrintTable(Node* node) const{
			cerr<<"=============print table==============="<<node->GetNumber()<<"\n";
			for(unsigned i = 0; i<table->size(); i++)
			{	
				for(unsigned j = 0; j<table->size();j++)
				{
					cerr << (*table)[i][j]<<"  ";
				}
				cerr<<"\n";
			}
		}



#endif
