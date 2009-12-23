/*
	A* implementation with SFBDS

	notes:
	- the hash function is from statehash.h and hence only supports
	  a maximum of 2^32 states for graphState and dimensions
	  2^16x2^16 for map locations
*/

#ifndef SFBDS_AStar
#define SFBDS_AStar

#include <queue>
#include <ext/hash_set>
#include <cstdlib>
#include "SearchEnvironment.h"
#include "GraphEnvironment.h"
#include "Map2DEnvironment.h"
#include "statehash.h"


template<class state, class action>
class SFBDSAStar {

public:
	SFBDSAStar( SearchEnvironment<state,action> *env );
	~SFBDSAStar();
	
	/*----------------------------------------------------------------------------
	| type definitions
	----------------------------------------------------------------------------*/
	class QueueNode {
		public:
		state s1, s2; // search a path from s1 to s2

		// note that we would actually only need one parent
		// BUT, we might want to use this test code to expand both sides
		// and work on the cross product...
		state p1, p2; // parent of s1 and s2, we only have to keep one parent for
		              // backward reference but we use the second one for more pruning
		unsigned int coming_from;
		  // smallest bit sets whether p1 is set
			// second bit sets whether p2 is set
			// third bit sets whether we come from p1 (=0) or p2 (=1)
		double gcost1, gcost2; // gcosts from start and goal
		double fcost; // gcost1 + gcost2 + hcost where hcost = h(s1,s2)
	};

	/*----------------------------------------------------------------------------
	| search functions
	----------------------------------------------------------------------------*/
	// this procedure does the entire search
	double astar( state start, state goal, std::vector<state> &path, int expandheuristic_param = 0 );

	// for visualization purposes we can also initialize and search step by step
	void initialize( state start, state goal, int expandheuristic_param = 0 );
	// expands the next node
	// (takes nodes from the open queue until one can be expanded)
	// the flag expanded returns which side has been expanded
	// if true => start side, if false => goal side
	QueueNode step( bool &expanded );

	/*----------------------------------------------------------------------------
	| visualization
	----------------------------------------------------------------------------*/
	void OpenGLDraw( GLdouble rad ) const;
	void OpenGLDraw( QueueNode q, bool side, GLdouble rad ) const;

	/*----------------------------------------------------------------------------
	| statistics
	----------------------------------------------------------------------------*/
	unsigned int nodesExpanded;
	unsigned int nodesPoppedFromOpenQueue;
	unsigned int successorsTouched;
	//unsigned int predecessorOperatorPruning;
	unsigned int distancePruning;
	unsigned int distanceSuccessorPruning;
	unsigned int closedListPrunes;
	unsigned int numberOfJumps;
	unsigned int numberOfJumpsInSolution;
	unsigned int reopenedNodes;
	unsigned int bpmxUpdates;

protected:

	// the most important function to this research
	// if expand_heuristic is true the start will be expanded
	// otherwise the goal will be expanded
	bool expandheuristic( state &start, state &goal, unsigned int coming_from );

	// updates the coming_from tag as described below
	unsigned int update_coming_from( unsigned int &old_flag, bool &expand );

	struct QueueNodeCompare {
		bool operator() ( const QueueNode q1, const QueueNode q2 ) const {
			if( fequal( q1.fcost, q2.fcost ) )
				return( fless( q1.gcost1 + q1.gcost2, q2.gcost1 + q2.gcost2 ) );
			return( q1.fcost > q2.fcost );
		};
	};
	typedef std::priority_queue<QueueNode, std::vector<QueueNode>, QueueNodeCompare> AStarOpenQueue;

	// Definition of CLOSED queue
	struct QueueNodeHash {
		size_t operator() ( const QueueNode q ) const {
			return sfbds_state_hash<state>( q.s1, q.s2 );
		};		
	};
	struct QueueNodeEqual {
		size_t operator() ( const QueueNode q1, const QueueNode q2 ) const {
			return( (q1.s1 == q2.s1 && q1.s2 == q2.s2) || (q1.s1 == q2.s2 && q1.s2 == q2.s1) );
		};
	};
	typedef __gnu_cxx::hash_set<QueueNode, QueueNodeHash, QueueNodeEqual> AStarClosedList;

	// closed lists for single searches from start and goal
	typedef __gnu_cxx::hash_map<state, double> DistanceList;

	// traces back the path from q using the closed list
	void trace_back_path( QueueNode q, std::vector<state> &path );

	// variables
	AStarOpenQueue open;
	AStarClosedList closed;
	DistanceList distances_from_start, distances_from_goal;

	SearchEnvironment<state,action> *env;

	int expandheuristic_param;

	private:
	double sanity_fcost_check;


};


/*------------------------------------------------------------------------------
| Implementation
------------------------------------------------------------------------------*/

// Header
template<class state, class action>
SFBDSAStar<state,action>::SFBDSAStar( SearchEnvironment<state,action> *_env ):
	env( _env ) {
};

template<class state, class action>
SFBDSAStar<state,action>::~SFBDSAStar() {
	// the following is not needed because open and closed are normal variables in the class
	// reassign open to an empty queue
	/*
	open = AStarOpenQueue();
	// clear closed
	closed.clear();
	distances_from_start.clear();
	distances_from_goal.clear();
	std::cout << "destroyed class and cleared everything" << std::endl << std::flush;
	*/
};


template<class state, class action>
void SFBDSAStar<state,action>::initialize( state s1, state s2, int _expandheuristic_param ) {
	// reset counters
	nodesExpanded = 0;
	nodesPoppedFromOpenQueue = 0;
	successorsTouched  = 0;
	//predecessorOperatorPruning = 0;
	distancePruning = 0;
	distanceSuccessorPruning = 0;
	closedListPrunes = 0;
	numberOfJumps = 0;
	numberOfJumpsInSolution = 0;
	reopenedNodes = 0;
	bpmxUpdates = 0;

	expandheuristic_param = _expandheuristic_param;

	// cleanup just to make sure things are proper
	open = AStarOpenQueue();
	closed.clear();
	distances_from_start.clear();
	distances_from_goal.clear();

	// push the initial state onto the queue
	QueueNode q;
	q.s1 = s1; q.s2 = s2; q.coming_from = 0;
	q.gcost1 = 0;
	q.gcost2 = 0;
	q.fcost = env->HCost( s1, s2 );
	sanity_fcost_check = q.fcost;
	open.push( q );

	return;
};


template<class state,class action>
typename SFBDSAStar<state,action>::QueueNode SFBDSAStar<state,action>::step( bool &expand ) {

	typename AStarClosedList::iterator closediter1 = closed.begin(); // used for expanded node
	typename AStarClosedList::iterator closediter2 = closed.begin(); // used for successors
	typename DistanceList::iterator distancesiter;
	std::vector<state> neighbors;
	typename std::vector<state>::iterator stateiter;
	std::vector<QueueNode> successors;
	typename std::vector<QueueNode>::iterator nodeiter;
	QueueNode q, successor;	

	expand = true;

	while( !open.empty() ) {

		q = open.top(); open.pop();
		nodesPoppedFromOpenQueue++;

		// sanity check for BPMX that makes the fcost non-decreasing
		if( fgreater( sanity_fcost_check, q.fcost ) ) {
			std::cerr << "ERROR: fcost decreased in the search although we use BPMX" << std::endl << std::flush;
			exit( 1 );
		}

		if( q.s1 == q.s2 ) return q; // end the search when a terminal is found

		// TODO: reenable this pruning!!!
/*
		// prune if the distance from the original start is non-optimal
		distancesiter = distances_from_start.find( q.s1 );
		if( distancesiter != distances_from_start.end() && fless( distancesiter->second, q.gcost1 ) ) {
			distancePruning++;
			continue;
		} else
			distances_from_start[q.s1] = q.gcost1;

		// prune if the distance from the original goal is non-optimal
		distancesiter = distances_from_goal.find( q.s2 );
		if( distancesiter != distances_from_goal.end() && fless( distancesiter->second, q.gcost2 ) ) {
			distancePruning++;
			continue;
		} else
			distances_from_goal[q.s2] = q.gcost2;
*/

		// check whether q is already in the closed list
		closediter1 = closed.find( q );
		// prune if we have q already in the closed list with a smaller or equal overall g cost
		if( closediter1 != closed.end() &&
		    (q.gcost1 + q.gcost2 >= closediter1->gcost1 + closediter1->gcost2) ) {
			closedListPrunes++;
			continue;
		}

		// if we are to expand the node
		break;
	}


	// verbose
	if( closediter1 != closed.end() )
		reopenedNodes++;

	// verbose
	//std::cout << "expanding: " << q.s1 << " " << q.s2 << std::endl;

	// now find out which side of q to expand
	expand = expandheuristic( q.s1, q.s2, q.coming_from );
	if( expand ) env->GetSuccessors( q.s1, neighbors ); // expand q.s1
	else         env->GetSuccessors( q.s2, neighbors ); // expand q.s2
	nodesExpanded++;

	// set the hcost of the current node
	double max_root_hcost;
	// if there is a node in the closed list with this current hcost go for it
	if( closediter1 != closed.end() )
		max_root_hcost = std::max( q.fcost - q.gcost1 - q.gcost2, closediter1->fcost - closediter1->gcost1 - closediter1->gcost2 );
	else
		max_root_hcost = q.fcost - q.gcost1 - q.gcost2;

	// generate all successors and check them against the closed list
	for( stateiter = neighbors.begin(); stateiter != neighbors.end(); stateiter++ ) {

		double transit_cost;
		double hcost;

		successorsTouched++;
		if( expand ){
			successor.p1 = q.s1;
			successor.p2 = q.p2;
			successor.coming_from = update_coming_from( q.coming_from, expand );
			successor.s1 = *stateiter;
			successor.s2 = q.s2;
			transit_cost = env->GCost( q.s1, successor.s1 );
			successor.gcost1 = q.gcost1 + transit_cost;
			successor.gcost2 = q.gcost2;

		// TODO: reenable this pruning!!!
/*
			// check whether new successor has sub-optimal distance from the start
			distancesiter = distances_from_start.find( successor.s1 );
			if( distancesiter != distances_from_start.end() &&
			    fless( distancesiter->second, successor.gcost1 ) ) {
				distanceSuccessorPruning++;
				continue;
			} else
				distances_from_start[successor.s1] = successor.gcost1;
*/
		} else {
			successor.p2 = q.s2;
			successor.p1 = q.p1;
			successor.coming_from = update_coming_from( q.coming_from, expand );
			successor.s1 = q.s1;
			successor.s2 = *stateiter;
			successor.gcost1 = q.gcost1;
			transit_cost = env->GCost( q.s2, successor.s2 );
			successor.gcost2 = q.gcost2 + transit_cost;

		// TODO: reenable this pruning!!!
/*
			// check whether new successor has sub-optimal distance from the goal
			distancesiter = distances_from_goal.find( successor.s2 );
			if( distancesiter != distances_from_goal.end() &&
			    fless( distancesiter->second, successor.gcost2 ) ) {
				distanceSuccessorPruning++;
				continue;
			} else
				distances_from_goal[successor.s2] = successor.gcost2;
*/
		}

		// check whether the successor is already in the closed list
		closediter2 = closed.find( successor );
		if( closediter2 != closed.end() &&
		    successor.gcost1 + successor.gcost2 >= closediter2->gcost1 + closediter2->gcost2 )
			continue;

		// finding the hcost of the successor
		hcost = env->HCost( successor.s1, successor.s2 );
		successor.fcost = successor.gcost1 + successor.gcost2 + hcost;
		successors.push_back( successor );

		max_root_hcost = std::max( max_root_hcost, hcost - transit_cost );
	} // for all successors

	if( fless( q.fcost, q.gcost1 + q.gcost2 + max_root_hcost ) ) {
		// update the hcost of the parent
		q.fcost = q.gcost1 + q.gcost2 + max_root_hcost;
		bpmxUpdates++;
	}
	// if the element was in the closed list already delete it from there
	if( closediter1 != closed.end() )
		closed.erase( closediter1 );
	// write q to the closed list
	closed.insert( q );

	// now update all the hcosts of the successors (BPMX)
	for( nodeiter = successors.begin(); nodeiter != successors.end(); nodeiter++ ) {
		double transit_cost = nodeiter->gcost1 - q.gcost1 + nodeiter->gcost2 - q.gcost2;
		double hcost = nodeiter->fcost - nodeiter->gcost1 - nodeiter->gcost2;
		// maximize BPMX step
		if( hcost < max_root_hcost - transit_cost ) {
			bpmxUpdates++;
			hcost = max_root_hcost - transit_cost;
			// update the fcost
			nodeiter->fcost = nodeiter->gcost1 + nodeiter->gcost2 + hcost;
		};
		// insert into open queue
		open.push( *nodeiter );
	}

	return q;
};


// SFBDS A* implementation
template<class state, class action>
double SFBDSAStar<state,action>::astar( state s1, state s2, std::vector<state> &path, int expandheuristic_param ) {

	// reset variables
	path.clear();

	initialize( s1, s2, expandheuristic_param );

	if( s1 == s2 ) return 0.;

	QueueNode q;
	bool expanded;
	// iterate over the queue
	while( !open.empty() ) {
		q = step( expanded );
		// verbose
		//std::cout
		//	<< "q = (s1=" << q.s1
		//	<< " s2="     << q.s2
		//	<< " coming_from=" << q.coming_from
		//	<< " p1="     << q.p1
		//	<< " p2="     << q.p2
		//	<< " fcost="  << q.fcost
		//	<< " gcost1=" << q.gcost1
		//	<< " gcost2=" << q.gcost2
		//	<< ")" << std::endl;
		if( q.s1 == q.s2 ) break;
	}

	trace_back_path( q, path );
	
	return( q.gcost1 + q.gcost2 );
};


template<class state, class action>
unsigned int SFBDSAStar<state,action>::update_coming_from( unsigned int &old_flag, bool &expand ) {
	// we still have to code this for little endian machines

	unsigned int new_flag = old_flag;

	// set the parent bit to 1
	if( expand ) new_flag |= 1;
	else         new_flag |= 2;

	if( expand ) new_flag &= ~4; // set the third bit to 0, hence coming from p1
	else         new_flag |= 4; // set the third bit to 1, hence coming from p2

	return new_flag;
};


template<class state, class action>
void SFBDSAStar<state,action>::trace_back_path( QueueNode q, std::vector<state> &path ) {
	typename AStarClosedList::iterator closediter;

	// safety check
	assert( q.s1 == q.s2 );

	numberOfJumpsInSolution = 0;
	unsigned int coming_from = q.coming_from & 4;

	// safety if the end position does not have a parent
	path.push_back( q.s1 );

	while( q.coming_from & 3 ) {

		if( ( q.coming_from & 4 ) != coming_from ) {
			coming_from = q.coming_from & 4;
			numberOfJumpsInSolution++;
		}

		if( q.coming_from & 4 ) {
			path.insert( path.end(), q.p2 );
			q.s2 = q.p2;
		} else {
			path.insert( path.begin(), q.p1 );
			q.s1 = q.p1;
		}

		closediter = closed.find( q );
		if( closediter == closed.end() ) {
			std::cout << "ERROR: could not retrace the solution." << std::endl;
			exit(1);
		}
		q = (*closediter);
	}

	// if the first state we expanded was the goal we count this as a jump
	if( coming_from )
		numberOfJumpsInSolution++;

	return;
};



/*------------------------------------------------------------------------------
| visualization
------------------------------------------------------------------------------*/
// see astar.c for implementation
template<>
void SFBDSAStar<graphState,graphMove>::OpenGLDraw( GLdouble rad ) const;
template<>
void SFBDSAStar<graphState,graphMove>::OpenGLDraw( QueueNode q, bool side, GLdouble rad ) const;
template<>
void SFBDSAStar<xyLoc,tDirection>::OpenGLDraw( GLdouble rad ) const;
template<>
void SFBDSAStar<xyLoc,tDirection>::OpenGLDraw( QueueNode q, bool side, GLdouble rad ) const;


/*------------------------------------------------------------------------------
| heuristic for selecting which node to expand
------------------------------------------------------------------------------*/
template<class state, class action>
bool SFBDSAStar<state,action>::expandheuristic( state &start, state &goal, unsigned int coming_from ) {

	unsigned int num_start, num_goal;
	double h_start = 0., h_goal = 0.;
	long r;
	std::vector<state> successors;
	bool result = true;

	switch( expandheuristic_param ) {
		case 0: return true; // always expand the start node
		case 1: return false; // always expand the goal node

		case 2:
			// return the side with smaller branching factor
			env->GetSuccessors( start, successors );
			num_start = successors.size();
			successors.clear();
			env->GetSuccessors( goal, successors );
			num_goal  = successors.size();
			// verbose
			//std::cout << "comparing " << start << "@" << num_start << " with " << goal << "@" << num_goal << std::endl;

			// if both sides are equal do not change previous direction
			if( num_start == num_goal && (coming_from&3) )
				// if coming_from&4 is 0 then we come from p1 => return true
				// if coming_from&4 is 1 then we come from p2 => return false
				return( !(bool)(coming_from&4) );

			// else return the side with smaller branching factor
			result = (num_start <= num_goal);
			break;

		case 3:
			// return randomly weighted with the branching factor of each side
			env->GetSuccessors( start, successors );
			num_start = successors.size();
			successors.clear();
			env->GetSuccessors( goal, successors );
			num_goal = successors.size();
			r = random();

			result = ( (double)r < (double)RAND_MAX/(double)(num_start+num_goal) * (double)num_start );
			break;

		case 4:
			// return the side with the higher average hcost
			env->GetSuccessors( start, successors );
			for( typename std::vector<state>::iterator it = successors.begin(); it != successors.end(); it++ )
				h_start += env->HCost( *it, goal );
			h_start /= (double)successors.size();
			successors.clear();
			env->GetSuccessors( goal, successors );
			for( typename std::vector<state>::iterator it = successors.begin(); it != successors.end(); it++ )
				h_goal += env->HCost( *it, start );
			h_goal /= (double)successors.size();
			// verbose
			//std::cout << "average hcosts for (" << start << "," << goal << ") is " << h_start << " " << h_goal << std::endl;

			// if both averages are the same do not change direction
			if( fequal( h_start, h_goal ) && (coming_from&3) )
				return( !(bool)(coming_from&4) );
			// otherwise return the side with greater average heuristic value
			result = (h_start >= h_goal);
			break;

		case 5:
			// jump if and only if both sides have branching factor of 2
			env->GetSuccessors( start, successors );
			num_start = successors.size();
			successors.clear();
			env->GetSuccessors( goal, successors );
			num_goal  = successors.size();

			if( num_start == 2 && num_goal == 2 )
				result = (bool)(coming_from&4); // change direction
			else if( num_start == 2 )
				result = false;
			else if( num_goal == 2 )
				result = true;
			else
				return( !(bool)(coming_from&4) ); // do not change direction
			break;

	}

	// determine whether we have a jump
	if( (coming_from&3) && ( (bool)(coming_from&4) == result ) )
		numberOfJumps++;

	return result;
};


#endif
