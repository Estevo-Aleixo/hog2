/*
 *  AStarDelay.cpp
 *  hog2
 *
 *  Created by Nathan Sturtevant on 9/27/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "AStarDelay.h"

using namespace AStarDelayUtil;
using namespace GraphSearchConstants;

const static bool verbose = true;

void AStarDelay::GetPath(GraphEnvironment *_env, Graph* _g, graphState from, graphState to, std::vector<graphState> &thePath) 
{
	if (!InitializeSearch(_env,_g,from,to,thePath))
		return;
	
	while(!DoSingleSearchStep(thePath)) 
		{}
	
	if (thePath.size() > 0)
		printf("\nNodes expanded=%ld, Nodes touched=%ld.\n",GetNodesExpanded(),GetNodesTouched());
}

bool AStarDelay::InitializeSearch(GraphEnvironment *_env, Graph* _g, graphState from, graphState to, std::vector<graphState> &thePath) 
{
	env = _env;
	g = _g;
	nodesTouched = nodesExpanded = nodesReopened = 0;
	start = from;
	goal = to;
	
	closedList.clear();
	openQueue.reset();
	delayQueue.reset();
	fQueue.reset();
		
	if ((from == UINT32_MAX) || (to == UINT32_MAX) || (from == to))
	{
		thePath.resize(0);
		return false;
	}
	
	// step (1)
	SearchNode first(env->HCost(start, goal), 0, start, start);
	openQueue.Add(first);
	
	F = 0;
		
	return true;
}

bool AStarDelay::DoSingleSearchStep(std::vector<graphState> &thePath)
{
	static bool canReopen = false;

	SearchNode topNode;
	bool found = false;
	
	// three lists (1) f-cost with f>F (2) reopen (by g-cost) with f<F
	// (3) open (by g-cost) with f<F

	// if we are allowed to reopen, take the node with lowest between lists (2) and (3)
	// otherwise take from (3)/(2), else take from (1)
	if ((canReopen) && (delayQueue.size() > 0) && (fQueue.size() > 0))
	{
		if (fless(delayQueue.top().gCost, fQueue.top().gCost))
		{
			topNode = delayQueue.Remove();
			canReopen = false;
			nodesReopened++;
		}
		else {
			topNode = fQueue.Remove();
			canReopen = true;
		}
		found = true;
	}
	else if ((canReopen) && (delayQueue.size() > 0))
	{
		nodesReopened++;
		topNode = delayQueue.Remove();
		canReopen = false;
		found = true;
	}
	else if (fQueue.size() > 0)
	{
		topNode = fQueue.Remove();
		canReopen = true;
		found = true;
	}
	else if ((openQueue.size() > 0))
	{
		F = topNode.fCost;
		topNode = openQueue.Remove();
		canReopen = true;
		found = true;
	}

	if (found)
	{
		DoSingleStep(topNode, thePath);
	}
	else {
		thePath.resize(0); // no path found!
		closedList.clear();
		openQueue.reset();
		env = 0;
		return true;
	}
	return false;
}

/* The steps refer to the pseudo codes of the corresponding algorithms */

bool AStarDelay::DoSingleStep(SearchNode &topNode,
															std::vector<graphState> &thePath)
{
	nodesExpanded++;
	
	if (verbose)
	{
		printf("Expanding node %ld , gcost=%lf, h=%lf, f=%lf.\n",
					 topNode.currNode, topNode.gCost, topNode.fCost-topNode.gCost, topNode.fCost);
	}
	
	// and if there are no lower f-costs on the other open lists...
	// otherwise we need to delay this node
	if (env->GoalTest(topNode.currNode, goal))
	{
		ExtractPathToStart(topNode.currNode, thePath);
		return true;
	}
	
	// step (5), computing gi is delayed
	neighbors.resize(0);
	env->GetSuccessors(topNode.currNode, neighbors);
	
	double minCost = DBL_MAX;
	for (unsigned int x = 0; x < neighbors.size(); x++)
	{
		nodesTouched++;
		
		double cost = HandleNeighbor(neighbors[x], topNode);
		if (fless(cost, minCost))
			minCost = cost;
	}

	// path max rule (i or ii?)
	if (fless(topNode.fCost-topNode.gCost, minCost))
		topNode.fCost = topNode.gCost+minCost;
	closedList[topNode.currNode] = topNode;

	return false;
}

double AStarDelay::HandleNeighbor(graphState neighbor, SearchNode &topNode)
{
	
	if (openQueue.IsIn(SearchNode(neighbor))) // in OPEN
	{
		return UpdateOpenNode(neighbor, topNode);
	} 
	else if (closedList.find(neighbor) != closedList.end()) // in CLOSED
	{
		return UpdateClosedNode(neighbor, topNode);
	} 
	else if (delayQueue.IsIn(SearchNode(neighbor))) // in DELAY
	{
		return UpdateDelayedNode(neighbor, topNode);
	} 
	else if (fQueue.IsIn(SearchNode(neighbor))) // in low g-cost!
	{
		return UpdateLowGNode(neighbor, topNode);
	}
	else { // not opened yet
		return AddNewNode(neighbor, topNode);
	}
	return 0;
}

// return edge cost + h cost
double AStarDelay::AddNewNode(graphState neighbor, SearchNode &topNode)
{
	graphState topNodeID = topNode.currNode;
	double edgeCost = env->GCost(topNodeID,neighbor);
	double gcost = topNode.gCost + edgeCost;
	double h = env->HCost(neighbor,goal);
	double fcost = gcost + h;
	
	SearchNode n(fcost, gcost, neighbor, topNodeID);
	if (fless(fcost, F))
	{
		fQueue.Add(n); // nodes with cost < F
	}
	else {
		openQueue.Add(n);
	}
	return edgeCost+n.fCost-n.gCost;
}

// return edge cost + h cost
double AStarDelay::UpdateOpenNode(graphState neighbor, SearchNode &topNode)
{
	// lookup node
	SearchNode n;
	n = openQueue.find(SearchNode(neighbor));
	double edgeCost = env->GCost(topNode.currNode, neighbor);

	if (fless(topNode.gCost+edgeCost, n.gCost))
	{
		n.fCost -= n.gCost;
		n.gCost = topNode.gCost+edgeCost;
		n.fCost += n.gCost;
		n.prevNode = topNode.currNode;
		openQueue.DecreaseKey(n);
	}
	
	// return value for pathmax
	return edgeCost+n.fCost-n.gCost;
}

// return edge cost + h cost
double AStarDelay::UpdateClosedNode(graphState neighbor, SearchNode &topNode)
{
	// lookup node
	SearchNode n = closedList[neighbor];
	double edgeCost = env->GCost(topNode.currNode, neighbor);
	// check if we should update cost
	if (fless(topNode.gCost+edgeCost, n.gCost))
	{
		// do pathmax here -- update child-h to parent-h - edge cost
		if (fgreater(topNode.fCost-topNode.gCost, n.fCost-n.gCost))
		{
			n.gCost = topNode.gCost+edgeCost;
			// new heuristic value
			n.fCost = topNode.fCost-topNode.gCost-edgeCost;
			n.fCost += n.gCost;
		}
		else {
			n.fCost = n.fCost-n.gCost+topNode.gCost+edgeCost;
		}
		n.prevNode = topNode.currNode;

		// put into delay list if we can open it
		closedList.erase(neighbor);  // delete from CLOSED
		delayQueue.Add(n); // add to delay list
	}
	else if (fgreater(topNode.fCost-topNode.gCost, n.fCost-n.gCost))
	{
		n.fCost = topNode.fCost-topNode.gCost-edgeCost;
		n.fCost += n.gCost;
		closedList[neighbor] = n;
	}
	// return value for pathmax
	return edgeCost+n.fCost-n.gCost;
}

// return edge cost + h cost
double AStarDelay::UpdateDelayedNode(graphState neighbor, SearchNode &topNode)
{
	// lookup node
	SearchNode n;
	n = delayQueue.find(SearchNode(neighbor));
	double edgeCost = env->GCost(topNode.currNode, neighbor);

	if (fless(topNode.gCost+edgeCost, n.gCost))
	{
		n.fCost -= n.gCost;
		n.gCost = topNode.gCost+edgeCost;
		n.fCost += n.gCost;
		n.prevNode = topNode.currNode;
		delayQueue.DecreaseKey(n);
	}
	
	// return value for pathmax
	return edgeCost+n.fCost-n.gCost;
}

// return edge cost + h cost
double AStarDelay::UpdateLowGNode(graphState neighbor, SearchNode &topNode)
{
	// lookup node
	SearchNode n;
	n = fQueue.find(SearchNode(neighbor));
	double edgeCost = env->GCost(topNode.currNode, neighbor);
	
	if (fless(topNode.gCost+edgeCost, n.gCost))
	{
		n.fCost -= n.gCost;
		n.gCost = topNode.gCost+edgeCost;
		n.fCost += n.gCost;
		n.prevNode = topNode.currNode;
		fQueue.DecreaseKey(n);
	}
	
	// return value for pathmax
	return edgeCost+n.fCost-n.gCost;
}
	

//{
//	SearchNode n(f, gcost, neighbor,topNodeID);
//			openQueue.Add(n);
//			
//			if (verbose)
//			{
//				printf("Adding node %ld to OPEN, gcost=%lf, h=%lf, f=%lf.\n",neighbor,gcost,f-gcost,f);
//			}
//		}
//		
//		/* step (7) */
//		else 
//		{
//			SearchNode neighborNode;
//			if (openQueue.IsIn(SearchNode(neighbor)))
//			{
//				neighborNode = openQueue.find(SearchNode(neighbor));
//				
//				//if (neighborNode.gCost <= gcost)
//				if (!fgreater(neighborNode.gCost,gcost))
//					continue;
//				
//				if (verbose)
//				{
//					printf("Adjusting node %ld in OPEN, gcost=%lf, h=%lf, f=%lf; g_old=%lf.\n",neighbor,gcost,f-gcost,f, neighborNode.gCost);
//				}
//				
//				neighborNode.copy(f,gcost,neighbor,topNodeID); // parent is changed
//				openQueue.DecreaseKey(neighborNode);  // adjust its position in OPEN
//			}
//			else if (closedList.find(neighbor) != closedList.end())
//			{
//				neighborNode = closedList.find(neighbor)->second;
//				
//				//if (neighborNode.gCost <= gcost)
//				if (!fgreater(neighborNode.gCost,gcost))
//					continue;
//				
//				if (verbose)
//				{
//					printf("Moving node %ld from CLOSED to OPEN, gcost=%lf, h=%lf, f=%lf; g_old=%lf.\n",neighbor,gcost,f-gcost,f, neighborNode.gCost);
//				}
//				
//				neighborNode.copy(f,gcost,neighbor,topNodeID);  // parent is changed
//				closedList.erase(neighbor);  // delete from CLOSED
//				
//				openQueue.Add(neighborNode); // add to OPEN
//			}
//		}
//	}
//	
//	return false;

///* Algorithm B. step (3) is modified from algorithm A.*/
//bool AStarDelay::DoSingleStepB(std::vector<graphState> &thePath)
//{
//	/* step (2) */
//	if (openQueue.size() == 0)
//	{
//		thePath.resize(0); // no path found!
//		closedList.clear();
//		openQueue.reset();
//		FCache.reset();
//		env = 0;
//		return true;
//	}
//	
//	/* step (3) */
//	nodesExpanded++;
//	
//	// put those with f < F into cache
//	FCache.reset();
//	while(openQueue.size() > 0)
//		
//	{
//		SearchNode tmpNode = openQueue.top();
//		if (fless(tmpNode.fCost, F))
//		{
//			FCache.Add(openQueue.Remove());
//		}
//		else
//			break;
//	}
//	
//	// select the node to expand
//	SearchNode topNode;
//	if (FCache.size() > 0) 
//	{
//		topNode = FCache.Remove();
//	}
//	else 
//	{
//		topNode = openQueue.Remove();
//		
//		if(fgreater(topNode.fCost,F)) 
//		  {
//		    F = topNode.fCost; // update F
//		    if (verbose) 
//		      {
//			printf("F updated to %lf.\n",F);
//		      }
//		  }
//	}
//	
//	// move remaining nodes from FCache back to openQueue
//	while (FCache.size() > 0)
//	{
//		openQueue.Add(FCache.Remove());
//	}
//	
//	
//	graphState topNodeID = topNode.currNode;
//	closedList[topNodeID] = topNode;
//	
//	if (verbose)
//	{
//		printf("Expanding node %ld , gcost=%lf, h=%lf, f=%lf.\n",topNodeID,topNode.gCost,topNode.fCost-topNode.gCost,topNode.fCost);
//	}
//	
//	/* step (4) */
//	if (env->GoalTest(topNodeID, goal))
//	{
//		ExtractPathToStart(topNodeID, thePath);
//		//closedList.clear();
//		//openQueue.reset();
//		FCache.reset();
//		//env = 0;
//
//		// don't reset everything yet, since they are required by the rendering funciton
//		return true;
//	}
//	
//	/* step (5), computing gi is delayed */
//	neighbors.resize(0);
//	env->GetSuccessors(topNodeID, neighbors);
//	
//	for(unsigned int x = 0; x<neighbors.size(); x++)
//	{
//		nodesTouched++;
//		
//		/* step (5) */
//		graphState neighbor = neighbors[x];
//		double edgeWeight = env->GCost(topNodeID,neighbor);
//		double gcost = topNode.gCost + edgeWeight;
//		double h = env->HCost(neighbor,goal);
//		double f = gcost + h;
//		
//		/* step (6), neither in OPEN nor CLOSED */
//		if (!openQueue.IsIn(SearchNode(neighbor)) && closedList.find(neighbor) == closedList.end() )
//			
//		{
//			SearchNode n(f,gcost,neighbor,topNodeID);
//			openQueue.Add(n);
//			
//			if (verbose)
//			{
//				printf("Adding node %ld to OPEN, gcost=%lf, h=%lf, f=%lf.\n",neighbor,gcost,f-gcost,f);
//			}
//		}
//		/* step (7) */
//		else {
//			SearchNode neighborNode;
//			if (openQueue.IsIn(SearchNode(neighbor)))
//			{
//				neighborNode = openQueue.find(SearchNode(neighbor));
//				
//				//if (neighborNode.gCost <= gcost)
//				if (!fgreater(neighborNode.gCost,gcost))
//					continue;
//				
//				if (verbose) 
//				{
//					printf("Adjusting node %ld in OPEN, gcost=%lf, h=%lf, f=%lf; g_old=%lf.\n",neighbor,gcost,f-gcost,f, neighborNode.gCost);
//				}
//				
//				neighborNode.copy(f,gcost,neighbor,topNodeID); // parent is changed
//				openQueue.DecreaseKey(neighborNode);  // adjust its position in OPEN
//			}
//			else if (closedList.find(neighbor) != closedList.end()) 
//			{
//				neighborNode = closedList.find(neighbor)->second;
//				
//				//if (neighborNode.gCost <= gcost)
//				if (!fgreater(neighborNode.gCost,gcost))
//					continue;
//				
//				if (verbose) 
//				{
//					printf("Moving node %ld from CLOSED to OPEN, gcost=%lf, h=%lf, f=%lf; g_old=%lf.\n",
//								 neighbor,gcost,f-gcost,f, neighborNode.gCost);
//				}
//				
//				neighborNode.copy(f,gcost,neighbor,topNodeID);  // parent is changed
//				closedList.erase(neighbor);  // delete from CLOSED
//				
//				openQueue.Add(neighborNode); // add to OPEN
//			}
//		}
//	}
//	return false;
//}
//	
///* Algorithm B' . step (3a) (3b) are inserted into algorithm B. step (7) is also changed, since we need to store the updated h */
//bool AStarDelay::DoSingleStepBP(std::vector<graphState> &thePath) 
//{
//	/* step (2) */
//	if (openQueue.size() == 0)
//		
//	{
//		thePath.resize(0); // no path found!
//		closedList.clear();
//		openQueue.reset();
//		FCache.reset();
//		env = 0;
//		return true;
//	}
//	
//	/* step (3) */
//	nodesExpanded++;
//	
//	// put those with f < F into cache
//	FCache.reset();
//	while(openQueue.size() > 0) 
//	{
//		SearchNode tmpNode = openQueue.top();
//		if (fless(tmpNode.fCost , F)) 
//		{
//			FCache.Add(openQueue.Remove());
//		}
//		else
//			break;
//	}
//	
//	// select the node to expand
//	SearchNode topNode;
//	if (FCache.size() > 0) 
//	{
//		topNode = FCache.Remove();
//		printf("Expanding a node below F.\n");
//	}
//	else 
//	{
//		topNode = openQueue.Remove();
//		
//		if(fgreater(topNode.fCost,F)) 
//		{
//		  F = topNode.fCost; // update F
//		  if (verbose) 
//		    {
//		      printf("F updated to %lf.\n",F);
//		    }
//		}
//	}
//	
//	// move remaining nodes from FCache back to openQueue
//	while(FCache.size() > 0) 
//	{
//		openQueue.Add(FCache.Remove());
//	}
//	
//	graphState topNodeID = topNode.currNode;
//	closedList[topNodeID] = topNode;
//	
//	if (verbose) 
//	{
//		printf("Expanding node %ld , gcost=%lf, h=%lf, f=%lf.\n",topNodeID,topNode.gCost,topNode.fCost-topNode.gCost,topNode.fCost);
//	}
//	
//	/* step (4) */
//	if (env->GoalTest(topNodeID, goal))
//	{
//		ExtractPathToStart(topNodeID, thePath);
//		//closedList.clear();
//		//openQueue.reset();
//		FCache.reset();
//		//env = 0;
//
//		// don't reset everything yet, since they are required by the rendering funciton
//		return true;
//	}
//	
//	/* step (5), computing gi is delayed */
//	neighbors.resize(0);
//	env->GetSuccessors(topNodeID, neighbors);
//	
//	double hTop = topNode.fCost - topNode.gCost;
//	double minH2 = DBL_MAX; // min ( edgeWeight(i) + h(neighbor(i)) )
//	
//	for(unsigned int x = 0; x<neighbors.size(); x++) 
//	{
//		nodesTouched++;
//		
//		/* step (5) */
//		graphState neighbor = neighbors[x];
//		double edgeWeight = env->GCost(topNodeID,neighbor);
//		double gcost = topNode.gCost + edgeWeight;
//		
//		/* step Mero (3a) */
//		double h_tmp; // for printing reports only
//		double h;
//		// this is necessary because the modified h is stored in the node, not the environment
//		if (openQueue.IsIn(SearchNode(neighbor))) { 
//			SearchNode nb = openQueue.find(SearchNode(neighbor));
//			h = max( nb.fCost - nb.gCost, hTop - edgeWeight);
//			
//			h_tmp = nb.fCost - nb.gCost;
//		}
//		else if (closedList.find(neighbor) != closedList.end()) 
//		{
//			SearchNode nb = closedList.find(neighbor)->second;
//			h = max( nb.fCost - nb.gCost, hTop - edgeWeight);
//			
//			h_tmp = nb.fCost - nb.gCost;
//		}
//		else 
//		{
//			h = max( env->HCost(neighbor,goal), hTop - edgeWeight);
//			
//			h_tmp = env->HCost(neighbor,goal);
//		}
//		
//		if (verbose) 
//		{
//			if (fgreater(h,h_tmp))
//				printf("Improving h of node %ld by Mero rule (a), %lf->%lf\n",neighbor,h_tmp,h);
//		}
//		
//		double f = gcost + h;
//		
//		/* step Mero (3b) */
//		minH2 = min(minH2, h + edgeWeight);
//		
//		/* step (6), neither in OPEN nor CLOSED */
//		if (!openQueue.IsIn(SearchNode(neighbor)) && closedList.find(neighbor) == closedList.end() ) 
//		{
//			SearchNode n(f,gcost,neighbor,topNodeID);
//			openQueue.Add(n);
//			
//			if (verbose) 
//			{
//				printf("Adding node %ld to OPEN, gcost=%lf, h=%lf, f=%lf.\n",neighbor,gcost,f-gcost,f);
//			}
//		}
//		/* step (7) */
//		else 
//		{
//			SearchNode neighborNode;
//			if (openQueue.IsIn(SearchNode(neighbor))) 
//			{
//				neighborNode = openQueue.find(SearchNode(neighbor));
//				
//				//if (neighborNode.gCost <= gcost)
//				if (!fgreater(neighborNode.gCost,gcost)) 
//				{
//					if (fgreater(h , neighborNode.fCost - neighborNode.gCost)) 
//					{
//						// we may fail to update gcost, but still update h
//						f = h + neighborNode.gCost;
//						neighborNode.fCost = f;
//						openQueue.IncreaseKey(neighborNode);
//					}
//					continue;
//				}
//				
//				if (verbose) 
//				{
//					printf("Adjusting node %ld in OPEN, gcost=%lf, h=%lf, f=%lf; g_old=%lf.\n",neighbor,gcost,f-gcost,f, neighborNode.gCost);
//				}
//				
//				if (fless(f , neighborNode.fCost)) 
//				{
//					neighborNode.copy(f,gcost,neighbor,topNodeID); // parent is changed
//					openQueue.DecreaseKey(neighborNode);  // adjust its position in OPEN
//				}
//				else 
//				{
//					neighborNode.copy(f,gcost,neighbor,topNodeID); // parent is changed
//					openQueue.IncreaseKey(neighborNode);  // adjust its position in OPEN
//				}
//			}
//			else if (closedList.find(neighbor) != closedList.end()) 
//			{
//				neighborNode = closedList.find(neighbor)->second;
//				
//				//if (neighborNode.gCost <= gcost)
//				if (!fgreater(neighborNode.gCost,gcost)) 
//				{
//					if (fgreater(h , neighborNode.fCost - neighborNode.gCost)) 
//					{
//						// we may fail to update gcost, but still update h
//						f = h + neighborNode.gCost;
//						neighborNode.fCost = f;
//						closedList[neighbor] = neighborNode;
//					}
//					continue;
//				}
//				
//				if (verbose) 
//				{
//					printf("Moving node %ld from CLOSED to OPEN, gcost=%lf, h=%lf, f=%lf; g_old=%lf.\n",neighbor,gcost,f-gcost,f, neighborNode.gCost);
//				}
//				
//				neighborNode.copy(f,gcost,neighbor,topNodeID);  // parent is changed
//				closedList.erase(neighbor);  // delete from CLOSED
//				
//				openQueue.Add(neighborNode); // add to OPEN
//			}
//		}
//	}
//	/* step Mero (3b), update h of parent */
//	if (fgreater(minH2 , hTop)) 
//	{
//		topNode.fCost = minH2 + topNode.gCost;  // f = h + gcost
//		closedList[topNodeID] = topNode;
//		
//		if (verbose) 
//		{
//			printf("Improving h of node %ld by Mero rule (b), %lf->%lf\n",topNodeID,hTop,minH2);
//		}
//	}
//	
//	return false;
//}
		
void AStarDelay::ExtractPathToStart(graphState goalNode, std::vector<graphState> &thePath)
{
	SearchNode n;
	if (closedList.find(goalNode) != closedList.end())
	{
		n = closedList[goalNode];
	}
	else {
		n = openQueue.find(SearchNode(goalNode));
	}
	
	do {
		thePath.push_back(n.currNode);
		n = closedList[n.prevNode];
	} while (n.currNode != n.prevNode);
	//thePath.push_back(n.currNode);
}


void AStarDelay::OpenGLDraw(int)
{
	OpenGLDraw();
}

void AStarDelay::OpenGLDraw()
{
	//float r,gcost,b;
	double x,y,z;
	SearchNode sn;
	graphState nodeID;
	SearchNode topn;
	char buf[100];

	// draw nodes
	node_iterator ni = g->getNodeIter();
	for(node* n = g->nodeIterNext(ni); n; n = g->nodeIterNext(ni))
	{
		x = n->GetLabelF(kXCoordinate);
		y = n->GetLabelF(kYCoordinate);
		z = n->GetLabelF(kZCoordinate);

		nodeID = (graphState) n->GetNum();

		// if in closed
		NodeLookupTable::iterator hiter;
		if ((hiter = closedList.find(nodeID)) != closedList.end())
		{
			sn = hiter->second;
			glColor3f(1,0,0);  // red

			memset(buf,0,100);
			sprintf(buf,"%d [%d,%d,%d]",n->GetNum(), (int)sn.gCost, (int)(sn.fCost - sn.gCost), (int)sn.fCost);
		}
		// if in open
		else if(openQueue.IsIn(SearchNode(nodeID)))
		{
			glColor3f(0,0,1);  // blue
		}
		else if(fQueue.IsIn(SearchNode(nodeID)))
		{
			glColor3f(1,1,0);  // yellow
		}
		else if(delayQueue.IsIn(SearchNode(nodeID)))
		{
			glColor3f(0,1,1);
		}
		// neither in open nor closed, white
		else 
		{
			glColor3f(1,1,1);  // white
		}
		DrawSphere(x,y,z,0.025);

		// draw the text info, in black
		//DrawText(x,y,z+0.05,0,0,0,buf);
	}

// draw edges
//	edge_iterator ei = g->getEdgeIter();
//	for(edge* e = g->edgeIterNext(ei); e; e = g->edgeIterNext(ei))
//	{
//		DrawEdge(e->getFrom(), e->getTo(), e->GetWeight());
//	}
}

void AStarDelay::DrawText(double x, double y, double z, float r, float gg, float b, char* str)
{
	//glPushMatrix();
	// rotate ?

	glPushMatrix();
	glColor3f(r,gg,b);
	glTranslatef(x,y,z);
	glScalef(1.0/(20*120.0), 1.0/(20*120.0), 1);
	glRotatef(180, 0.0, 0.0, 1.0);
	glRotatef(180, 0.0, 1.0, 0.0);
	
	int i=0;
	while(str[i]) 
	{
		glutStrokeCharacter(GLUT_STROKE_ROMAN,str[i]);
		i++;
	}
	glPopMatrix();
}

void AStarDelay::DrawEdge(unsigned int from, unsigned int to, double weight)
{
	double x1,y1,z1;
	double x2,y2,z2;
	char buf[100] = {0};

	node* nfrom = g->GetNode(from);
	node* nto = g->GetNode(to);

	x1 = nfrom->GetLabelF(kXCoordinate);
	y1 = nfrom->GetLabelF(kYCoordinate);
	z1 = nfrom->GetLabelF(kZCoordinate);
	x2 = nto->GetLabelF(kXCoordinate);
	y2 = nto->GetLabelF(kYCoordinate);
	z2 = nto->GetLabelF(kZCoordinate);

	// draw line segment
	glBegin(GL_LINES);
	glColor3f(1,1,0); // yellow
	glVertex3f(x1,y1,z1);
	glVertex3f(x2,y2,z2);
	glEnd();

	// draw weight info
	sprintf(buf,"%ld",(long)weight);
	DrawText((x1+x2)/2, (y1+y2)/2, (z1+z2)/2 + 0.05, 1, 0, 0, buf); // in red
}
