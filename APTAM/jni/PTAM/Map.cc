// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015
#include "Map.h"
#include "MapPoint.h"

namespace APTAM {

Map::Map()
{
  Reset();
  bTrackingGood = false;
  pthread_mutex_init(&ptlock, NULL); //TODO destroy
}

void Map::LockMap()
{
	pthread_mutex_lock(&ptlock);
}

void Map::UnlockMap()
{
	pthread_mutex_unlock(&ptlock);
}

void Map::Reset()
{
  for(unsigned int i=0; i<vpPoints.size(); i++)
    delete vpPoints[i];
  vpPoints.clear();
  bGood = false;
  EmptyTrash();
}

void Map::MoveBadPointsToTrash()
{
  int nBad = 0;
  for(int i = vpPoints.size()-1; i>=0; i--)
    {
      if(vpPoints[i]->bBad)
	{
	  vpPointsTrash.push_back(vpPoints[i]);
	  vpPoints.erase(vpPoints.begin() + i);
	  nBad++;
	}
    };
}

void Map::EmptyTrash()
{
  for(unsigned int i=0; i<vpPointsTrash.size(); i++)
    delete vpPointsTrash[i];
  vpPointsTrash.clear();
}

}

